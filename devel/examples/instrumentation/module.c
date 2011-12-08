/* An example demonstrating instrumentation of memory reads and writes */
 
/*
 * module.c: module-related definitions; loading and unloading detection.
 */

#include <linux/version.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/string.h>
#include <linux/mutex.h>
#include <linux/debugfs.h>
#include <linux/list.h>
#include <linux/err.h>

#include "functions.h"
#include "debug_util.h"
#include "detour_buffer.h"
#include "sections.h"
#include "demo.h"
/* ====================================================================== */

MODULE_AUTHOR("Eugene A. Shatokhin");
MODULE_LICENSE("GPL"); 
/* ====================================================================== */

/* Name of the module to analyze, an empty name will match no module */
char *target_name = "";
module_param(target_name, charp, S_IRUGO);

/* [DBG] Name of the function to provide additional debug output for. */
char *target_function = "";
module_param(target_function, charp, S_IRUGO);

/* If 0, memory access operations with the addressing expressions based on
 * %rsp/%esp will not be recorded. The corresponding instructions are likely
 * to deal only with the local variables of the function and its parameters
 * passed by value. 
 * Note that this does not "cut off" all the operations with the stack, just
 * those that reference %rsp/%esp directly.
 * If the parameter has a non-zero value, these operations will be 
 * instrumented and processed like any other ones.
 * Leaving this parameter as zero may reduce code bloat: the instrumented 
 * versions of the affected memory access operations may be smaller.
 * Default value: 0. */
int process_sp_accesses = 0;
module_param(process_sp_accesses, int, S_IRUGO);
/* ====================================================================== */

/* A directory for our system in debugfs. */
static struct dentry *debugfs_dir_dentry = NULL;
const char *debugfs_dir_name = "kedr_sample";
/* ====================================================================== */

/* The module being analyzed. NULL if the module is not currently loaded. */
static struct module *target_module = NULL;

/* If nonzero, module load and unload notifications will be handled,
 * if 0, they will not.
 */
static int handle_module_notifications = 0;

/* A mutex to protect target_module and related variables when processing 
 * loading and unloading of the target.
 */
static DEFINE_MUTEX(target_module_mutex);

/* This flag indicates whether try_module_get() failed for our module in
 * on_module_load().
 */
static int module_get_failed = 0;
/* ====================================================================== */

/* Module filter.
 * Should return nonzero if detector should watch for module with this name.
 * We are interested in analyzing only the module with the given name.
 */
static int 
filter_module(const char *mod_name)
{
	return strcmp(mod_name, target_name) == 0;
}

/*
 * on_module_load() handles loading of the target module 
 * ("just loaded" event).
 *
 * Note that this function is called with target_module_mutex locked.
 */
static void 
on_module_load(struct module *mod)
{
	int ret = 0;
		
	pr_info("[sample] "
	"Target module \"%s\" has just loaded. Estimated size of the "
	"code areas (in bytes): %u\n",
		module_name(mod), 
		(mod->init_text_size + mod->core_text_size));
	
	/* Prevent our module from unloading when the target is loaded */
	if (try_module_get(THIS_MODULE) == 0)
	{
		pr_err("[sample] "
	"try_module_get() failed for the module \"%s\".\n",
			module_name(THIS_MODULE));
		module_get_failed = 1;
		
		/* If we failed to lock our module in memory, we should not
		 * instrument or otherwise affect the target module. */
		return;
	}
	
	/* Clear previous debug data */
	debug_util_clear();
	
	/* Initialize everything necessary to process the target module */
	ret = kedr_init_function_subsystem(mod);
	if (ret) {
		pr_err("[sample] "
	"Failed to initialize function subsystem. Error code: %d\n",
			ret);
		goto out;
	}
	
	ret = kedr_process_target(mod);
	if (ret) {
		pr_err("[sample] "
	"Error occured while processing \"%s\". Code: %d\n",
			module_name(mod), ret);
		goto out_cleanup_func;
	}
	
	ret = kedr_demo_init(mod);
	if (ret) {
		pr_err("[sample] "
	"Failed to initialize \"demo\" subsystem. Code: %d\n", ret);
		goto out_cleanup_func;
	}
	return;
	
out_cleanup_func: 
	kedr_cleanup_function_subsystem();
out:	
	return;
}

/*
 * on_module_unload() handles unloading of the target module 
 * ("cleaned up and about to unload" event).
 *
 * Note that this function is called with target_module_mutex locked.
 *
 * [NB] This function is called even if initialization of the target module 
 * fails.
 * */
static void 
on_module_unload(struct module *mod)
{
	pr_info("[sample] "
	"target module \"%s\" is going to unload.\n",
		module_name(mod));
	
	if (!module_get_failed) {
		// TODO: cleanup what is left (if anything)
		kedr_demo_fini(mod);
		kedr_cleanup_function_subsystem();
		module_put(THIS_MODULE);
	}
	module_get_failed = 0; /* reset the flag */
}

/* A callback function to handle loading and unloading of a module. 
 * Sets target_module pointer among other things. */
static int 
detector_notifier_call(struct notifier_block *nb,
	unsigned long mod_state, void *vmod)
{
	struct module* mod = (struct module *)vmod;
	BUG_ON(mod == NULL);
    
	if (mutex_lock_killable(&target_module_mutex) != 0)
	{
		pr_warning("[sample] "
		"failed to lock target_module_mutex\n");
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

		BUG_ON(target_module != NULL);
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
	mutex_unlock(&target_module_mutex);
	return 0;
}
/* ================================================================ */

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

static int __init
sample_module_init(void)
{
	int ret = 0;
	pr_info("[sample] Initializing\n");
	
	debugfs_dir_dentry = debugfs_create_dir(debugfs_dir_name, NULL);
	if (IS_ERR(debugfs_dir_dentry)) {
		pr_err("[sample] debugfs is not supported\n");
		ret = -ENODEV;
		goto out;
	}

	if (debugfs_dir_dentry == NULL) {
		pr_err("[sample] "
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
	
	ret = kedr_init_detour_subsystem();
	if (ret != 0)
		goto out_cleanup_sections;
	
	// TODO: if something else needs to be initialized, do it 
	// before registering our callbacks with the notification system.
	
	/* find_module() requires 'module_mutex' to be locked. */
	ret = mutex_lock_killable(&module_mutex);
	if (ret != 0)
	{
		pr_info("[sample] "
		"failed to lock module_mutex\n");
		goto out_cleanup_detour;
	}
    
	ret = register_module_notifier(&detector_nb);
	if (ret < 0)
		goto out_unlock;
    
	/* Check if the target is already loaded */
	if (find_module(target_name) != NULL)
	{
		pr_info("[sample] "
		"target module \"%s\" is already loaded\n",
		target_name);

		pr_info("[sample] "
"processing of already loaded target modules is not supported\n");
		ret = -EEXIST;
		goto out_unreg_notifier;
	}
    
	ret = mutex_lock_killable(&target_module_mutex);
	if (ret != 0)
	{
		pr_info("[sample] "
		"failed to lock target_module_mutex\n");
		goto out_unreg_notifier;
	}

	handle_module_notifications = 1;
	mutex_unlock(&target_module_mutex);

	mutex_unlock(&module_mutex);
        
/* From now on, our module will be notified when the target module
 * is loaded or have finished cleaning-up and is just about to unload. */
	return 0; /* success */

out_unreg_notifier:
	unregister_module_notifier(&detector_nb);

out_unlock:
	mutex_unlock(&module_mutex);

out_cleanup_detour:
	kedr_cleanup_detour_subsystem();

out_cleanup_sections:
	kedr_cleanup_section_subsystem();

out_cleanup_debug:
	debug_util_fini();

out_rmdir:
	debugfs_remove(debugfs_dir_dentry);

out:
	return ret;
}

static void __exit
sample_module_exit(void)
{
	pr_info("[sample] Cleaning up\n");
	
	/* Unregister notifications before cleaning up the rest. */
	unregister_module_notifier(&detector_nb);
	
	kedr_cleanup_detour_subsystem();
	kedr_cleanup_section_subsystem();
	debug_util_fini();
	debugfs_remove(debugfs_dir_dentry);
	
	// TODO: more cleanup if necessary
}

module_init(sample_module_init);
module_exit(sample_module_exit); 
