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

#include <kedr/kedr_mem/functions.h>
#include <kedr/kedr_mem/core_api.h>
#include <kedr/object_types.h>

#include <kedr-coi/interceptors/file_operations_interceptor.h>

#include "file_operations_model.h"
#include "inode_operations_model.h"
#include "super_operations_model.h"
#include "file_system_type_model.h"

#include "fs_interception.h"

#include "module_ref_model.h"
/* ====================================================================== */

MODULE_AUTHOR("Andrey Tsyvarev");
MODULE_LICENSE("GPL");
/* ====================================================================== */

/* Initialization tasks needed to use KEDR-COI. */
static int 
coi_init(void)
{
	int ret = 0;
	
	ret = fs_interception_init();
	if (ret != 0) 
		goto out;
	
    ret = file_operations_model_connect(&file_operations_interceptor_payload_register);
	if (ret != 0) 
		goto out_fs;

    ret = inode_operations_model_connect(&inode_operations_interceptor_payload_register);
	if (ret != 0) 
		goto out_file_model;

    ret = super_operations_model_connect(&super_operations_interceptor_payload_register);
	if (ret != 0) 
		goto out_inode_model;

    ret = file_system_type_model_connect(&file_system_type_interceptor_payload_register);
	if (ret != 0) 
		goto out_super_model;


    return 0;

out_super_model:
	super_operations_model_disconnect(&super_operations_interceptor_payload_unregister);
out_inode_model:
	inode_operations_model_disconnect(&inode_operations_interceptor_payload_unregister);
out_file_model:
	file_operations_model_disconnect(&file_operations_interceptor_payload_unregister);
out_fs:
	fs_interception_destroy();
out:
	return ret;
}

static void 
coi_cleanup(void)
{
	file_system_type_model_disconnect(&file_system_type_interceptor_payload_unregister);
	super_operations_model_disconnect(&super_operations_interceptor_payload_unregister);
	inode_operations_model_disconnect(&inode_operations_interceptor_payload_unregister);
	file_operations_model_disconnect(&file_operations_interceptor_payload_unregister);
	fs_interception_destroy();
}
/* ====================================================================== */
/* Interception of fs functions. */
static int register_filesystem_fst_lifetime(
    struct file_system_type* fs)
{
	int returnValue;
	
	unsigned long pc = (unsigned long)&register_filesystem;
	unsigned long tid = kedr_get_thread_id();

	file_system_type_interceptor_watch(fs);
	
	kedr_eh_on_signal_pre(tid, pc, FST_MODEL_STATE_PRE_REGISTERED(fs), KEDR_SWT_COMMON);
	
	returnValue = register_filesystem(fs);
	
	kedr_eh_on_signal_post(tid, pc, FST_MODEL_STATE_PRE_REGISTERED(fs), KEDR_SWT_COMMON);
	
	if(returnValue)
		file_system_type_interceptor_forget(fs);
	
	return returnValue;
}

static int unregister_filesystem_fst_lifetime(
    struct file_system_type* fs)
{
	int returnValue = unregister_filesystem(fs);
	
	if(returnValue == 0)
		file_system_type_interceptor_forget(fs);
	
	return returnValue;
}


static struct kedr_repl_pair rp[] = {
	{&register_filesystem, &register_filesystem_fst_lifetime},
	{&unregister_filesystem, &unregister_filesystem_fst_lifetime},

	{NULL, NULL}
};
/* ====================================================================== */

static void on_target_load(struct module* m)
{
    fs_interception_start();
}

static void on_target_unload(struct module* m)
{
    fs_interception_stop();
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
	.on_target_loaded = on_target_load,
    .on_target_about_to_unload = on_target_unload,
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
