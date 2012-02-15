/* A module to test "Sections" subsystem. 
 * [NB] This module uses the load/unload notifications to take control and
 * obtain the information about ELF sections before the target module begins
 * its initialization. This is because we need ".init*" sections to be still
 * present in memory ("Sections" subsystem performs sanity checks on the
 * found sections). */

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

#include <linux/kernel.h> 
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/init.h>
#include <linux/errno.h>
#include <linux/err.h>
#include <linux/debugfs.h>
#include <linux/list.h>
#include <linux/slab.h>
#include <linux/string.h>

#include "config.h"
#include "core_impl.h"

#include "debug_util.h"
#include "sections.h"
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

/* "test_failed" - test result, 0 - passed, any other value - failed */
int test_failed = 1; /* failed by default */
module_param(test_failed, int, S_IRUGO); 
/* [NB] If our module does not receive notifications for some reason or 
 * processes them incorrectly, 'test_failed' will remain 1 and will 
 * therefore indicate that something went wrong. */
/* ====================================================================== */

/* The module being analyzed. NULL if the module is not currently loaded. 
 * The accesses to this variable must be protected with 'target_mutex'. */
static struct module *target_module = NULL;

/* If nonzero, module load and unload notifications will be handled,
 * if 0, they will not. */
static int handle_module_notifications = 0;

/* A mutex to protect the data related to the target module. */
static DEFINE_MUTEX(target_mutex);

/* A directory for the core in debugfs. */
static struct dentry *debugfs_dir_dentry = NULL;
const char *debugfs_dir_name = KEDR_DEBUGFS_DIR;
/* ====================================================================== */

/* Find the target module, collect the data about its ELF sections and 
 * output the data to the debug output channel. */
static int 
do_collect_data(void)
{
	int ret = 0;
	struct kedr_section *sec;
	char one_char[1]; /* for the 1st call to snprintf */
	char *buf = NULL;
	const char *fmt = "%s 0x%lx\n";
	int len;
	LIST_HEAD(sections);
	
	ret = kedr_get_sections(target_module, &sections);
	if (ret != 0) {
		pr_warning(KEDR_MSG_PREFIX
	"Failed to obtain names and addresses of the target's sections.\n");
		goto out;
	}
	
	list_for_each_entry(sec, &sections, list) {
		len = snprintf(&one_char[0], 1, fmt, sec->name, sec->addr);
		buf = (char *)kzalloc(len + 1, GFP_KERNEL);
		if (buf == NULL) {
			pr_err(KEDR_MSG_PREFIX "do_collect_data(): "
		"not enough memory to prepare a message of size %d\n",
				len);
			ret = -ENOMEM;
			goto out;
		}
		snprintf(buf, len + 1, fmt, sec->name, sec->addr);
		debug_util_print_string(buf);
		kfree(buf);
	}
out:
	/* kedr_release_sections() will empty the list and destroy its
	 * elements. */
	kedr_release_sections(&sections);
	return ret;
}
/* ====================================================================== */

static int 
target_module_loaded(void)
{
	return (target_module != NULL);
}

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
	int ret = 0;
	pr_info(KEDR_MSG_PREFIX
		"Target module \"%s\" has just loaded. \n",
		module_name(mod));
	
	ret = do_collect_data();
	if (ret != 0) {
		pr_warning(KEDR_MSG_PREFIX
	"Failed to obtain information about ELF sections in \"%s\". "
	"Error code: %d\n",
			module_name(mod), ret);
		return;
	}
	
	test_failed = 0;
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
	/* Nothing to do here */
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

/* A struct for watching for loading/unloading of modules. */
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

static void __exit
test_cleanup_module(void)
{
	/* [NB] Unregister notifications before cleaning up the rest. */
	unregister_module_notifier(&detector_nb);
	kedr_cleanup_section_subsystem();
	debug_util_fini();
	debugfs_remove(debugfs_dir_dentry);
	return;
}

static int __init
test_init_module(void)
{
	int ret = 0;
	debugfs_dir_dentry = debugfs_create_dir(debugfs_dir_name, NULL);
	if (IS_ERR(debugfs_dir_dentry)) {
		pr_warning(KEDR_MSG_PREFIX "debugfs is not supported\n");
		ret = -ENODEV;
		goto out;
	}

	if (debugfs_dir_dentry == NULL) {
		pr_warning(KEDR_MSG_PREFIX
			"failed to create a directory in debugfs\n");
		ret = -EINVAL;
		goto out;
	}
	
	ret = debug_util_init(debugfs_dir_dentry);
	if (ret != 0)
		goto out_rmdir;
	
	ret = kedr_init_section_subsystem(debugfs_dir_dentry);
	if (ret != 0)
		goto out_cleanup_debug;
	
	/* [NB] If something else needs to be initialized, do it before 
	 * registering our callbacks with the notification system.
	 * Do not forget to re-check labels in the error path after that. */
	
	/* find_module() requires 'module_mutex' to be locked. */
	ret = mutex_lock_killable(&module_mutex);
	if (ret != 0)
	{
		pr_warning(KEDR_MSG_PREFIX 
			"Failed to lock module_mutex\n");
		goto out_cleanup_sections;
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
	return 0;

out_unreg_notifier:
	unregister_module_notifier(&detector_nb);

out_unlock:
	mutex_unlock(&module_mutex);

out_cleanup_sections:
	kedr_cleanup_section_subsystem();

out_cleanup_debug:
	debug_util_fini();

out_rmdir:
	debugfs_remove(debugfs_dir_dentry);
out:
	return ret;
}

module_init(test_init_module);
module_exit(test_cleanup_module);
/* ====================================================================== */
