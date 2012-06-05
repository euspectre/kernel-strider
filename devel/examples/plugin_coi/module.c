/* A plugin to the function handling subsystem that allows to use KEDR-COI
 * to establish several kinds of happens-before links. */

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
#include "cdev_fops_interceptor.h"
/* ====================================================================== */

MODULE_AUTHOR("Eugene A. Shatokhin");
MODULE_LICENSE("GPL");
/* ====================================================================== */

#define KEDR_MSG_PREFIX "[kedr_drd_plugin_coi] "
/* ====================================================================== */

/* IDs of the happens-before arcs for a given device. */
struct cdev_hb_id
{
	struct list_head list;
	dev_t devno;
	
	/* ID for the following relation: "Registration of a callback for a 
	 * given device starts before the execution of that callback for 
	 * that device starts". 
	 * As we can intercept open() calls relatively reliably, we can 
	 * create the happens-before arc between the start of cdev_add() and
	 * the start of open() rather than do a similar thing for all 
	 * callbacks. */
	unsigned long id_reg_start;
	
	/* ID for the following relation: "Execution of a callback ends 
	 * before the end of the deregistration of that callback or before
	 * the start of the module's exit function (if it exists), whichever
	 * comes first".
	 * It is enough to consider release() callback here. If we were not
	 * sure whether release() could be intercepted reliably, we would 
	 * have to do a similar thing for each callback. */
	unsigned long id_end_exit;
};

/* The list of the IDs of happens-before arcs used for the character
 * devices. */
static LIST_HEAD(cdev_hb_ids);

/* Searches for the ID corresponding to the device with the given major and
 * minor numbers ('mj' and 'mn', respectively). Returns the ID structure if 
 * found, NULL if not. */
static struct cdev_hb_id *
find_id_for_cdev(unsigned int mj, unsigned int mn)
{
	struct cdev_hb_id *pos;
	list_for_each_entry(pos, &cdev_hb_ids, list) {
		if (MAJOR(pos->devno) == mj && MINOR(pos->devno) == mn)
			return pos;
	}
	return NULL;
}

/* Creates the ID structure for a device with the given major and minor 
 * numbers and adds it to the list. Returns the pointer to the ID structure 
 * if successful, NULL otherwise. 
 * The function does not check if an ID already exists for the given device.
 */
static struct cdev_hb_id *
create_id_for_cdev(unsigned int mj, unsigned int mn)
{
	struct cdev_hb_id *item;
	
	item = kzalloc(sizeof(*item), GFP_ATOMIC);
	if (item == NULL)
		return NULL;
	
	item->devno = MKDEV(mj, mn);
	item->id_reg_start = kedr_get_unique_id();
	item->id_end_exit = kedr_get_unique_id();
	
	if (item->id_reg_start == 0 || item->id_end_exit == 0) {
		kfree(item);
		return NULL;
	}
	
	list_add_tail(&item->list, &cdev_hb_ids);
	return item;
}
/* ====================================================================== */

/* The skeleton of this part is based on "Read counter" example provided 
 * with KEDR-COI, adapted to suit the goals of this plugin. */

static void 
fop_open_pre(struct inode* inode, struct file* filp,
	struct kedr_coi_operation_call_info* call_info)
{
	struct cdev_hb_id *item;
	unsigned long id = 0;
	struct kedr_event_handlers *eh;
	unsigned long tid;
	unsigned long pc;
	
	file_operations_interceptor_watch(filp);
	
	/* Relation: "Registration of the callbacks starts before open() 
	 * starts". */
	eh = kedr_get_event_handlers();
	tid = kedr_get_thread_id();
	pc = (unsigned long)call_info->op_orig;
	
	item = find_id_for_cdev(imajor(inode), iminor(inode));
	if (item != NULL) {
		id = item->id_reg_start;
		if (eh->on_wait_pre != NULL)
			eh->on_wait_pre(eh, tid, pc, id, KEDR_SWT_COMMON);
		
		if (eh->on_wait_post != NULL)
			eh->on_wait_post(eh, tid, pc, id, KEDR_SWT_COMMON);
	}
	else {
		pr_warning(KEDR_MSG_PREFIX 
	"fop_open_pre(): not found ID for the device (%u, %u)\n", 
			imajor(inode), iminor(inode));
	}
	
	/* Specify that the struct file instance pointed to by 'filp' is 
	 * now available ("memory acquired"). */
	if (eh->on_alloc_pre != NULL) 
		eh->on_alloc_pre(eh, tid, pc, sizeof(*filp));
	if (eh->on_alloc_post != NULL)
		eh->on_alloc_post(eh, tid, pc, sizeof(*filp), 
			(unsigned long)filp);
}

static void 
fop_open_post(struct inode* inode, struct file* filp, int ret_val,
	struct kedr_coi_operation_call_info* call_info)
{
	if (ret_val != 0) {
		struct kedr_event_handlers *eh;
		unsigned long tid;
		unsigned long pc;
		
		/* If open() has failed, we may inform the interceptor that
		 * it does not need to bother watching the current '*filp' 
		 * object. */
		file_operations_interceptor_forget(filp);
	
		eh = kedr_get_event_handlers();
		tid = kedr_get_thread_id();
		pc = (unsigned long)call_info->op_orig;
		
		/* Specify that the struct file instance pointed to by 
		 * 'filp' is no longer available ("memory released"). */
		if (eh->on_free_pre != NULL)
			eh->on_free_pre(eh, tid, pc, (unsigned long)filp);
		if (eh->on_free_post != NULL)
			eh->on_free_post(eh, tid, pc, (unsigned long)filp);
	}
}

static void 
fop_release_post(struct inode* inode, struct file* filp, int ret_val,
	struct kedr_coi_operation_call_info* call_info)
{
	struct cdev_hb_id *item;
	unsigned long id = 0;
	struct kedr_event_handlers *eh;
	unsigned long tid;
	unsigned long pc;
	
	eh = kedr_get_event_handlers();
	tid = kedr_get_thread_id();
	pc = (unsigned long)call_info->op_orig;
	
	if (ret_val == 0) {
		/* If release() has been successful, the interceptor may
		 * stop watching '*filp'. */
		file_operations_interceptor_forget(filp);
		
		/* Specify that the struct file instance pointed to by 
		 * 'filp' is no longer available ("memory released"). */
		if (eh->on_free_pre != NULL)
			eh->on_free_pre(eh, tid, pc, (unsigned long)filp);
		if (eh->on_free_post != NULL)
			eh->on_free_post(eh, tid, pc, (unsigned long)filp);
	}
	
	/* Relation: "file operations happen-before the exit function or 
	 * deregistration, whichever comes first". */
	item = find_id_for_cdev(imajor(inode), iminor(inode));
	if (item != NULL) {
		id = item->id_end_exit;
		if (eh->on_signal_pre != NULL)
			eh->on_signal_pre(eh, tid, pc, id, KEDR_SWT_COMMON);
		
		if (eh->on_signal_post != NULL)
			eh->on_signal_post(eh, tid, pc, id, KEDR_SWT_COMMON);
	}
	else {
		pr_warning(KEDR_MSG_PREFIX 
	"fop_release_post(): not found ID for the device (%u, %u)\n", 
			imajor(inode), iminor(inode));
	}
}

static struct kedr_coi_pre_handler fop_pre_handlers[] =
{
	file_operations_open_pre(fop_open_pre),
	kedr_coi_pre_handler_end
};

static struct kedr_coi_post_handler fop_post_handlers[] =
{
	file_operations_open_post(fop_open_post),
	file_operations_release_post_external(fop_release_post),
	kedr_coi_post_handler_end
};

static struct kedr_coi_payload fop_payload =
{
	/* 'mod' remains NULL because locking this module in the memory 
	 * is already taken care of by the function handling subsystem. */
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

	ret = file_operations_interceptor_start();
	if (ret != 0) 
		goto out_unreg;
	
	return 0;

out_unreg:
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
	file_operations_interceptor_stop();
	file_operations_interceptor_payload_unregister(&fop_payload);
	cdev_file_operations_interceptor_destroy();
	file_operations_interceptor_destroy();
}
/* ====================================================================== */

static void 
on_before_exit(struct module *mod)
{
	struct kedr_event_handlers *eh;
	unsigned long tid;
	unsigned long pc;
	struct cdev_hb_id *pos;
	
	/* Relation: "file operations happen-before the exit function or 
	 * deregistration, whichever comes first". */
	eh = kedr_get_event_handlers();
	tid = kedr_get_thread_id();
	pc = (unsigned long)mod->exit;
	
	list_for_each_entry(pos, &cdev_hb_ids, list) {
		unsigned long id = pos->id_end_exit;
		
		if (eh->on_wait_pre != NULL)
			eh->on_wait_pre(eh, tid, pc, id, KEDR_SWT_COMMON);
		
		if (eh->on_wait_post != NULL)
			eh->on_wait_post(eh, tid, pc, id, KEDR_SWT_COMMON);
	}
}
/* ====================================================================== */

static int 
repl_cdev_add(struct cdev *p, dev_t dev, unsigned int count)
{
	int ret;
	unsigned int mn = MINOR(dev);
	unsigned int mj = MAJOR(dev);
	unsigned int i;
	
	struct cdev_hb_id *item;
	unsigned long id = 0;
	struct kedr_event_handlers *eh;
	unsigned long tid;
	unsigned long pc;
	
	cdev_file_operations_interceptor_watch(p);
	
	/* Relation: "Registration of the callbacks starts before open() 
	 * starts". Applying it to each device separately. */
	eh = kedr_get_event_handlers();
	tid = kedr_get_thread_id();
	pc = (unsigned long)&cdev_add;
	
	for (i = 0; i < count; ++i) {
		item = create_id_for_cdev(mj, mn + i);
		if (item == NULL) {
			pr_warning(KEDR_MSG_PREFIX 
	"repl_cdev_add(): failed to obtain ID for the device (%u, %u)\n", 
				mj, mn + i);
			continue;
		}
		
		id = item->id_reg_start;	
		if (eh->on_signal_pre != NULL)
			eh->on_signal_pre(eh, tid, pc, id, KEDR_SWT_COMMON);
		
		if (eh->on_signal_post != NULL)
			eh->on_signal_post(eh, tid, pc, id, KEDR_SWT_COMMON);
	}
	
	/* Call the target function itself. */
	ret = cdev_add(p, dev, count);
	
	/* If cdev_add() has failed, no need to watch the object. */
	if (ret != 0)
		cdev_file_operations_interceptor_forget(p);
		
	return ret;
}

static void 
repl_cdev_del(struct cdev *p)
{
	unsigned int mj;
	unsigned int mn;
	unsigned int count;
	unsigned int i;
	
	struct cdev_hb_id *item;
	unsigned long id = 0;
	struct kedr_event_handlers *eh;
	unsigned long tid;
	unsigned long pc;
		
	mj = MAJOR(p->dev);
	mn = MINOR(p->dev);
	count = p->count;
	
	cdev_del(p);
	
	/* Relation: "file operations happen-before the exit function or 
	 * deregistration, whichever comes first". */
	eh = kedr_get_event_handlers();
	tid = kedr_get_thread_id();
	pc = (unsigned long)&cdev_del;
	
	for (i = 0; i < count; ++i) {
		item = find_id_for_cdev(mj, mn + i);
		if (item == NULL) {
			pr_warning(KEDR_MSG_PREFIX 
	"repl_cdev_del(): not found ID for the device (%u, %u)\n", 
				mj, mn + i);
			continue;
		}
		
		id = item->id_end_exit;
		if (eh->on_wait_pre != NULL)
			eh->on_wait_pre(eh, tid, pc, id, KEDR_SWT_COMMON);
		
		if (eh->on_wait_post != NULL)
			eh->on_wait_post(eh, tid, pc, id, KEDR_SWT_COMMON);
	}
	
	cdev_file_operations_interceptor_forget(p);
}

struct kedr_repl_pair rp[] = {
	{&cdev_add, &repl_cdev_add},
	{&cdev_del, &repl_cdev_del},
	/* [NB] Add more replacement functions if needed */
	{NULL, NULL}
};
/* ====================================================================== */

static void 
on_unload(struct module *mod)
{
	struct cdev_hb_id *pos;
	struct cdev_hb_id *tmp;
	
	list_for_each_entry_safe(pos, tmp, &cdev_hb_ids, list) {
		list_del(&pos->list);
		kfree(pos);
	}
}
/* ====================================================================== */

struct kedr_fh_plugin fh_plugin = {
	.owner = THIS_MODULE,
	.on_before_exit_call = on_before_exit,
	.on_target_about_to_unload = on_unload,
	.repl_pairs = rp
};
/* ====================================================================== */

static void __exit
plugin_coi_exit(void)
{
	kedr_fh_plugin_unregister(&fh_plugin);
	coi_cleanup();
	
	/* [NB] If needed, perform more cleanup here. */
	return;
}

static int __init
plugin_coi_init(void)
{
	int ret = 0;
	/* [NB] If needed, perform more initialization tasks here. */
	
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
