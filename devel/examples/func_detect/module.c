/* An example demonstrating how to find the boundaries of the functions 
 * in a just loaded kernel module.
 */
 
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

/* ====================================================================== */
MODULE_AUTHOR("Eugene A. Shatokhin");
MODULE_LICENSE("GPL"); 

/* ====================================================================== */
/* Name of the module to analyze, an empty name will match no module */
static char* target_name = "";
module_param(target_name, charp, S_IRUGO);

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
	
	printk(KERN_INFO "[sample] "
	"target module \"%s\" has just loaded.\n",
		module_name(mod));
	
	/* Prevent our module from unloading when the target is loaded */
	if (try_module_get(THIS_MODULE) == 0)
	{
		printk(KERN_ERR "[sample] "
	"try_module_get() failed for the module \"%s\".\n",
			module_name(THIS_MODULE));
		module_get_failed = 1;
		
		/* If we failed to lock our module in memory, we should not
		 * instrument or otherwise affect the target module. */
		return;
	}
	
	/* Initialize everything necessary to process the target module */
	ret = kedr_init_function_subsystem();
	if (ret) {
		printk(KERN_ERR "[sample] "
	"Error occured in kedr_init_function_subsystem(). Code: %d\n",
			ret);
		goto fail;
	}
	
	//<>
	ret = kedr_load_function_list(mod);
	if (ret) {
		printk(KERN_ERR "[sample] "
	"Error occured while processing functions in \"%s\". Code: %d\n",
			module_name(mod), ret);
		goto cleanup_func_and_fail;
	}
	//<>
	// TODO
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
	printk(KERN_INFO "[sample] "
	"target module \"%s\" is going to unload.\n",
		module_name(mod));
	
	if (!module_get_failed) {
		// TODO
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
		printk(KERN_INFO "[sample] "
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
    .priority = 3, /* Some number */
};

/* ====================================================================== */
static int __init
sample_module_init(void)
{
	int result = 0;
	printk(KERN_INFO "[sample] Initializing\n");
	
	// TODO: if something else needs to be initialized, do it 
	// before registering with the notification system
	
	/* find_module() requires module_mutex to be locked. */
	result = mutex_lock_interruptible(&module_mutex);
	if (result != 0)
	{
		printk(KERN_INFO "[sample] "
		"failed to lock module_mutex\n");
		goto fail;
	}
    
	result = register_module_notifier(&detector_nb);
	if (result < 0)
		goto unlock_and_fail;
    
	/* Check if the target is already loaded */
	if (find_module(target_name) != NULL)
	{
		printk(KERN_INFO "[sample] "
		"target module \"%s\" is already loaded\n",
		target_name);

		printk(KERN_INFO "[sample] "
"instrumentation of already loaded target modules is not supported\n");
		result = -EEXIST;
		goto unreg_and_fail;
	}
    
	result = mutex_lock_interruptible(&target_module_mutex);
	if (result != 0)
	{
		printk(KERN_INFO "[sample] "
		"failed to lock target_module_mutex\n");
		goto unreg_and_fail;
	}

	handle_module_notifications = 1;
	mutex_unlock(&target_module_mutex);

	mutex_unlock(&module_mutex);
        
/* From now on, our module will be notified when the target module
 * is loaded or have finished cleaning-up and is just about to unload.
 */
	return 0; /* success */

unreg_and_fail:
	unregister_module_notifier(&detector_nb);

unlock_and_fail:
	mutex_unlock(&module_mutex);
fail:
	return result;
}

static void __exit
sample_module_exit(void)
{
	printk(KERN_INFO "[sample] Cleaning up\n");
	
	// Better to unregister notifications before cleaning up the rest.
	unregister_module_notifier(&detector_nb);
	
	// TODO: more cleanup if necessary
}

module_init(sample_module_init);
module_exit(sample_module_exit); 
