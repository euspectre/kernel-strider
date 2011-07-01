/* This example demonstrates how to split the code of the functions into
 * the blocks (to be instrumented later). */
 
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

#include "functions.h"
#include "debug_util.h"
#include "detour_buffer.h"

/* ====================================================================== */
MODULE_AUTHOR("Eugene A. Shatokhin");
MODULE_LICENSE("GPL"); 

/* ====================================================================== */
/* Name of the module to analyze, an empty name will match no module */
static char* target_name = "";
module_param(target_name, charp, S_IRUGO);

/* Name of the function to process. */
char* target_function = "";
module_param(target_function, charp, S_IRUGO);

/* ====================================================================== */
/* The module being analyzed. NULL if the module is not currently loaded. */
struct module* target_module = NULL;

/* If nonzero, module load and unload notifications will be handled,
 * if 0, they will not.
 */
int handle_module_notifications = 0;

/* A mutex to protect target_module and related variables when processing 
 * loading and unloading of the target.
 */
DEFINE_MUTEX(target_module_mutex);

/* This flag indicates whether try_module_get() failed for our module in
 * on_module_load().
 */
int module_get_failed = 0;

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
	"target module \"%s\" has just loaded.\n",
		module_name(mod));
	
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
	ret = kedr_init_function_subsystem();
	if (ret) {
		pr_err("[sample] "
	"Error occured in kedr_init_function_subsystem(). Code: %d\n",
			ret);
		goto fail;
	}
	
	ret = kedr_process_target(mod);
	if (ret) {
		pr_err("[sample] "
	"Error occured while processing \"%s\". Code: %d\n",
			module_name(mod), ret);
		goto cleanup_func_and_fail;
	}

	// TODO: more processing if necessary
	return;
	
cleanup_func_and_fail: 
	kedr_cleanup_function_subsystem();
fail:	
	return;
}

/*
 * on_module_unload() handles uloading of the target module 
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
		kedr_cleanup_function_subsystem();
		module_put(THIS_MODULE);
	}
	module_get_failed = 0; /* reset it - just in case */
}

/* A callback function to handle loading and unloading of a module. 
 * Sets target_module pointer among other things. */
static int 
detector_notifier_call(struct notifier_block *nb,
	unsigned long mod_state, void *vmod)
{
	struct module* mod = (struct module *)vmod;
	BUG_ON(mod == NULL);
    
	if (mutex_lock_interruptible(&target_module_mutex) != 0)
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
	/* Priority 0 would also do but lower priority value is safer.
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
	int result = 0;
	pr_info("[sample] Initializing\n");
	
	result = debug_util_init();
	if (result != 0)
		goto fail;
		
	result = kedr_init_detour_subsystem();
	if (result != 0)
		goto deinit_and_fail;
	
	// TODO: if something else needs to be initialized, do it 
	// before registering our callbacks with the notification system.
	
	/* find_module() requires module_mutex to be locked. */
	result = mutex_lock_interruptible(&module_mutex);
	if (result != 0)
	{
		pr_info("[sample] "
		"failed to lock module_mutex\n");
		goto fini_detour_and_fail;
	}
    
	result = register_module_notifier(&detector_nb);
	if (result < 0)
		goto unlock_and_fail;
    
	/* Check if the target is already loaded */
	if (find_module(target_name) != NULL)
	{
		pr_info("[sample] "
		"target module \"%s\" is already loaded\n",
		target_name);

		pr_info("[sample] "
"instrumentation of already loaded target modules is not supported\n");
		result = -EEXIST;
		goto unreg_and_fail;
	}
    
	result = mutex_lock_interruptible(&target_module_mutex);
	if (result != 0)
	{
		pr_info("[sample] "
		"failed to lock target_module_mutex\n");
		goto unreg_and_fail;
	}

	handle_module_notifications = 1;
	mutex_unlock(&target_module_mutex);

	mutex_unlock(&module_mutex);
        
/* From now on, our module will be notified when the target module
 * is loaded or have finished cleaning-up and is just about to unload. */
	return 0; /* success */

unreg_and_fail:
	unregister_module_notifier(&detector_nb);

unlock_and_fail:
	mutex_unlock(&module_mutex);

fini_detour_and_fail:
	kedr_cleanup_detour_subsystem();

deinit_and_fail:
	debug_util_fini();

fail:
	return result;
}

static void __exit
sample_module_exit(void)
{
	pr_info("[sample] Cleaning up\n");
	
	// Better to unregister notifications before cleaning up the rest.
	unregister_module_notifier(&detector_nb);
	
	kedr_cleanup_detour_subsystem();
	debug_util_fini();
	// TODO: more cleanup if necessary
}

module_init(sample_module_init);
module_exit(sample_module_exit); 
