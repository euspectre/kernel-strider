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

#include <kedr/kedr_mem/core_api.h>
#include <kedr/kedr_mem/local_storage.h>
#include <kedr/kedr_mem/functions.h>

#include "config.h"
#include "core_impl.h"

#include "sections.h"
#include "module_ms_alloc.h"
#include "i13n.h"
#include "hooks.h"
/* ====================================================================== */

MODULE_AUTHOR("Eugene A. Shatokhin");
MODULE_LICENSE("GPL");
/* ====================================================================== */

/* Name of the module to analyze. An empty name will match no module */
char *target_name = "";
module_param(target_name, charp, S_IRUGO);

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
 * based addressing). [TODO]
 * 
 * Note that PUSH/POP %reg instructions are currently not processed as 
 * memory events even if this parameter is non-zero and so are the stack 
 * accesses from PUSH/POP <expr> (but the normal rules apply to the access
 * via <expr> in case of these instructions). */
int process_stack_accesses = 0;
module_param(process_stack_accesses, int, S_IRUGO);
/* ====================================================================== */

static struct kedr_event_handlers *eh_default = NULL;

/* The current set of event handlers. If no set is registered, 'eh_current' 
 * must be the address of the default set, i.e. eh_current == eh_default. 
 * Except the initial assignment, all accesses to 'eh_current' pointer must
 * be protected with 'target_mutex'. This way, we make sure the instrumented 
 * code will see the set of handlers in a consistent state. 
 * 
 * Note that calling the handlers from '*eh_current' is expected to be done 
 * without locking 'target_mutex'. As long as the structure pointed to by
 * 'eh_current' stays unchanged since its registration till its 
 * de-registration, this makes no harm. Only the changes in the pointer 
 * itself must be protected. */
struct kedr_event_handlers *eh_current = NULL;

/* The module being analyzed. NULL if the module is not currently loaded. 
 * The accesses to this variable must be protected with 'target_mutex'. */
static struct module *target_module = NULL;

/* If nonzero, module load and unload notifications will be handled,
 * if 0, they will not. */
static int handle_module_notifications = 0;

/* A mutex to protect the data related to the target module. */
static DEFINE_MUTEX(target_mutex);
/* ====================================================================== */

/* A directory for the core in debugfs. */
static struct dentry *debugfs_dir_dentry = NULL;
const char *debugfs_dir_name = KEDR_DEBUGFS_DIR;
/* ====================================================================== */

/* The instrumentation object. NULL if the instrumentation failed or was not
 * performed. */
static struct kedr_i13n *i13n = NULL;
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
 * except its initialization should be performed with 'target_mutex' locked. 
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
	
	/* Provides: function call handlers */
	KEDR_PR_FUNC_HANDLERS,
	
	/* [NB] Add more roles here if necessary */
	
	/* The number of provider roles, keep this item last. */
	KEDR_PR_NUM_ROLES
};

static struct module *providers[KEDR_PR_NUM_ROLES];

/* Set the provider with the given role. 
 * Must not be called if the target module has already been instrumented. 
 * As this function is called with 'target_mutex' locked, it can either see
 * the target completely instrumented (i13n != NULL) and the providers 
 * already "locked" in memory or it can see the providers unlocked. Only in
 * the latter case, it is allowed to use this function. */
static void 
set_provider(struct module *m, enum kedr_provider_role role)
{
	BUG_ON(m == NULL);
	BUG_ON(i13n != NULL);
	providers[role] = m;
}

/* Reset the provider with the given role to the default. 
 * Must not be called if the target module has already been instrumented. */
static void 
reset_provider(enum kedr_provider_role role)
{
	BUG_ON(i13n != NULL);
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
		/* Unlock the modules we might have been locked before the
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

static struct kedr_function_handlers default_function_handlers = {
	.owner = THIS_MODULE,
	.fill_call_info = NULL,
};

struct kedr_function_handlers *function_handlers = 
	&default_function_handlers;
/* ====================================================================== */

/* Non-zero if some set of event handlers has already been registered, 
 * 0 otherwise. 
 * Must be called with 'target_mutex' locked. */
static int
event_handlers_registered(void)
{
	return (eh_current != eh_default);
}

static int 
target_module_loaded(void)
{
	return (target_module != NULL);
}

int 
kedr_register_event_handlers(struct kedr_event_handlers *eh)
{
	int ret = 0;
	BUG_ON(eh == NULL || eh->owner == NULL);
	
	if (mutex_lock_killable(&target_mutex) != 0) {
		pr_warning(KEDR_MSG_PREFIX 
		"kedr_register_event_handlers(): failed to lock mutex\n");
		return -EINTR;
	}
	
	if (target_module_loaded()) {
		pr_warning(KEDR_MSG_PREFIX 
		"Unable to register event handlers: target module is "
		"loaded\n");
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
	mutex_unlock(&target_mutex);
	return 0; /* success */

out_unlock:
	mutex_unlock(&target_mutex);
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
	 * 'target_mutex' forever, it is our bug anyway and reboot will 
	 * probably be necessary among other things. It seems safer to let
	 * it hang than to allow the owner of the event handlers go away
	 * while these handlers might be in use. */
	mutex_lock(&target_mutex);
	
	if (target_module_loaded()) {
		pr_warning(KEDR_MSG_PREFIX 
		"Attempt to unregister event handlers while the target "
		"module is loaded\n");
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
	mutex_unlock(&target_mutex);
	return;
}
EXPORT_SYMBOL(kedr_unregister_event_handlers);

struct kedr_event_handlers *
kedr_get_event_handlers(void)
{
	WARN_ON_ONCE(!target_module_loaded());
	return eh_current;
}
EXPORT_SYMBOL(kedr_get_event_handlers);
/* ====================================================================== */

/* Module filter.
 * Should return nonzero if the core should watch for the module with the
 * specified name. We are interested in analyzing only the module with that 
 * name. */
static int 
filter_module(const char *module_name)
{
	return strcmp(module_name, target_name) == 0;
}

/* on_module_load() handles loading of the target module. This function is
 * called after the target module has been loaded into memory but before it
 * begins its initialization.
 *
 * Note that this function must be called with 'target_mutex' locked. */
static void 
on_module_load(struct module *mod)
{
	BUG_ON(i13n != NULL);
	
	pr_info(KEDR_MSG_PREFIX
		"Target module \"%s\" has just loaded.\n",
		module_name(mod));
	
	if (providers_get() != 0) {
		/* If we failed to lock the providers in memory, we 
		 * should not instrument or otherwise affect the target. */
		return;
	}

	i13n = kedr_i13n_process_module(mod);
	BUG_ON(i13n == NULL);
	if (IS_ERR(i13n)) {
		pr_warning(KEDR_MSG_PREFIX 
		"Failed to instrument module \"%s\". Error code: %d\n",
			module_name(mod), (int)PTR_ERR(i13n));
		i13n = NULL;
		
		/* Instrumentation failed, no need to keep the providers in
		 * memory in this case. The target module will run 
		 * unmodified anyway. */
		providers_put();
		return;
	}
	
	/* Call the event handler, if set. */
	if (eh_current->on_target_loaded != NULL)
		eh_current->on_target_loaded(eh_current, mod);
	return;
}

/* on_module_unload() handles unloading of the target module. This function 
 * is called after the cleanup function of the latter has completed and the
 * module loader is about to unload that module.
 *
 * Note that this function must be called with 'target_mutex' locked.
 *
 * [NB] This function is called even if the initialization of the target
 * module fails. */
static void 
on_module_unload(struct module *mod)
{
	pr_info(KEDR_MSG_PREFIX
		"Target module \"%s\" is going to unload.\n",
		module_name(mod));
	
	/* If we failed to lock the providers in memory when the target had
	 * just loaded or failed to perform the instrumentation then, the 
	 * target module worked unchanged and usage count of the providers
	 * was not modified. Nothing to clean up in this case. */
	if (i13n == NULL)
		return;
	
	/* Call the event handler, if set. */
	if (eh_current->on_target_about_to_unload != NULL)
		eh_current->on_target_about_to_unload(eh_current, 
			mod);
	
	kedr_i13n_cleanup(i13n);
	i13n = NULL; /* prepare for the next instrumentation session */
	providers_put();
}

/* A callback function to handle loading and unloading of a module. 
 * Sets 'target_module' pointer among other things. */
static int 
detector_notifier_call(struct notifier_block *nb,
	unsigned long mod_state, void *vmod)
{
	struct module* mod = (struct module *)vmod;
	BUG_ON(mod == NULL);
    
	if (mutex_lock_killable(&target_mutex) != 0)
	{
		pr_warning(KEDR_MSG_PREFIX
		"detector_notifier_call(): failed to lock target_mutex\n");
		return 0;
	}
    
	if (!handle_module_notifications)
		goto out;
	
	/* handle changes in the module state */
	switch(mod_state)
	{
	case MODULE_STATE_COMING: /* the module has just loaded */
		if(!filter_module(module_name(mod))) 
			break;

		BUG_ON(target_module_loaded());
		target_module = mod;
		on_module_load(mod);
		break;

	case MODULE_STATE_GOING: /* the module is going to unload */
		/* if the target module has already been unloaded,
		 * target_module is NULL, so (mod != target_module) 
		 * will be true. */
		if(mod != target_module) 
			break;

		on_module_unload(mod);
		target_module = NULL;
	}

out:
	mutex_unlock(&target_mutex);
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
	
	/* Because we need to check 'target_module' and update the allocator
	 * atomically w.r.t. the loading of the target module, we should
	 * lock 'target_mutex'. 
	 * It is only allowed to change the allocator if the target module
	 * is not loaded: different allocators can be incompatible with 
	 * each other. If the local storage has been allocated by a given 
	 * allocator, it must be freed by the same allocator. */
	ret = mutex_lock_killable(&target_mutex);
	if (ret != 0)
	{
		pr_warning(KEDR_MSG_PREFIX
		"kedr_set_ls_allocator(): failed to lock target_mutex\n");
		goto out;
	}

	if (target_module_loaded()) {
		pr_warning(KEDR_MSG_PREFIX
	"Attempt to change local storage allocator while the target "
	"is loaded. The allocator will not be changed.\n");
		goto out_unlock;
	}
	
	if (al != NULL) {
		if (ls_allocator != &default_ls_allocator) {
			pr_warning(KEDR_MSG_PREFIX
	"Attempt to set the local storage allocator while a custom "
	"allocator is still active. The allocator will not be changed.\n");
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
	mutex_unlock(&target_mutex);
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
	ret = mutex_lock_killable(&target_mutex);
	if (ret != 0)
	{
		pr_warning(KEDR_MSG_PREFIX
		"kedr_set_core_hooks(): failed to lock target_mutex\n");
		goto out;
	}

	if (target_module_loaded()) {
		pr_warning(KEDR_MSG_PREFIX
	"Attempt to change the core hooks while the target is loaded. "
	"The hooks will not be changed.\n");
		goto out_unlock;
	}
	
	if (hooks != NULL) {
		if (core_hooks != &default_hooks) {
			pr_warning(KEDR_MSG_PREFIX
	"Attempt to set the core hooks while custom hooks are still "
	"active. The hooks will not be changed.\n");
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
	mutex_unlock(&target_mutex);
out:
	return;
}
EXPORT_SYMBOL(kedr_set_core_hooks);
/* ====================================================================== */

void
kedr_set_function_handlers(struct kedr_function_handlers *fh)
{
	int ret = 0;
	ret = mutex_lock_killable(&target_mutex);
	if (ret != 0)
	{
		pr_warning(KEDR_MSG_PREFIX
	"kedr_set_function_handlers(): failed to lock target_mutex\n");
		goto out;
	}

	if (target_module_loaded()) {
		pr_warning(KEDR_MSG_PREFIX
			"Attempt to change the function handlers "
			"while the target is loaded. "
			"The handlers will not be changed.\n");
		goto out_unlock;
	}
	
	if (fh != NULL) {
		if (function_handlers != &default_function_handlers) {
			pr_warning(KEDR_MSG_PREFIX
	"Attempt to set the function handlers while custom ones are still "
	"active. The handlers will not be changed.\n");
			goto out_unlock;
		}
		function_handlers = fh;
		set_provider(fh->owner, KEDR_PR_FUNC_HANDLERS);
	}
	else {
		function_handlers = &default_function_handlers;
		reset_provider(KEDR_PR_FUNC_HANDLERS);
	}

out_unlock:	
	mutex_unlock(&target_mutex);
out:
	return;
}
EXPORT_SYMBOL(kedr_set_function_handlers);
/* ====================================================================== */

static void __init
init_providers(void)
{
	int i;
	for (i = 0; i < KEDR_PR_NUM_ROLES; ++i)
		providers[i] = THIS_MODULE;
}
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

static int __init
core_init_module(void)
{
	int ret = 0;
	
	pr_info(KEDR_MSG_PREFIX 
	"Initializing (" KEDR_PACKAGE_NAME " version " KEDR_PACKAGE_VERSION 
	")\n");
	
	if (target_name[0] == '\0') {
		pr_warning(KEDR_MSG_PREFIX 
			"Parameter \"target_name\" must not be empty.\n");
		return -EINVAL;
	}
	
	ret = init_defaults();
	if (ret != 0) {
		pr_warning(KEDR_MSG_PREFIX 
			"Initialization of defaults failed.\n");
		return ret;
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
	
	ret = kedr_init_section_subsystem(debugfs_dir_dentry);
	if (ret != 0)
		goto out_rmdir;
	
	ret = kedr_init_module_ms_alloc();
	if (ret != 0)
		goto out_cleanup_sections;
	
	/* [NB] If something else needs to be initialized, do it before 
	 * registering our callbacks with the notification system.
	 * Do not forget to re-check labels in the error path after that. */
	
	/* find_module() requires 'module_mutex' to be locked. */
	ret = mutex_lock_killable(&module_mutex);
	if (ret != 0)
	{
		pr_warning(KEDR_MSG_PREFIX 
			"Failed to lock module_mutex\n");
		goto out_cleanup_alloc;
	}
    
	ret = register_module_notifier(&detector_nb);
	if (ret < 0) {
		pr_warning(KEDR_MSG_PREFIX 
			"register_module_notifier() failed with error %d\n",
			ret);
		goto out_unlock;
	}
    
	/* Check if the target is already loaded */
	if (find_module(target_name) != NULL)
	{
		pr_warning(KEDR_MSG_PREFIX
		"Target module \"%s\" is already loaded. Processing of "
		"already loaded target modules is not supported\n",
		target_name);

		ret = -EEXIST;
		goto out_unreg_notifier;
	}
    
	ret = mutex_lock_killable(&target_mutex);
	if (ret != 0)
	{
		pr_warning(KEDR_MSG_PREFIX
			"init(): failed to lock target_mutex\n");
		goto out_unreg_notifier;
	}

	handle_module_notifications = 1;
	mutex_unlock(&target_mutex);

	mutex_unlock(&module_mutex);
        
/* From now on, our module will be notified when the target module
 * is loaded or have finished cleaning-up and is just about to unload. */
	return 0; /* success */

out_unreg_notifier:
	unregister_module_notifier(&detector_nb);

out_unlock:
	mutex_unlock(&module_mutex);

out_cleanup_alloc:
	kedr_cleanup_module_ms_alloc();
	
out_cleanup_sections:
	kedr_cleanup_section_subsystem();

out_rmdir:
	debugfs_remove(debugfs_dir_dentry);

out_free_eh:
	kfree(eh_default);
	return ret;
}

static void __exit
core_exit_module(void)
{
	pr_info(KEDR_MSG_PREFIX "Cleaning up\n");
	
	/* [NB] Unregister notifications before cleaning up the rest. */
	unregister_module_notifier(&detector_nb);
	
	kedr_cleanup_module_ms_alloc();
	kedr_cleanup_section_subsystem();
	
	// TODO: more cleanup here
	
	debugfs_remove(debugfs_dir_dentry);
	kfree(eh_default);
	return;
}

module_init(core_init_module);
module_exit(core_exit_module);
/* ================================================================ */
