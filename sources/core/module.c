/* module.c - initialization, cleanup, parameters and other common stuff. */

/* ========================================================================
 * Copyright (C) 2012, KEDR development team
 * Authors:
 *      Eugene A. Shatokhin <spectre@ispras.ru>
 *      Andrey V. Tsyvarev  <tsyvarev@ispras.ru>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published
 * by the Free Software Foundation.
 ======================================================================== */

/* [NB] The following applies not only to this source file but rather to
 * all source files in this project. Unless specifically stated, a function
 * returning int returns 0 on success and a negative error code on failure.
 */

#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/err.h>
#include <linux/slab.h>
#include <linux/mutex.h>
#include <linux/debugfs.h>
#include <linux/list.h>
#include <linux/string.h>
#include <linux/spinlock.h>
#include <asm/uaccess.h>

#include <kedr/kedr_mem/core_api.h>
#include <kedr/kedr_mem/local_storage.h>
#include <kedr/kedr_mem/functions.h>

#include "config.h"
#include "core_impl.h"

#include "sections.h"
#include "module_ms_alloc.h"
#include "i13n.h"
#include "hooks.h"
#include "tid.h"
#include "util.h"
#include "resolve_ip.h"
#include "fh_impl.h"
#include "target.h"
/* ====================================================================== */

MODULE_AUTHOR("Eugene A. Shatokhin");
MODULE_LICENSE("GPL");
/* ====================================================================== */

/* Names of the modules to be processed ("target modules"). The names can be
 * separated with any number of commas and semicolons. Note that spaces are
 * not allowed as separators as the module loaded interprets them in a 
 * special way. 
 *
 * "*" or at least one target module should be specified.
 *
 * If "*" is used instead of the list of modules, our system will process 
 * all the modules that will load after it except the modules with the names
 * staring from "kedr_" and "test_". If it is needed to analyze such 
 * modules, their names should be listed explicitly.
 * 
 * '*' is interpreted this way only if it is the only character in the value
 * of "targets" (except separator characters that may be also present). If 
 * there are other characters in that string value, '*' is considered to be
 * part of the name of a module, which is usually not what you want. 
 * [NB] Glob-expressions (e.g. "iwl*i") are not supported. */
char *targets = "*";
module_param(targets, charp, S_IRUGO);

/* Path where the user-mode helper scripts are located. Normally, the user
 * would not change it, it is mainly for testing purposes. */
char *umh_dir = KEDR_UM_HELPER_PATH;
module_param(umh_dir, charp, S_IRUGO);

/* This parameter controls whether to track memory accesses that actually
 * read and/or modify data on stack. Namely, if this parameter is zero,
 * - the instructions of type E and M that refer to memory relative to %rsp
 * are not tracked;
 * - the memory events may also be filtered out in runtime if the
 * corresponding instructions access the stack only (even if not using %rsp-
 * based addressing).
 *
 * Note that PUSH/POP %reg instructions are currently not processed as
 * memory events even if this parameter is non-zero and so are the stack
 * accesses from PUSH/POP <expr> (but the normal rules apply to the access
 * via <expr> in case of these instructions). */
int process_stack_accesses = 0;
module_param(process_stack_accesses, int, S_IRUGO);

/* This parameter controls whether to report accesses to the user space
 * memory. If it is 0, such accesses will not be reported. */
int process_um_accesses = 0;
module_param(process_um_accesses, int, S_IRUGO);

/* This parameter controls sampling technique used when reporting memory
 * accesses made in the common blocks.
 *
 * "Sampling" means that only part of the memory accesses made in a region
 * of code is going to be reported. This allows to reduce the intensity of
 * the event stream as well as the size of an event trace without missing
 * too many races (hopefully).
 *
 * This is similar to the sampling technique used by ThreadSanitizer and
 * LiteRace:
 *	http://code.google.com/p/data-race-test/wiki/LiteRaceSampling
 *	http://www.cs.ucla.edu/~dlmarino/pubs/pldi09.pdf
 * Similar to ThreadSanitizer, common blocks are considered during sampling
 * rather than the whole functions as it is implemented in LiteRace.
 *
 * The more number of times a block of code is executed in a given thread,
 * the more events will be skipped when reporting memory accesses performed
 * in this block in that thread.
 *
 * sampling_rate == 0 means that the sampling is disabled. To enable it, set
 * 'sampling_rate' to 1, 2, ... or 31. The higher the value, the more
 * "aggressive" the sampling will be (the more events are to be skipped).
 *
 * [NB] This parameter does not affect reporting of memory accesses in
 * locked operations, I/O operations that access memory, function calls,
 * etc. Only the memory accesses from the common blocks are considered.
 *
 * Currently, it is not recommended to use sampling if more than several
 * hundreds of threads are going to execute in the target module
 * long enough simultaneously. One of the limiting factors is the mechanism
 * to obtain thread indexes (see tid.c). Note that everything should work
 * even if there are more threads but I cannot say how fast the execution
 * will be in this case. At least, the slow path in the mechanism that
 * obtains thread index for a thread ID will trigger more often. Not sure if
 * the performance degradation can be significant here. */
unsigned int sampling_rate = 0;
module_param(sampling_rate, uint, S_IRUGO);
/* ====================================================================== */

/* An structure that identifies an analysis session for the target module. 
 * A session starts when the target module is loaded but before our system
 * begins to instrument it. The session stops, when the target is about to 
 * unload and "target unload" event has been processed. 
 *
 * Unless specifically stated, all operations with the session object must
 * be performed with 'session_mutex' locked, except its initialization and
 * cleanup.
 * 
 * [NB] Initialize it before connecting to the kernel notification system.*/
struct kedr_session
{
	/* The list of the 'target objects' (see below). 
	 * If the particular targets have been specified for our system, the
	 * list contains the preallocated objects for these (no matter 
	 * whether the targets are loaded or not).
	 * 
	 * If processing of all modules has been requested (that is, our 
	 * system has been loaded with targets='*'), the session object 
	 * starts with an empty 'targets' list. New elements are added to it
	 * when the kernel modules are loaded and therefore become the 
	 * targets. Note that the elements remain here even after the 
	 * corresponding target modules have been unloaded and are reused if
	 * they are loaded again. */
	struct list_head targets;
	
	/* Non-zero if processing of all modules to be loaded has been 
	 * requested (parameter 'targets' is '*'), 0 otherwise. */
	int process_all;
	
	/* Number of the currently loaded target modules. */
	unsigned int num_loaded;
	
	/* Nonzero, if the system failed to start the session. The target
	 * modules will not be processed until the core of our system is 
	 * reloaded. */
	int is_broken;
};

/* The session object. */
struct kedr_session session;

/* A mutex to protect the data related to the analysis session and to the
 * target modules in particular. */
DEFINE_MUTEX(session_mutex);

/* A spinlock to protect operations with the list of target object while the
 * session is active. Not needed if the target is not active or if the list
 * is only being read with 'session_mutex' locked. */
static DEFINE_SPINLOCK(target_list_lock);
/* ====================================================================== */

/* Total number of blocks containing potential memory accesses and the
 * number of blocks skipped because of sampling, respectively.
 * These counters may be incremented and output without synchronization.
 * As they are only intended for gathering statistics and for debugging,
 * some inaccuracies due to races are acceptable. */
size_t blocks_total = 0;
size_t blocks_skipped = 0;

/* Files for these counters in debugfs. */
static struct dentry *blocks_total_file = NULL;
static struct dentry *blocks_skipped_file = NULL;
/* ====================================================================== */

static struct kedr_event_handlers *eh_default = NULL;

/* The current set of event handlers. If no set is registered, 'eh_current'
 * must be the address of the default set, i.e. eh_current == eh_default.
 * Except the initial assignment, all accesses to 'eh_current' pointer must
 * be protected with 'session_mutex'. This way, we make sure the instrumented
 * code will see the set of handlers in a consistent state.
 *
 * Note that calling the handlers from '*eh_current' is expected to be done
 * without locking 'session_mutex'. As long as the structure pointed to by
 * 'eh_current' stays unchanged since its registration till its
 * de-registration, this makes no harm. Only the changes in the pointer
 * itself must be protected. */
struct kedr_event_handlers *eh_current = NULL;

/* If nonzero, module load and unload notifications will be handled,
 * if 0, they will not. */
static int handle_module_notifications = 0;
/* ====================================================================== */

/* A directory for the core in debugfs. */
static struct dentry *debugfs_dir_dentry = NULL;
const char *debugfs_dir_name = KEDR_DEBUGFS_DIR;
/* ====================================================================== */

/* The pool of the IDs that are unique during the session with the target
 * module. */
static LIST_HEAD(id_pool);

/* Operations with the pool of IDs should be performed with this mutex
 * locked. */
static DEFINE_MUTEX(id_pool_mutex);

/* Creates a new ID and adds it to the pool. */
unsigned long
kedr_get_unique_id(void)
{
	struct list_head *item = NULL;

	if (mutex_lock_killable(&id_pool_mutex) != 0) {
		pr_warning(KEDR_MSG_PREFIX
		"kedr_get_unique_id(): failed to lock mutex\n");
		return 0;
	}

	item = kzalloc(sizeof(*item), GFP_KERNEL);
	if (item != NULL)
		list_add(item, &id_pool);

	mutex_unlock(&id_pool_mutex);
	return (unsigned long)item;
}
EXPORT_SYMBOL(kedr_get_unique_id);

static void
clear_id_pool(void)
{
	struct list_head *pos;
	struct list_head *tmp;

	if (mutex_lock_killable(&id_pool_mutex) != 0) {
		pr_warning(KEDR_MSG_PREFIX
		"clear_id_pool(): failed to lock mutex\n");
		return;
	}

	list_for_each_safe(pos, tmp, &id_pool) {
		list_del(pos);
		kfree(pos);
	}
	mutex_unlock(&id_pool_mutex);
}
/* ====================================================================== */

static int
session_active(void)
{
	return (session.num_loaded > 0);
}

static void
cleanup_session(void)
{
	struct kedr_target *t;
	struct kedr_target *tmp;

	BUG_ON(session_active());
	
	list_for_each_entry_safe(t, tmp, &session.targets, list) {
		BUG_ON(t->mod != NULL);
		BUG_ON(t->i13n != NULL);
		
		list_del(&t->list);
		kfree(t->name);
		kfree(t);
	}
}

/* Replace '-' with '_' in the name of the target to allow the user to 
 * specify the target names like "kvm-intel" or the like. */
static void
replace_dashes(char *str)
{
	int i = 0;
	while (str[i] != 0) {
		if (str[i] == '-')
			str[i] = '_';
		++i;
	}
}

/* Preallocate objects for the known targets. */
static int __init
add_target_object(size_t beg, size_t len)
{
	struct kedr_target *t;
	
	t = kzalloc(sizeof(*t), GFP_KERNEL);
	if (t == NULL) {
		pr_warning(KEDR_MSG_PREFIX 
		"Failed to create a target object: out of memory.\n");
		return -ENOMEM;
	}
			
	t->name = kstrndup(&targets[beg], len, GFP_KERNEL);
	if (t->name == NULL) {
		pr_warning(KEDR_MSG_PREFIX 
		"Failed to copy the name of the target: out of memory.\n");
		
		kfree(t);
		return -ENOMEM;
	}
	
	replace_dashes(t->name);
	
	/* No need for locking the spinlock, the session is not active. */
	list_add(&t->list, &session.targets);
	return 0;
}

/* Initializes the session object according to the value of "targets" 
 * parameter. Pre-creates the kedr_target objects if needed. */
static int __init
init_session(void)
{
	static const char *seps = ",;";
	int ret = -EINVAL;
	int targets_found = 0;
	
	size_t beg = 0;
	size_t end = strlen(targets);
	
	INIT_LIST_HEAD(&session.targets);
	session.process_all = 0;
	session.num_loaded = 0;
	session.is_broken = 0;
	
	beg += strspn(targets, seps);
	while (beg < end) {
		size_t len = strcspn(&targets[beg], seps);
		
		if (targets[beg] == '*' && len == 1) {
			session.process_all = 1;
		}
		else {
			ret = add_target_object(beg, len);
			if (ret != 0)
				goto fail;
			
			targets_found = 1;
		}
		beg += len + strspn(&targets[beg + len], seps);
	}
	
	if (!session.process_all && !targets_found) {
		pr_warning(KEDR_MSG_PREFIX
			"At least one target should be specified.\n");
		goto fail;
	}
	
	if (session.process_all && targets_found) {
		pr_warning(KEDR_MSG_PREFIX
"If '*' is used, it must be the only item in the list of targets.\n");
		goto fail;
	}

	return 0;

fail:
	cleanup_session();
	return ret;
}

/* These two functions looks for a target object corresponding to the module
 * with the given name or struct module. Return the object if found, NULL 
 * otherwise. 
 *
 * Should be called with 'session_mutex' locked. */
static struct kedr_target *
find_target_object_by_name(const char *name)
{
	struct kedr_target *pos = NULL;
	struct kedr_target *t = NULL;
	
	list_for_each_entry(pos, &session.targets, list) {
		if (strcmp(pos->name, name) == 0) {
			t = pos;
			break;
		}
	}
	return t;
}

static struct kedr_target *
find_target_object_by_mod(struct module *mod)
{
	struct kedr_target *pos = NULL;
	struct kedr_target *t = NULL;

	list_for_each_entry(pos, &session.targets, list) {
		if (pos->mod == mod) {
			t = pos;
			break;
		}
	}
	return t;
}

static struct kedr_target *
object_for_loaded_target(struct module *mod)
{
	struct kedr_target *t;
	unsigned long irq_flags;
	
	t = kzalloc(sizeof(*t), GFP_KERNEL);
	if (t == NULL) {
		pr_warning(KEDR_MSG_PREFIX 
		"Failed to create a target object: out of memory.\n");
		return NULL;
	}
			
	t->name = kstrdup(module_name(mod), GFP_KERNEL);
	if (t->name == NULL) {
		pr_warning(KEDR_MSG_PREFIX 
		"Failed to copy the name of the target: out of memory.\n");
		
		kfree(t);
		return NULL;
	}

	/* [NB] No need to replace '-' with '_' in the name of the target
	 * in this case: insmod/modprobe/... must have done that already. */

	spin_lock_irqsave(&target_list_lock, irq_flags);
	list_add(&t->list, &session.targets);
	spin_unlock_irqrestore(&target_list_lock, irq_flags);
	return t;
}

/* Nonzero of the name of the module does not start with "kedr_" or "test_",
 * 0 otherwise. */
static int
is_special_module(struct module *mod)
{
	static char pr1[] = "kedr_";
	static char pr2[] = "test_";
	
	if (strncmp(module_name(mod), &pr1[0], ARRAY_SIZE(pr1) - 1) == 0)
		return 1;
	
	if (strncmp(module_name(mod), &pr2[0], ARRAY_SIZE(pr2) - 1) == 0)
		return 1;
	
	return 0;
}

/* The function returns the target object for the given module.
 * NULL is returned if the module is not a target or if an error occurs. 
 * Should be called with 'session_mutex' locked. */
static struct kedr_target *
get_target_object(struct module *mod)
{
	struct kedr_target *t;
	
	/* First check if the target module is already known. Lookup by name
	 * because the module might have been unloaded and loaded again and
	 * might have a different struct module now. */
	t = find_target_object_by_name(module_name(mod));
	
	if (t == NULL && session.process_all && !is_special_module(mod))
		t = object_for_loaded_target(mod);
			
	if (t != NULL) {
		BUG_ON(t->mod != NULL);
		t->mod = mod;
	}
	return t;
}

int
kedr_for_each_loaded_target(int (*func)(struct kedr_target *, void *), 
	void *data)
{
	struct kedr_target *t;
	int ret = 0;
	
	list_for_each_entry(t, &session.targets, list) {
		if (t->mod != NULL && t->i13n != NULL) {
			ret = func(t, data);
			if (ret < 0) {
				/* error */
				goto out;
			}
			else if (ret > 0) { 
				/* no error, but no need to go further */
				ret = 0;
				goto out;
			}
		}
	}
out:
	return ret;
}
/* ====================================================================== */

/* "Provider" support */
/* A provider is a component that provides its functions to the core
 * (e.g. event handlers, allocators, other kinds of callbacks).
 * Currently, each such provider has a distinct role, see enum
 * kedr_provider_role below.
 * The core itself is a provider, the one used by default.
 *
 * The core increases the usage count for each provider with
 * try_module_get() for the time the instrumented target is in memory.
 * If it fails to "lock" one or more providers this way, it must not
 * instrument the target. If the instrumentation failed or has not been
 * performed yet ('i13n' is NULL), the providers must remain "unlocked" (at
 * least, their usage count set by our module should remain 0).
 *
 * Operations with the collection of providers (set, reset, get, put)
 * except its initialization should be performed with 'session_mutex' locked.
 * This way, these operations will be atomic w.r.t. the loading / unloading
 * of the target. */
enum kedr_provider_role
{
	/* Provides: event handlers */
	KEDR_PR_EVENT_HANDLERS = 0,

	/* Provides: alloc/free routines for local storage */
	KEDR_PR_LS_ALLOCATOR,

	/* Provides: hooks for the core */
	KEDR_PR_HOOKS,

	/* [NB] Add more roles here if necessary */

	/* The number of provider roles, keep this item last. */
	KEDR_PR_NUM_ROLES
};

static struct module *providers[KEDR_PR_NUM_ROLES];

/* Set the provider with the given role.
 * Must not be called if the session is already active. */
static void
set_provider(struct module *m, enum kedr_provider_role role)
{
	BUG_ON(m == NULL);
	BUG_ON(session_active());
	providers[role] = m;
}

/* Reset the provider with the given role to the default.
 * Must not be called if the session is already active. */
static void
reset_provider(enum kedr_provider_role role)
{
	BUG_ON(session_active());
	providers[role] = THIS_MODULE;
}

/* Try to increase usage count for each of the providers and therefore make
 * their modules unloadable. The function returns 0 if successful, an error
 * code otherwise. After the call to this function, the usage count is
 * incremented either for all of the providers (on success) or for none of
 * them (on failure).
 *
 * Note that our module (THIS_MODULE) will be unloadable from memory anyway.
 * Either no external provider has been registered so far and the default
 * one (our module) is used and processed by by try_module_get() in this
 * function. Or an external provider is set for at least one role
 * and that provider will be "locked". But it uses the API exported by our
 * module and therefore our module will not be unloadable too at least until
 * all providers are unregistered.
 *
 * For each successful call to this function, there should be a call to
 * providers_put() somewhere. */
static int
providers_get(void)
{
	int ret = 0;
	int i;
	int k;
	for (i = 0; i < KEDR_PR_NUM_ROLES; ++i) {
		if (try_module_get(providers[i]) == 0) {
			pr_err(KEDR_MSG_PREFIX
			"try_module_get() failed for the module \"%s\".\n",
			module_name(providers[i]));
			ret = -ENODEV;
			break;
		}
	}

	if (ret != 0) {
		/* Unlock the modules we might have locked before the
		 * failed one (#i). */
		 for (k = 0; k < i; ++k)
			module_put(providers[k]);
		 return ret;
	}
	return 0;
}

/* Unlock the providers (see module_put()). */
static void
providers_put(void)
{
	int i;
	for (i = 0; i < KEDR_PR_NUM_ROLES; ++i)
		module_put(providers[i]);
}
/* ====================================================================== */

static struct kedr_local_storage *
default_alloc_ls(struct kedr_ls_allocator *al)
{
	return (struct kedr_local_storage *)kzalloc(
		sizeof(struct kedr_local_storage), GFP_ATOMIC);
}

static void
default_free_ls(struct kedr_ls_allocator *al,
	struct kedr_local_storage *ls)
{
	kfree(ls);
	return;
}

static struct kedr_ls_allocator default_ls_allocator = {
	.owner = THIS_MODULE,
	.alloc_ls = default_alloc_ls,
	.free_ls  = default_free_ls,
};

struct kedr_ls_allocator *ls_allocator = &default_ls_allocator;
/* ====================================================================== */

static struct kedr_core_hooks default_hooks;
struct kedr_core_hooks *core_hooks = &default_hooks;
/* ====================================================================== */

/* Non-zero if some set of event handlers has already been registered,
 * 0 otherwise.
 * Must be called with 'session_mutex' locked. */
static int
event_handlers_registered(void)
{
	return (eh_current != eh_default);
}

int
kedr_register_event_handlers(struct kedr_event_handlers *eh)
{
	int ret = 0;
	BUG_ON(eh == NULL || eh->owner == NULL);

	if (mutex_lock_killable(&session_mutex) != 0) {
		pr_warning(KEDR_MSG_PREFIX
		"kedr_register_event_handlers(): failed to lock mutex\n");
		return -EINTR;
	}

	if (session_active()) {
		pr_warning(KEDR_MSG_PREFIX
	"Unable to register event handlers: analysis session is active.\n");
		ret = -EBUSY;
		goto out_unlock;
	}

	if (event_handlers_registered()) {
		pr_warning(KEDR_MSG_PREFIX
		"Attempt to register event handlers while some set of "
		"handlers is already registered\n");
		ret = -EINVAL;
		goto out_unlock;
	}

	eh_current = eh;
	set_provider(eh->owner, KEDR_PR_EVENT_HANDLERS);
	mutex_unlock(&session_mutex);
	return 0; /* success */

out_unlock:
	mutex_unlock(&session_mutex);
	return ret;
}
EXPORT_SYMBOL(kedr_register_event_handlers);

void
kedr_unregister_event_handlers(struct kedr_event_handlers *eh)
{
	BUG_ON(eh == NULL || eh->owner == NULL);

	/* [NB] mutex_lock_killable() is not suitable here because we must
	 * lock the mutex anyway. The handlers must be restored to their
	 * defaults even if their owner did something wrong.
	 * If this mutex_lock() call hangs because some other code has taken
	 * 'session_mutex' forever, it is our bug anyway and reboot will
	 * probably be necessary among other things. It seems safer to let
	 * it hang than to allow the owner of the event handlers go away
	 * while these handlers might be in use. */
	mutex_lock(&session_mutex);

	if (session_active()) {
		pr_warning(KEDR_MSG_PREFIX
"Attempt to unregister event handlers while the session is active\n");
		goto out;
	}

	if (eh != eh_current) {
		pr_warning(KEDR_MSG_PREFIX
		"Attempt to unregister event handlers that are not "
		"registered\n");
		goto out;
	}

out:
	/* No matter if there were errors detected above or not, restore the
	 * handlers to their defaults, it is safer anyway. */
	eh_current = eh_default;
	reset_provider(KEDR_PR_EVENT_HANDLERS);
	mutex_unlock(&session_mutex);
	return;
}
EXPORT_SYMBOL(kedr_unregister_event_handlers);

struct kedr_event_handlers *
kedr_get_event_handlers(void)
{
	WARN_ON_ONCE(!session_active());
	return eh_current;
}
EXPORT_SYMBOL(kedr_get_event_handlers);
/* ====================================================================== */

static int
session_start(void)
{
	int ret = providers_get();
	if (ret != 0)
		return ret;

	ret = kedr_fh_plugins_get();
	if (ret != 0) {
		providers_put();
		return ret;
	}
	
	kedr_eh_on_session_start();
	kedr_fh_on_session_start();
	
	blocks_total = 0;
	blocks_skipped = 0;
	return 0;
}

static void
session_end(void)
{
	kedr_fh_on_session_end();
	kedr_eh_on_session_end();
	
	kedr_fh_plugins_put();
	providers_put();
	clear_id_pool();
}

/* on_module_load() handles loading of the target module. This function is
 * called after the target module has been loaded into memory but before it
 * begins its initialization.
 *
 * Note that this function must be called with 'session_mutex' locked. */
static void
on_module_load(struct kedr_target *t) 
{
	int session_begins = (session.num_loaded == 0);
	
	if (session.is_broken)
		return;
		
	BUG_ON(t == NULL);
	BUG_ON(t->i13n != NULL);
	
	if (session_begins) {
		int ret = session_start();
		if (ret != 0) {
			pr_warning(KEDR_MSG_PREFIX
		"Failed to start the analysis session. Error code: %d\n",
				ret);
			session.is_broken = 1;
			return;
		}
	}
	
	/* If we failed to start the session, no targets will be processed
	 * until the core module is reloaded. If the session started 
	 * successfully but instrumentation of some of the target modules 
	 * has failed, these modules will not be analysed this time but 
	 * other targets (if they exist) will be. */
		
	++session.num_loaded;
	
	pr_info(KEDR_MSG_PREFIX
		"Target module \"%s\" has just loaded.\n",
		module_name(t->mod));
	
	t->i13n = kedr_i13n_process_module(t->mod);
	BUG_ON(t->i13n == NULL);
	if (IS_ERR(t->i13n)) {
		t->i13n = NULL;
		return;
	}

	/* First, report "target load" event, then allow the plugins to
	 * generate more events for this target if they need to. */
	kedr_eh_on_target_loaded(t->mod);
	kedr_fh_on_target_load(t->mod);
	return;
}

/* on_module_unload() handles unloading of the target module. This function
 * is called after the cleanup function of the latter has completed and the
 * module loader is about to unload that module.
 *
 * Note that this function must be called with 'session_mutex' locked.
 *
 * [NB] This function is called even if the initialization of the target
 * module fails. */
static void
on_module_unload(struct kedr_target *t)
{
	int session_ends = (session.num_loaded == 1);
	
	BUG_ON(t == NULL);
	BUG_ON(t->mod == NULL);
	
	if (session.is_broken)
		return;
	
	pr_info(KEDR_MSG_PREFIX
		"Target module \"%s\" is going to unload.\n",
		module_name(t->mod));

	/* If we failed to lock the providers in memory when the target had
	 * just loaded or failed to perform the instrumentation then, the
	 * target module worked unchanged and usage count of the providers
	 * was not modified. Nothing to clean up in this case. */
	if (t->i13n == NULL)
		goto out;

	/* The function handling plugins may generate events themselves, so
	 * make them do it before the event handling subsystem reports
	 * "target unload" event. */
	kedr_fh_on_target_unload(t->mod);
	kedr_eh_on_target_about_to_unload(t->mod);

	kedr_i13n_cleanup(t->i13n);
	t->i13n = NULL; /* prepare for the next instrumentation session */
	
out:
	t->mod = NULL;
	
	--session.num_loaded;
	if (session_ends)
		session_end();
}

/* A callback function to handle loading and unloading of a module. */
static int
detector_notifier_call(struct notifier_block *nb,
	unsigned long mod_state, void *vmod)
{
	struct module* mod = (struct module *)vmod;
	struct kedr_target *t = NULL;
	
	BUG_ON(mod == NULL);

	if (mutex_lock_killable(&session_mutex) != 0)
	{
		pr_warning(KEDR_MSG_PREFIX
		"detector_notifier_call(): failed to lock session_mutex\n");
		return 0;
	}

	if (!handle_module_notifications)
		goto out;

	/* handle changes in the module state */
	switch(mod_state)
	{
	case MODULE_STATE_COMING: /* the module has just loaded */
		t = get_target_object(mod);
		if (t == NULL)
			break;
		
		on_module_load(t);
		break;

	case MODULE_STATE_GOING: /* the module is going to unload */
		t = find_target_object_by_mod(mod);
		if (t == NULL)
			break;

		on_module_unload(t);
		break;
	}

out:
	mutex_unlock(&session_mutex);
	return 0;
}

/* A struct for watching for loading/unloading of modules.*/
struct notifier_block detector_nb = {
	.notifier_call = detector_notifier_call,
	.next = NULL,
	.priority = -1,
	/* Priority 0 would also do but a lower priority value is safer.
	 * Our handler should be called after ftrace does its job
	 * (the notifier registered by ftrace uses priority 0).
	 * ftrace seems to instrument the beginning of each function in the
	 * newly loaded modules for its own purposes.
	 * If our handler is called first, WARN_ON is triggered in ftrace.
	 * Everything seems to work afterwards but still the warning is
	 * annoying. I suppose it is better to just let ftrace do its
	 * work first and only then instrument the resulting code of
	 * the target module. */
};
/* ====================================================================== */

void
kedr_set_ls_allocator(struct kedr_ls_allocator *al)
{
	int ret = 0;

	/* Because we need to check if the session is active, we should
	 * lock 'session_mutex'.
	 * It is only allowed to change the allocator if the session is not
	 * active: different allocators can be incompatible with each other.
	 * If the local storage has been allocated by a given allocator,
	 * it must be freed by the same allocator. */
	ret = mutex_lock_killable(&session_mutex);
	if (ret != 0)
	{
		pr_warning(KEDR_MSG_PREFIX
		"kedr_set_ls_allocator(): failed to lock session_mutex\n");
		goto out;
	}

	if (session_active()) {
		pr_warning(KEDR_MSG_PREFIX
	"Failed to change local storage allocator: the session is active.\n");
		goto out_unlock;
	}

	if (al != NULL) {
		if (ls_allocator != &default_ls_allocator) {
			pr_warning(KEDR_MSG_PREFIX
	"Failed to set the local storage allocator while a custom allocator is active.\n");
			goto out_unlock;
		}
		ls_allocator = al;
		set_provider(al->owner, KEDR_PR_LS_ALLOCATOR);
	}
	else {
		ls_allocator = &default_ls_allocator;
		reset_provider(KEDR_PR_LS_ALLOCATOR);
	}

out_unlock:
	mutex_unlock(&session_mutex);
out:
	return;
}
EXPORT_SYMBOL(kedr_set_ls_allocator);

struct kedr_ls_allocator *
kedr_get_ls_allocator(void)
{
	return ls_allocator;
}
EXPORT_SYMBOL(kedr_get_ls_allocator);
/* ====================================================================== */

void
kedr_set_core_hooks(struct kedr_core_hooks *hooks)
{
	int ret = 0;
	ret = mutex_lock_killable(&session_mutex);
	if (ret != 0)
	{
		pr_warning(KEDR_MSG_PREFIX
		"kedr_set_core_hooks(): failed to lock session_mutex\n");
		goto out;
	}

	if (session_active()) {
		pr_warning(KEDR_MSG_PREFIX
	"Failed to change the core hooks while the session is active.\n");
		goto out_unlock;
	}

	if (hooks != NULL) {
		if (core_hooks != &default_hooks) {
			pr_warning(KEDR_MSG_PREFIX
	"Failed to set the core hooks while custom hooks are still active.\n");
			goto out_unlock;
		}
		core_hooks = hooks;
		set_provider(hooks->owner, KEDR_PR_HOOKS);
	}
	else {
		core_hooks = &default_hooks;
		reset_provider(KEDR_PR_HOOKS);
	}

out_unlock:
	mutex_unlock(&session_mutex);
out:
	return;
}
EXPORT_SYMBOL(kedr_set_core_hooks);
/* ====================================================================== */

int
kedr_fh_plugin_register(struct kedr_fh_plugin *fh)
{
	int ret = 0;

	BUG_ON(fh == NULL);
	ret = mutex_lock_killable(&session_mutex);
	if (ret != 0)
	{
		pr_warning(KEDR_MSG_PREFIX
	"kedr_fh_plugin_register(): failed to lock session_mutex\n");
		goto out;
	}

	if (session_active()) {
		pr_warning(KEDR_MSG_PREFIX
	"Failed to register the function handling plugin: the session is active.\n");
		ret = -EBUSY;
		goto out_unlock;
	}
	ret = kedr_fh_plugin_register_impl(fh);

out_unlock:
	mutex_unlock(&session_mutex);
out:
	return ret;
}
EXPORT_SYMBOL(kedr_fh_plugin_register);

void
kedr_fh_plugin_unregister(struct kedr_fh_plugin *fh)
{
	int ret = 0;

	BUG_ON(fh == NULL);
	ret = mutex_lock_killable(&session_mutex);
	if (ret != 0)
	{
		pr_warning(KEDR_MSG_PREFIX
	"kedr_fh_plugin_unregister(): failed to lock session_mutex\n");
		return;
	}

	if (session_active()) {
		pr_warning(KEDR_MSG_PREFIX
	"Failed to unregister the function handling plugin: the session is active.\n");
		goto out_unlock;
	}
	kedr_fh_plugin_unregister_impl(fh);

out_unlock:
	mutex_unlock(&session_mutex);
}
EXPORT_SYMBOL(kedr_fh_plugin_unregister);
/* ====================================================================== */

static struct kedr_i13n *
i13n_for_addr(unsigned long addr)
{
	struct kedr_i13n *i13n = NULL;
	struct kedr_target *t;
	unsigned long irq_flags;
	
	spin_lock_irqsave(&target_list_lock, irq_flags);
	list_for_each_entry(t, &session.targets, list) {
		if (t->mod == NULL)
			continue;
			
		if (kedr_is_text_address(addr, t->mod))
			i13n = t->i13n;
	}
	
	spin_unlock_irqrestore(&target_list_lock, irq_flags);
	return i13n;
}

struct kedr_func_info *
kedr_find_func_info(unsigned long addr)
{
	struct kedr_i13n *i13n;

	if (!session_active())
		return NULL;
	
	i13n = i13n_for_addr(addr);
	if (i13n == NULL)
		return NULL;

	return kedr_i13n_func_info_for_addr(i13n, addr);
}
EXPORT_SYMBOL(kedr_find_func_info);
/* ====================================================================== */

static void __init
init_providers(void)
{
	int i;
	for (i = 0; i < KEDR_PR_NUM_ROLES; ++i)
		providers[i] = THIS_MODULE;
}
/* ====================================================================== */

/* The list of the names of the loaded and instrumented target modules 
 * separated by newlines. Available via "loaded_targets" file in debugfs. 
 * The file will contain string "none" if no targets are currently loaded.*/
static char *loaded_targets = NULL;
static struct dentry *loaded_targets_file = NULL;

/* Allocates and populates 'loaded_targets' string. 
 * Should be called with 'session_mutex' locked. */
static int
update_loaded_targets_list(void)
{
	size_t len = 0;
	size_t pos = 0;
	struct kedr_target *t;
	
	kfree(loaded_targets);
	loaded_targets = NULL;
	
	/* 'session_mutex' is locked, so the target modules cannot come or 
	 * go, the list remains the same. No need to protect it further. */
	list_for_each_entry(t, &session.targets, list) {
		if (t->mod != NULL && t->i13n != NULL) {
			len += strlen(t->name) + 1; /* +1 for '\n' */
		}
	}
	
	if (len == 0)
		return 0;
	
	loaded_targets = kzalloc(len + 1, GFP_KERNEL);
	if (loaded_targets == NULL)
		return -ENOMEM;
	
	list_for_each_entry(t, &session.targets, list) {
		size_t cur_len;
		if (t->mod == NULL || t->i13n == NULL)
			continue;
		
		cur_len = strlen(t->name);
		BUG_ON(pos + cur_len + 1 > len);
		
		strncpy(&loaded_targets[pos], t->name, cur_len);
		pos += cur_len;
		loaded_targets[pos] = '\n';
		++pos;
	}
	return 0;	
}

/* File: "loaded_targets", read-only */
static int 
loaded_targets_open(struct inode *inode, struct file *filp)
{
	int ret = 0;
	
	if (mutex_lock_killable(&session_mutex) != 0)
	{
		pr_warning(KEDR_MSG_PREFIX "loaded_targets_read(): "
			"got a signal while trying to acquire a mutex.\n");
		return -EINTR;
	}
	
	ret = update_loaded_targets_list();
	if (ret != 0)
		goto fail;
	
	mutex_unlock(&session_mutex);
	return nonseekable_open(inode, filp);

fail:
	mutex_unlock(&session_mutex);
	return ret;
}

static int
loaded_targets_release(struct inode *inode, struct file *filp)
{
	return 0;
}

static ssize_t 
loaded_targets_read(struct file *filp, char __user *buf, size_t count,
	loff_t *f_pos)
{
	ssize_t ret = 0;
	loff_t pos = *f_pos;
	size_t data_len;
	static char none_str[] = "none\n";
	char *out_buf = loaded_targets;
	
	if (mutex_lock_killable(&session_mutex) != 0)
	{
		pr_warning(KEDR_MSG_PREFIX "loaded_targets_read(): "
			"got a signal while trying to acquire a mutex.\n");
		return -EINTR;
	}
	
	if (out_buf == NULL)
		out_buf = none_str;
	
	data_len = strlen(out_buf);
	
	/* Reading outside of the data buffer is not allowed */
	if ((pos < 0) || (pos > data_len)) {
		ret = -EINVAL;
		goto out;
	}

	/* EOF reached or 0 bytes requested */
	if ((count == 0) || (pos == data_len)) {
		ret = 0; 
		goto out;
	}

	if (pos + count > data_len) 
		count = data_len - pos;
	if (copy_to_user(buf, &out_buf[pos], count) != 0) {
		ret = -EFAULT;
		goto out;
	}

	mutex_unlock(&session_mutex);

	*f_pos += count;
	return count;

out:
	mutex_unlock(&session_mutex);
	return ret;
}

static const struct file_operations loaded_targets_ops = {
	.owner = THIS_MODULE,
	.open = loaded_targets_open,
	.release = loaded_targets_release,
	.read = loaded_targets_read,
};
/* ====================================================================== */

/* Initialize the default handlers, callbacks, hooks, etc., before
 * registering with the notification system. */
static int __init
init_defaults(void)
{
	eh_default = kzalloc(sizeof(*eh_default), GFP_KERNEL);
	if (eh_default == NULL)
		return -ENOMEM;

	eh_default->owner = THIS_MODULE;
	eh_current = eh_default;

	memset(&default_hooks, 0, sizeof(default_hooks));
	default_hooks.owner = THIS_MODULE;

	init_providers();
	return 0;
}

static void
remove_debugfs_files(void)
{
	if (blocks_total_file != NULL)
		debugfs_remove(blocks_total_file);
	if (blocks_skipped_file != NULL)
		debugfs_remove(blocks_skipped_file);
	if (loaded_targets_file != NULL)
		debugfs_remove(loaded_targets_file);
}

static int __init
create_debugfs_files(void)
{
	int ret = 0;
	const char *name = "ERROR";

	BUG_ON(debugfs_dir_dentry == NULL);

	blocks_total_file = debugfs_create_size_t("blocks_total", S_IRUGO,
		debugfs_dir_dentry, &blocks_total);
	if (blocks_total_file == NULL) {
		name = "blocks_total";
		ret = -ENOMEM;
		goto out;
	}

	blocks_skipped_file = debugfs_create_size_t("blocks_skipped",
		S_IRUGO, debugfs_dir_dentry, &blocks_skipped);
	if (blocks_skipped_file == NULL) {
		name = "blocks_skipped";
		ret = -ENOMEM;
		goto out;
	}
	
	loaded_targets_file = debugfs_create_file("loaded_targets", S_IRUGO, 
		debugfs_dir_dentry, NULL, &loaded_targets_ops);
	if (loaded_targets_file == NULL) {
		name = "loaded_targets";
		ret = -ENOMEM;
		goto out;
	}
	
	return 0;
out:
	pr_warning(KEDR_MSG_PREFIX
		"Failed to create a file in debugfs (\"%s\").\n",
		name);
	remove_debugfs_files();
	return ret;
}

/* Must be called with 'module_mutex' locked. As no target module may come 
 * or go when this mutex is locked, no need to additionally protect the 
 * session object here. */
static int __init
some_targets_loaded(void)
{
	struct kedr_target *t;	
	BUG_ON(session_active()); /* Must never trigger but ... */
	
	list_for_each_entry(t, &session.targets, list) {
		if (find_module(t->name) != NULL)
			return 1;
	}
	return 0;
}

static int __init
core_init_module(void)
{
	int ret = 0;

	pr_info(KEDR_MSG_PREFIX
		"Initializing (" KEDR_KS_PACKAGE_NAME " version "
		KEDR_KS_PACKAGE_VERSION ")\n");
	
	ret = init_session();
	if (ret != 0)
		return ret;

	if (sampling_rate > 31) {
		pr_warning(KEDR_MSG_PREFIX
		"Parameter \"sampling_rate\" has an invalid value (%u). "
		"Must be 0 .. 31.\n",
			sampling_rate);
		ret = -EINVAL;
		goto out_cleanup_session;
	}

	ret = init_defaults();
	if (ret != 0) {
		pr_warning(KEDR_MSG_PREFIX
			"Initialization of the defaults failed.\n");
		goto out_cleanup_session;
	}

	/* Create the directory for the core in debugfs */
	debugfs_dir_dentry = debugfs_create_dir(debugfs_dir_name, NULL);
	if (IS_ERR(debugfs_dir_dentry)) {
		pr_warning(KEDR_MSG_PREFIX "debugfs is not supported\n");
		ret = -ENODEV;
		goto out_free_eh;
	}

	if (debugfs_dir_dentry == NULL) {
		pr_warning(KEDR_MSG_PREFIX
			"failed to create a directory in debugfs\n");
		ret = -EINVAL;
		goto out_free_eh;
	}

	ret = kedr_init_resolve_ip(debugfs_dir_dentry);
	if (ret != 0)
		goto out_rmdir;

	ret = create_debugfs_files();
	if (ret != 0)
		goto out_cleanup_resolve_ip;

	ret = kedr_init_section_subsystem(debugfs_dir_dentry);
	if (ret != 0)
		goto out_remove_files;

	ret = kedr_init_module_ms_alloc();
	if (ret != 0)
		goto out_cleanup_sections;

	ret = kedr_init_tid_sampling();
	if (ret != 0)
		goto out_cleanup_alloc;

	/* [NB] If something else needs to be initialized, do it before
	 * registering our callbacks with the notification system.
	 * Do not forget to re-check labels in the error path after that. */

	/* some_targets_loaded() requires 'module_mutex' to be locked. */
	ret = mutex_lock_killable(&module_mutex);
	if (ret != 0)
	{
		pr_warning(KEDR_MSG_PREFIX
			"Failed to lock module_mutex\n");
		goto out_cleanup_tid;
	}

	ret = register_module_notifier(&detector_nb);
	if (ret < 0) {
		pr_warning(KEDR_MSG_PREFIX
			"register_module_notifier() failed with error %d\n",
			ret);
		goto out_unlock;
	}

	/* Check if one or more targets are already loaded. */
	if (some_targets_loaded())
	{
		pr_warning(KEDR_MSG_PREFIX
"One or more target modules are already loaded. Processing of already loaded target modules is not supported\n");

		ret = -EEXIST;
		goto out_unreg_notifier;
	}

	ret = mutex_lock_killable(&session_mutex);
	if (ret != 0)
	{
		pr_warning(KEDR_MSG_PREFIX
			"init(): failed to lock session_mutex\n");
		goto out_unreg_notifier;
	}

	handle_module_notifications = 1;
	mutex_unlock(&session_mutex);

	mutex_unlock(&module_mutex);

/* From now on, our module will be notified when the target module
 * is loaded or have finished cleaning-up and is just about to unload. */
	return 0; /* success */

out_unreg_notifier:
	unregister_module_notifier(&detector_nb);

out_unlock:
	mutex_unlock(&module_mutex);

out_cleanup_tid:
	kedr_cleanup_tid_sampling();

out_cleanup_alloc:
	kedr_cleanup_module_ms_alloc();

out_cleanup_sections:
	kedr_cleanup_section_subsystem();

out_remove_files:
	remove_debugfs_files();

out_cleanup_resolve_ip:
	kedr_cleanup_resolve_ip();

out_rmdir:
	debugfs_remove(debugfs_dir_dentry);

out_free_eh:
	kfree(eh_default);

out_cleanup_session:
	cleanup_session();
	kfree(loaded_targets);
	return ret;
}

static void __exit
core_exit_module(void)
{
	/* [NB] Unregister notifications before cleaning up the rest. */
	unregister_module_notifier(&detector_nb);

	kedr_cleanup_tid_sampling();
	kedr_cleanup_module_ms_alloc();
	kedr_cleanup_section_subsystem();

	remove_debugfs_files();
	kedr_cleanup_resolve_ip();
	debugfs_remove(debugfs_dir_dentry);
	kfree(eh_default);
	
	cleanup_session();
	kfree(loaded_targets);

	WARN_ON(!list_empty(&id_pool));
	return;
}

module_init(core_init_module);
module_exit(core_exit_module);
/* ====================================================================== */
