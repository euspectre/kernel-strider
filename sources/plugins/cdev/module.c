/* A plugin to the function handling subsystem that allows to use KEDR-COI
 * to establish needed happens-before links for character devices. */

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
#include <linux/init.h>
#include <linux/errno.h>
#include <linux/list.h>
#include <linux/slab.h>
#include <linux/fs.h>
#include <linux/cdev.h>

#include <kedr/kedr_mem/functions.h>
#include <kedr/kedr_mem/core_api.h>
#include <kedr/object_types.h>

#include <kedr-coi/interceptors/file_operations_interceptor.h>
#include "cdev_file_operations_interceptor.h"

#include "file_operations_model.h"

#include "module_ref_model.h"
/* ====================================================================== */

MODULE_AUTHOR("Andrey Tsyvarev");
MODULE_LICENSE("GPL");
/* ====================================================================== */

/* 
 * Signal-wait id for character device object.
 *
 * NOTE: Only 'PRE' id is exist. 'POST' id has no sence because no one
 * signal it (file operations may be executed after device unregistered).
 */

static inline unsigned long CDEV_MODEL_STATE_PRE_INITIALIZED(
	struct cdev* dev)
{
	//TODO: field of the cdev structure should be used.
	return (unsigned long)dev;
}

/* ====================================================================== */

/* Interception of file callbacks which determine lifetime of object. */
static void 
fop_open_post(struct inode* inode, struct file* filp, int ret_val,
	struct kedr_coi_operation_call_info* call_info)
{
	if (ret_val != 0) {
		/* If open() has failed, we may inform the interceptor that
		 * it does not need to bother watching the current '*filp' 
		 * object. */
		file_operations_interceptor_forget(filp);
	}
}

static void 
fop_release_post(struct inode* inode, struct file* filp, int ret_val,
	struct kedr_coi_operation_call_info* call_info)
{
	if (ret_val == 0) {
		/* If release() has been successful, the interceptor may
		 * stop watching '*filp'. */
		file_operations_interceptor_forget(filp);
	}
}

/* character-device-specific model*/
static void 
fop_open_pre(struct inode* inode, struct file* filp,
	struct kedr_coi_operation_call_info* call_info)
{
	unsigned long pc = (unsigned long)call_info->op_orig;
	unsigned long tid = kedr_get_thread_id();

    /* File may be opened only if corresponded device is added.*/
	kedr_eh_on_wait(tid, pc,
		CDEV_MODEL_STATE_PRE_INITIALIZED(inode->i_cdev), KEDR_SWT_COMMON);
}



static struct kedr_coi_pre_handler fop_pre_handlers[] =
{
	file_operations_open_pre_external(fop_open_pre),
	kedr_coi_pre_handler_end
};

static struct kedr_coi_post_handler fop_post_handlers[] =
{
    file_operations_open_post_external(fop_open_post),
	file_operations_release_post_external(fop_release_post),
	kedr_coi_post_handler_end
};

static struct kedr_coi_payload fop_payload =
{
	.mod = THIS_MODULE,
    
	.pre_handlers = fop_pre_handlers,
	.post_handlers = fop_post_handlers,
};

/* Initialization tasks needed to use KEDR-COI. */
static int 
coi_init(void)
{
	int ret = 0;
	
	ret = file_operations_interceptor_init(NULL);
	if (ret != 0) 
		goto out;
	
	ret = cdev_file_operations_interceptor_init(
		file_operations_interceptor_factory_interceptor_create,
		NULL);
	if (ret != 0) 
		goto out_fop;

	ret = file_operations_interceptor_payload_register(&fop_payload);
	if (ret != 0) 
		goto out_cdev;

    ret = file_operations_model_connect(&file_operations_interceptor_payload_register);
	if (ret != 0) 
		goto out_file_payload;


    return 0;

out_file_payload:
    file_operations_interceptor_payload_unregister(&fop_payload);
out_cdev:
	cdev_file_operations_interceptor_destroy();
out_fop:
	file_operations_interceptor_destroy();
out:
	return ret;
}

static void 
coi_cleanup(void)
{
    file_operations_model_disconnect(&file_operations_interceptor_payload_unregister);
	file_operations_interceptor_payload_unregister(&fop_payload);
	cdev_file_operations_interceptor_destroy();
	file_operations_interceptor_destroy();
}
/* ====================================================================== */
/* Interception of character device functions. */
static int 
repl_cdev_add(struct cdev *p, dev_t dev, unsigned int count)
{
	int ret;
	
	/* 
	 * PC should be converted into call address when trace will
	 * be processed.
	 */
	unsigned long pc = 0;
	unsigned long tid = kedr_get_thread_id();
	
	cdev_file_operations_interceptor_watch(p);
	
	/* 
     * Relation: Files for device(es) may be opened only after device(es)
     * registration.
     */
	kedr_eh_on_signal_pre(tid, pc,
		CDEV_MODEL_STATE_PRE_INITIALIZED(p), KEDR_SWT_COMMON);
    /* Call the target function itself. */
	ret = cdev_add(p, dev, count);
    
    kedr_eh_on_signal_post(tid, pc,
		CDEV_MODEL_STATE_PRE_INITIALIZED(p), KEDR_SWT_COMMON);	
	
	/* 
	 * If cdev_add() has failed, no need to watch the object.
	 * 
	 * Signal sending cannot be reverted, but this is not needed - no one
	 * can wait this signal.
	 */
	if (ret != 0)
		cdev_file_operations_interceptor_forget(p);
		
	return ret;
}

static void 
repl_cdev_del(struct cdev *p)
{
	/* It is caller who should order this call wrt others. */
    cdev_file_operations_interceptor_forget(p);
}

struct kedr_repl_pair rp[] = {
	{&cdev_add, &repl_cdev_add},
	{&cdev_del, &repl_cdev_del},

	{NULL, NULL}
};
/* ====================================================================== */

static void 
on_load(struct module *mod)
{
    file_operations_interceptor_start();
}

static void 
on_unload(struct module *mod)
{
	file_operations_interceptor_stop();
}

static void on_before_exit(struct module* m)
{
	/* Relation: all module_put(m) calls should be happened before exit() */
	unsigned long tid = kedr_get_thread_id();
	unsigned long pc = (unsigned long)m->exit;

	kedr_eh_on_wait(tid, pc, MODULE_MODEL_STATE_POST_INITIALIZED(m),
		KEDR_SWT_COMMON);
}

/* ====================================================================== */

struct kedr_fh_plugin fh_plugin = {
	.owner = THIS_MODULE,
	.on_target_loaded = on_load,
    .on_target_about_to_unload = on_unload,
	.on_before_exit_call = on_before_exit,
	.repl_pairs = rp
};
/* ====================================================================== */

static void __exit
plugin_coi_exit(void)
{
	kedr_fh_plugin_unregister(&fh_plugin);
	coi_cleanup();
	
	return;
}

static int __init
plugin_coi_init(void)
{
	int ret = 0;
	
	ret = coi_init();
	if (ret != 0)
		goto out_unreg;
	
	ret = kedr_fh_plugin_register(&fh_plugin);
	if (ret != 0)
		goto out;
	
	return 0;

out_unreg:
	kedr_fh_plugin_unregister(&fh_plugin);
out:
	return ret;
}

module_init(plugin_coi_init);
module_exit(plugin_coi_exit);
/* ====================================================================== */
