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
#include <linux/rcupdate.h>

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
	KEDR_CB_CDEV_COMPAT_IOCTL,
	KEDR_CB_CDEV_UNLOCKED_IOCTL,
	KEDR_CB_CDEV_MMAP,
	KEDR_CB_CDEV_POLL,
	
	/* Number of the known types, keep this item last. */
	KEDR_CB_CDEV_COUNT
};

/* IDs of the happens-before arcs for a given device. */
struct kedr_cdev_hb_id
{
	struct list_head list;
	dev_t devno;

	/* The target module the happens-before arcs are specified for. */
	struct module *target;
	
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
 * found, NULL if not.
 * The search is limited only to the ID corresponding to the given target
 * module. */
static struct kedr_cdev_hb_id *
find_ids_for_cdev(unsigned int mj, unsigned int mn, struct module *target)
{
	struct kedr_cdev_hb_id *pos;
	unsigned long flags;
	
	spin_lock_irqsave(&cdev_ids_lock, flags);
	list_for_each_entry(pos, &cdev_hb_ids, list) {
		if (pos->target != target)
			continue;
		
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
create_ids_for_cdev(unsigned int mj, unsigned int mn, struct module *target)
{
	struct kedr_cdev_hb_id *item;
	unsigned long flags;
	unsigned int i; 
	
	item = kzalloc(sizeof(*item), GFP_KERNEL);
	if (item == NULL)
		return NULL;

	item->target = target;
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
	 * more than once by the same target module, the item for the most
	 * recently created device will be found by find_ids_for_cdev()
	 * because list_add() adds it to the head of the list. */
	list_add(&item->list, &cdev_hb_ids);
	spin_unlock_irqrestore(&cdev_ids_lock, flags);
	return item;
}

/* Trigger an event for each possible type of a callback provided by a given
 * target module. */
static void
trigger_reg_start_signal_events(
	unsigned long tid, unsigned long pc, struct kedr_cdev_hb_id *ids,
	struct module *target)
{
	unsigned long id = 0;
	unsigned int cbtype;

	if (ids->target != target)
		return;
	
	for (cbtype = 0; cbtype < KEDR_CB_CDEV_COUNT; ++cbtype) {
		id = ids->ids_reg_start[cbtype];	
		kedr_happens_before(tid, pc, id);
	}
}

static void
trigger_end_exit_wait_events(
	unsigned long tid, unsigned long pc, struct kedr_cdev_hb_id *ids,
	struct module *target)
{
	unsigned long id = 0;
	unsigned int cbtype;

	if (ids->target != target)
		return;
	
	for (cbtype = 0; cbtype < KEDR_CB_CDEV_COUNT; ++cbtype) {
		id = ids->ids_end_exit[cbtype];	
		kedr_happens_after(tid, pc, id);
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
	       struct inode *inode, struct file *filp,
	       struct module *target)
{
	unsigned long id;
	struct kedr_cdev_hb_id *item;
	unsigned long tid;
	
	tid = kedr_get_thread_id();
	
	item = find_ids_for_cdev(imajor(inode), iminor(inode), target);
	if (item != NULL) {
		id = item->ids_reg_start[cb_type];
		kedr_happens_after(tid, pc, id);
	}
	else {
		pr_warning(KEDR_MSG_PREFIX 
		"fop_common_pre(): "
		"not found ID for the device (%u, %u) created by %s.\n",
			imajor(inode), iminor(inode), module_name(target));
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
		struct inode *inode, struct file *filp,
		struct module *target)
{
	unsigned long id;
	struct kedr_cdev_hb_id *item;
	unsigned long tid;
	
	tid = kedr_get_thread_id();
	
	/* Specify that the struct file instance pointed to by 'filp' is
	 * no longer available ("memory released"). */
	kedr_eh_on_free(tid, pc, (unsigned long)filp);
	
	/* HB relation */
	item = find_ids_for_cdev(imajor(inode), iminor(inode), target);
	if (item != NULL) {
		id = item->ids_end_exit[cb_type];
		kedr_happens_before(tid, pc, id);
	}
	else {
		pr_warning(KEDR_MSG_PREFIX 
	"fop_common_post(): "
	"not found ID for the device (%u, %u) created by %s.\n", 
			imajor(inode), iminor(inode), module_name(target));
	}
}

static void 
fop_open_pre(struct kedr_local_storage *ls)
{
	struct inode *inode;
	struct file *filp;
	inode = (struct inode *)KEDR_LS_ARG1(ls);
	filp = (struct file *)KEDR_LS_ARG2(ls);
	
	fop_common_pre(KEDR_CB_CDEV_OPEN, ls->fi->addr, inode, filp,
		       ls->fi->owner);
}

static void 
fop_open_post(struct kedr_local_storage *ls)
{
	struct inode *inode;
	struct file *filp;
	inode = (struct inode *)KEDR_LS_ARG1(ls);
	filp = (struct file *)KEDR_LS_ARG2(ls);
		
	fop_common_post(KEDR_CB_CDEV_OPEN, ls->fi->addr, inode, filp,
		       ls->fi->owner);
}

static void 
fop_release_pre(struct kedr_local_storage *ls)
{
	struct inode *inode;
	struct file *filp;
	inode = (struct inode *)KEDR_LS_ARG1(ls);
	filp = (struct file *)KEDR_LS_ARG2(ls);
	
	fop_common_pre(KEDR_CB_CDEV_RELEASE, ls->fi->addr, inode, filp,
		       ls->fi->owner);
}

static void 
fop_release_post(struct kedr_local_storage *ls)
{
	struct inode *inode;
	struct file *filp;
	inode = (struct inode *)KEDR_LS_ARG1(ls);
	filp = (struct file *)KEDR_LS_ARG2(ls);
	
	fop_common_post(KEDR_CB_CDEV_RELEASE, ls->fi->addr, inode, filp,
		       ls->fi->owner);
}

static void 
fop_read_pre(struct kedr_local_storage *ls)
{
	struct file *filp;
	filp = (struct file *)KEDR_LS_ARG1(ls);
	fop_common_pre(KEDR_CB_CDEV_READ, ls->fi->addr,
		       filp->f_path.dentry->d_inode, filp, ls->fi->owner);
}

static void 
fop_read_post(struct kedr_local_storage *ls)
{
	struct file *filp;
	filp = (struct file *)KEDR_LS_ARG1(ls);
	fop_common_post(KEDR_CB_CDEV_READ, ls->fi->addr,
			filp->f_path.dentry->d_inode, filp, ls->fi->owner);
}

static void 
fop_write_pre(struct kedr_local_storage *ls)
{
	struct file *filp;
	filp = (struct file *)KEDR_LS_ARG1(ls);
	fop_common_pre(KEDR_CB_CDEV_WRITE, ls->fi->addr,
		       filp->f_path.dentry->d_inode, filp, ls->fi->owner);
}

static void 
fop_write_post(struct kedr_local_storage *ls)
{
	struct file *filp;
	filp = (struct file *)KEDR_LS_ARG1(ls);
	fop_common_post(KEDR_CB_CDEV_WRITE, ls->fi->addr,
			filp->f_path.dentry->d_inode, filp, ls->fi->owner);
}

static void 
fop_llseek_pre(struct kedr_local_storage *ls)
{
	struct file *filp;
	filp = (struct file *)KEDR_LS_ARG1(ls);
	fop_common_pre(KEDR_CB_CDEV_LLSEEK, ls->fi->addr,
		       filp->f_path.dentry->d_inode, filp, ls->fi->owner);
}

static void 
fop_llseek_post(struct kedr_local_storage *ls)
{
	struct file *filp;
	filp = (struct file *)KEDR_LS_ARG1(ls);
	fop_common_post(KEDR_CB_CDEV_LLSEEK, ls->fi->addr,
			filp->f_path.dentry->d_inode, filp, ls->fi->owner);
}

static void
fop_compat_ioctl_pre(struct kedr_local_storage *ls)
{
	struct file *filp;
	filp = (struct file *)KEDR_LS_ARG1(ls);
	fop_common_pre(KEDR_CB_CDEV_COMPAT_IOCTL, ls->fi->addr,
		       filp->f_path.dentry->d_inode, filp, ls->fi->owner);
}

static void
fop_compat_ioctl_post(struct kedr_local_storage *ls)
{
	struct file *filp;
	filp = (struct file *)KEDR_LS_ARG1(ls);
	fop_common_post(KEDR_CB_CDEV_COMPAT_IOCTL, ls->fi->addr,
			filp->f_path.dentry->d_inode, filp, ls->fi->owner);
}

static void
fop_unlocked_ioctl_pre(struct kedr_local_storage *ls)
{
	struct file *filp;
	filp = (struct file *)KEDR_LS_ARG1(ls);
	fop_common_pre(KEDR_CB_CDEV_UNLOCKED_IOCTL, ls->fi->addr,
		       filp->f_path.dentry->d_inode, filp, ls->fi->owner);
}

static void
fop_unlocked_ioctl_post(struct kedr_local_storage *ls)
{
	struct file *filp;
	filp = (struct file *)KEDR_LS_ARG1(ls);
	fop_common_post(KEDR_CB_CDEV_UNLOCKED_IOCTL, ls->fi->addr,
			filp->f_path.dentry->d_inode, filp, ls->fi->owner);
}

static void
fop_mmap_pre(struct kedr_local_storage *ls)
{
	struct file *filp;
	filp = (struct file *)KEDR_LS_ARG1(ls);
	fop_common_pre(KEDR_CB_CDEV_MMAP, ls->fi->addr,
		       filp->f_path.dentry->d_inode, filp, ls->fi->owner);
}

static void
fop_mmap_post(struct kedr_local_storage *ls)
{
	struct file *filp;
	filp = (struct file *)KEDR_LS_ARG1(ls);
	fop_common_post(KEDR_CB_CDEV_MMAP, ls->fi->addr,
			filp->f_path.dentry->d_inode, filp, ls->fi->owner);
}

static void
fop_poll_pre(struct kedr_local_storage *ls)
{
	struct file *filp;
	filp = (struct file *)KEDR_LS_ARG1(ls);
	fop_common_pre(KEDR_CB_CDEV_POLL, ls->fi->addr,
		       filp->f_path.dentry->d_inode, filp, ls->fi->owner);
}

static void
fop_poll_post(struct kedr_local_storage *ls)
{
	struct file *filp;
	filp = (struct file *)KEDR_LS_ARG1(ls);
	fop_common_post(KEDR_CB_CDEV_POLL, ls->fi->addr,
			filp->f_path.dentry->d_inode, filp, ls->fi->owner);
}

/* [NB] Mind the order of the initializers. */
static void (*pre_handler[])(struct kedr_local_storage *ls) = {
	fop_open_pre,
	fop_release_pre,
	fop_read_pre,
	fop_write_pre,
	fop_llseek_pre,
	fop_compat_ioctl_pre,
	fop_unlocked_ioctl_pre,
	fop_mmap_pre,
	fop_poll_pre,
};
static void (*post_handler[])(struct kedr_local_storage *ls) = {
	fop_open_post,
	fop_release_post,
	fop_read_post,
	fop_write_post,
	fop_llseek_post,
	fop_compat_ioctl_post,
	fop_unlocked_ioctl_post,
	fop_mmap_post,
	fop_poll_post,
};

/* Checks if the callbacks in '*p' can be found in the target module and if
 * so, sets the handlers for them. If a handler is already set, the function
 * does not change it. */
static void
set_callback_handlers(struct cdev *p)
{
	void *cb_addr[KEDR_CB_CDEV_COUNT];
	const struct file_operations *fops;
	unsigned int i;
	
	if (p == NULL)
		return;
	fops = p->ops;
	
	memset(&cb_addr[0], 0, sizeof(cb_addr)); /* just in case */
	cb_addr[KEDR_CB_CDEV_OPEN] = 	fops->open;
	cb_addr[KEDR_CB_CDEV_RELEASE] = fops->release;
	cb_addr[KEDR_CB_CDEV_READ] = 	fops->read;
	cb_addr[KEDR_CB_CDEV_WRITE] = 	fops->write;
	cb_addr[KEDR_CB_CDEV_LLSEEK] = 	fops->llseek;
	cb_addr[KEDR_CB_CDEV_COMPAT_IOCTL] = 	fops->compat_ioctl;
	cb_addr[KEDR_CB_CDEV_UNLOCKED_IOCTL] = 	fops->unlocked_ioctl;
	cb_addr[KEDR_CB_CDEV_MMAP] = 	fops->mmap;
	cb_addr[KEDR_CB_CDEV_POLL] = 	fops->poll;
	
	for (i = 0; i < KEDR_CB_CDEV_COUNT; ++i) {
		if (cb_addr[i] == 0)
			continue;
		
		kedr_set_func_handlers(cb_addr[i], pre_handler[i], 
			post_handler[i], NULL, 0);
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
on_exit_pre(struct kedr_fh_plugin *fh, struct module *mod,
	    void **per_target)
{
	struct kedr_cdev_hb_id *pos;
	unsigned long tid;
	unsigned long pc;
	
	tid = kedr_get_thread_id();
	pc = (unsigned long)mod->exit;
	
	list_for_each_entry(pos, &cdev_hb_ids, list)
		trigger_end_exit_wait_events(tid, pc, pos, mod);
}

static void
on_exit_post(struct kedr_fh_plugin *fh, struct module *mod,
	     void ** per_target)
{
	struct kedr_cdev_hb_id *pos;
	struct kedr_cdev_hb_id *tmp;
	
	list_for_each_entry_safe(pos, tmp, &cdev_hb_ids, list) {
		if (pos->target != mod)
			continue;
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
