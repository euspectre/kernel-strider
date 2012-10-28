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

#include <linux/module.h>
#include <linux/init.h>
#include <linux/errno.h>
#include <linux/kernel.h>
#include <linux/string.h>
#include <linux/list.h>

#include <linux/cdev.h>
#include <linux/fs.h>
#include <linux/slab.h>
#include <linux/spinlock.h>

#include <kedr/kedr_mem/core_api.h>
#include <kedr/kedr_mem/local_storage.h>
#include <kedr/kedr_mem/functions.h>
#include <kedr/object_types.h>
/* ====================================================================== */

#define KEDR_MSG_PREFIX "[kedr_fh_drd_cdev] "
/* ====================================================================== */

MODULE_AUTHOR("Eugene A. Shatokhin");
MODULE_LICENSE("GPL");
/* ====================================================================== */

/* Types of the file operation callbacks for character devices. Other 
 * types can be added before KEDR_CB_CDEV_COUNT if it is needed to process 
 * other kinds of callbacks. */
enum kedr_cdev_callback_type {
	KEDR_CB_CDEV_OPEN = 0,
	KEDR_CB_CDEV_RELEASE,
	KEDR_CB_CDEV_READ,
	KEDR_CB_CDEV_WRITE,
	KEDR_CB_CDEV_LLSEEK,
	
	/* Number of the known types, keep this item last. */
	KEDR_CB_CDEV_COUNT
};

/* IDs of the happens-before arcs for a given device. */
struct kedr_cdev_hb_id
{
	struct list_head list;
	dev_t devno;
	
	/* IDs for the following relation: "Registration of a callback for a 
	 * given device starts before the execution of that callback for 
	 * that device starts". */
	unsigned long ids_reg_start[KEDR_CB_CDEV_COUNT];
	
	/* IDs for the following relation: "Execution of a callback ends 
	 * before the end of the deregistration of that callback or before
	 * the start of the module's exit function (if it exists), whichever
	 * comes first". */
	unsigned long ids_end_exit[KEDR_CB_CDEV_COUNT];
};
/* ====================================================================== */

/* The list of the IDs of happens-before arcs used for the character
 * devices. */
static LIST_HEAD(cdev_hb_ids);

/* A spinlock to protect the list of the IDs. */
static DEFINE_SPINLOCK(cdev_ids_lock);

/* Searches for the IDs corresponding to the device with the given major and
 * minor numbers ('mj' and 'mn', respectively). Returns the ID structure if 
 * found, NULL if not. */
static struct kedr_cdev_hb_id *
find_ids_for_cdev(unsigned int mj, unsigned int mn)
{
	struct kedr_cdev_hb_id *pos;
	unsigned long flags;
	
	spin_lock_irqsave(&cdev_ids_lock, flags);
	list_for_each_entry(pos, &cdev_hb_ids, list) {
		if (MAJOR(pos->devno) == mj && MINOR(pos->devno) == mn) {
			spin_unlock_irqrestore(&cdev_ids_lock, flags);
			return pos;
		}
	}
	spin_unlock_irqrestore(&cdev_ids_lock, flags);
	return NULL;
}

/* Creates the ID structure for a device with the given major and minor 
 * numbers and adds it to the list. Returns the pointer to the ID structure 
 * if successful, NULL otherwise. 
 * The function does not check if an ID structure already exists for the 
 * given device. */
static struct kedr_cdev_hb_id *
create_ids_for_cdev(unsigned int mj, unsigned int mn)
{
	struct kedr_cdev_hb_id *item;
	unsigned long flags;
	unsigned int i; 
	
	item = kzalloc(sizeof(*item), GFP_KERNEL);
	if (item == NULL)
		return NULL;
	
	item->devno = MKDEV(mj, mn);
	for (i = 0; i < KEDR_CB_CDEV_COUNT; ++i) {
		item->ids_reg_start[i] = kedr_get_unique_id();
		item->ids_end_exit[i] = kedr_get_unique_id();
		
		if (item->ids_reg_start[i] == 0 || 
		    item->ids_end_exit[i] == 0) {
			kfree(item);
			return NULL;
		}
	}
	
	spin_lock_irqsave(&cdev_ids_lock, flags);
	/* If a device with the same major and minor numbers is created
	 * more than once, the item for the most recently created device
	 * will be found by find_ids_for_cdev() because list_add()
	 * adds it to the head of the list. */
	list_add(&item->list, &cdev_hb_ids);
	spin_unlock_irqrestore(&cdev_ids_lock, flags);
	return item;
}

/* Trigger an event for each possible type of a callback. */
static void
trigger_reg_start_signal_events(unsigned long tid, unsigned long pc, 
	struct kedr_cdev_hb_id *ids)
{
	unsigned long id = 0;
	unsigned int cbtype;
	
	for (cbtype = 0; cbtype < KEDR_CB_CDEV_COUNT; ++cbtype) {
		id = ids->ids_reg_start[cbtype];	
		kedr_eh_on_signal(tid, pc, id, KEDR_SWT_COMMON);
	}
}

static void
trigger_end_exit_wait_events(unsigned long tid, unsigned long pc, 
	struct kedr_cdev_hb_id *ids)
{
	unsigned long id = 0;
	unsigned int cbtype;
	
	for (cbtype = 0; cbtype < KEDR_CB_CDEV_COUNT; ++cbtype) {
		id = ids->ids_end_exit[cbtype];	
		kedr_eh_on_wait(tid, pc, id, KEDR_SWT_COMMON);
	}
}

/* This structure is needed to pass the data from the pre- to the post
 * handler for cdev_del(). */
struct kedr_data_cdev_del
{
	unsigned int mj;
	unsigned int mn;
	unsigned int count;
};

/* Use this function in the pre handlers for file operations having 
 * 'struct file *' and probably 'struct inode *' as the arguments to 
 * generate appropriate events. */
static void 
fop_common_pre(enum kedr_cdev_callback_type cb_type, unsigned long pc, 
	struct inode *inode, struct file *filp)
{
	unsigned long id;
	struct kedr_cdev_hb_id *item;
	unsigned long tid;
	
	tid = kedr_get_thread_id();
	
	item = find_ids_for_cdev(imajor(inode), iminor(inode));
	if (item != NULL) {
		id = item->ids_reg_start[cb_type];
		kedr_eh_on_wait(tid, pc, id, KEDR_SWT_COMMON);
	}
	else {
		pr_warning(KEDR_MSG_PREFIX 
	"fop_common_pre(): not found ID for the device (%u, %u)\n", 
			imajor(inode), iminor(inode));
	}
	
	/* Specify that the struct file instance pointed to by 'filp' is 
	 * now available ("memory acquired"). 
	 * [NB] Actually, the instance is likely to be allocated before 
	 * open() and deallocated after release(). But it is OK for now to
	 * assume for simplicity that it is allocated before entry to a 
	 * given callback and deallocated after the callback completes. */
	kedr_eh_on_alloc(tid, pc, sizeof(*filp), (unsigned long)filp);
}

/* Use this function in the post handlers for file operations having 
 * 'struct file *' and probably 'struct inode *' as the arguments to 
 * generate appropriate events. */
static void 
fop_common_post(enum kedr_cdev_callback_type cb_type, unsigned long pc, 
	struct inode *inode, struct file *filp)
{
	unsigned long id;
	struct kedr_cdev_hb_id *item;
	unsigned long tid;
	
	tid = kedr_get_thread_id();
	
	/* Specify that the struct file instance pointed to by 'filp' is
	 * no longer available ("memory released"). */
	kedr_eh_on_free(tid, pc, (unsigned long)filp);
	
	/* HB relation */
	item = find_ids_for_cdev(imajor(inode), iminor(inode));
	if (item != NULL) {
		id = item->ids_end_exit[cb_type];
		kedr_eh_on_signal(tid, pc, id, KEDR_SWT_COMMON);
	}
	else {
		pr_warning(KEDR_MSG_PREFIX 
	"fop_common_post(): not found ID for the device (%u, %u)\n", 
			imajor(inode), iminor(inode));
	}
}

static void 
fop_open_pre(struct kedr_local_storage *ls)
{
	struct inode *inode;
	struct file *filp;
	inode = (struct inode *)KEDR_LS_ARG1(ls);
	filp = (struct file *)KEDR_LS_ARG2(ls);
	
	fop_common_pre(KEDR_CB_CDEV_OPEN, ls->fi->addr, inode, filp);
}

static void 
fop_open_post(struct kedr_local_storage *ls)
{
	struct inode *inode;
	struct file *filp;
	inode = (struct inode *)KEDR_LS_ARG1(ls);
	filp = (struct file *)KEDR_LS_ARG2(ls);
		
	fop_common_post(KEDR_CB_CDEV_OPEN, ls->fi->addr, inode, filp);
}

static void 
fop_release_pre(struct kedr_local_storage *ls)
{
	struct inode *inode;
	struct file *filp;
	inode = (struct inode *)KEDR_LS_ARG1(ls);
	filp = (struct file *)KEDR_LS_ARG2(ls);
	
	fop_common_pre(KEDR_CB_CDEV_RELEASE, ls->fi->addr, inode, filp);
}

static void 
fop_release_post(struct kedr_local_storage *ls)
{
	struct inode *inode;
	struct file *filp;
	inode = (struct inode *)KEDR_LS_ARG1(ls);
	filp = (struct file *)KEDR_LS_ARG2(ls);
	
	fop_common_post(KEDR_CB_CDEV_RELEASE, ls->fi->addr, inode, filp);
}

static void 
fop_read_pre(struct kedr_local_storage *ls)
{
	struct file *filp;
	filp = (struct file *)KEDR_LS_ARG1(ls);
	fop_common_pre(KEDR_CB_CDEV_READ, ls->fi->addr, 
		filp->f_dentry->d_inode, filp);	
}

static void 
fop_read_post(struct kedr_local_storage *ls)
{
	struct file *filp;
	filp = (struct file *)KEDR_LS_ARG1(ls);
	fop_common_post(KEDR_CB_CDEV_READ, ls->fi->addr, 
		filp->f_dentry->d_inode, filp);
}

static void 
fop_write_pre(struct kedr_local_storage *ls)
{
	struct file *filp;
	filp = (struct file *)KEDR_LS_ARG1(ls);
	fop_common_pre(KEDR_CB_CDEV_WRITE, ls->fi->addr, 
		filp->f_dentry->d_inode, filp);	
}

static void 
fop_write_post(struct kedr_local_storage *ls)
{
	struct file *filp;
	filp = (struct file *)KEDR_LS_ARG1(ls);
	fop_common_post(KEDR_CB_CDEV_WRITE, ls->fi->addr, 
		filp->f_dentry->d_inode, filp);
}

static void 
fop_llseek_pre(struct kedr_local_storage *ls)
{
	struct file *filp;
	filp = (struct file *)KEDR_LS_ARG1(ls);
	fop_common_pre(KEDR_CB_CDEV_LLSEEK, ls->fi->addr, 
		filp->f_dentry->d_inode, filp);	
}

static void 
fop_llseek_post(struct kedr_local_storage *ls)
{
	struct file *filp;
	filp = (struct file *)KEDR_LS_ARG1(ls);
	fop_common_post(KEDR_CB_CDEV_LLSEEK, ls->fi->addr, 
		filp->f_dentry->d_inode, filp);
}

/* [NB] Mind the order of the initializers. */
static void (*pre_handler[])(struct kedr_local_storage *ls) = {
	fop_open_pre,
	fop_release_pre,
	fop_read_pre,
	fop_write_pre,
	fop_llseek_pre,
};
static void (*post_handler[])(struct kedr_local_storage *ls) = {
	fop_open_post,
	fop_release_post,
	fop_read_post,
	fop_write_post,
	fop_llseek_post,
};

/* Checks if the callbacks in '*p' can be found in the target module and if
 * so, sets the handlers for them. If a handler is already set, the function
 * does not change it. */
static void
set_callback_handlers(struct cdev *p)
{
	unsigned long cb_addr[KEDR_CB_CDEV_COUNT];
	const struct file_operations *fops;
	unsigned int i;
	unsigned long flags;
	
	if (p == NULL)
		return;
	fops = p->ops;
	
	memset(&cb_addr[0], 0, sizeof(cb_addr)); /* just in case */
	cb_addr[KEDR_CB_CDEV_OPEN] = 	(unsigned long)fops->open;
	cb_addr[KEDR_CB_CDEV_RELEASE] = (unsigned long)fops->release;
	cb_addr[KEDR_CB_CDEV_READ] = 	(unsigned long)fops->read;
	cb_addr[KEDR_CB_CDEV_WRITE] = 	(unsigned long)fops->write;
	cb_addr[KEDR_CB_CDEV_LLSEEK] = 	(unsigned long)fops->llseek;
	
	for (i = 0; i < KEDR_CB_CDEV_COUNT; ++i) {
		struct kedr_func_info *fi;
		if (cb_addr[i] == 0)
			continue;
		
		fi = kedr_find_func_info(cb_addr[i]);
		if (fi == NULL)
			continue;
		
		/* OK, found func_info for the callback. Check and set the
		 * handlers. Note that we do not change the handlers that
		 * are already set. */
		spin_lock_irqsave(&fi->handler_lock, flags);
		if (fi->pre_handler == NULL)
			rcu_assign_pointer(fi->pre_handler, pre_handler[i]);
		if (fi->post_handler == NULL)
			rcu_assign_pointer(fi->post_handler, post_handler[i]);
		spin_unlock_irqrestore(&fi->handler_lock, flags);
	}
}
/* ====================================================================== */

<$if concat(function.name)$><$block : join(\n\n)$>
<$endif$>/* ====================================================================== */

static struct kedr_fh_handlers *handlers[] = {
	<$if concat(handlerItem)$><$handlerItem: join(,\n\t)$>,
	<$endif$>NULL
};
/* ====================================================================== */

static void
on_exit_pre(struct kedr_fh_plugin *fh, struct module *mod)
{
	struct kedr_cdev_hb_id *pos;
	unsigned long tid;
	unsigned long pc;
	
	tid = kedr_get_thread_id();
	pc = (unsigned long)mod->exit;
	
	list_for_each_entry(pos, &cdev_hb_ids, list)
		trigger_end_exit_wait_events(tid, pc, pos);
}

static void
on_exit_post(struct kedr_fh_plugin *fh, struct module *mod)
{
	struct kedr_cdev_hb_id *pos;
	struct kedr_cdev_hb_id *tmp;
	
	list_for_each_entry_safe(pos, tmp, &cdev_hb_ids, list) {
		list_del(&pos->list);
		kfree(pos);
	}
}

static struct kedr_fh_plugin fh = {
	.owner = THIS_MODULE,
	.on_exit_pre = on_exit_pre,
	.on_exit_post = on_exit_post,
	
	.handlers = &handlers[0]
};
/* ====================================================================== */

static int __init
func_drd_init_module(void)
{
	return kedr_fh_plugin_register(&fh);	
}

static void __exit
func_drd_exit_module(void)
{
	kedr_fh_plugin_unregister(&fh);
}

module_init(func_drd_init_module);
module_exit(func_drd_exit_module);
/* ====================================================================== */
