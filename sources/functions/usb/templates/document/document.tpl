/* ========================================================================
 * Copyright (C) 2015, Eugene Shatokhin <eugene.shatokhin@rosalab.ru>
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

#include <linux/usb.h>
#include <linux/slab.h>
#include <linux/spinlock.h>
#include <linux/rcupdate.h>

#include <kedr/kedr_mem/core_api.h>
#include <kedr/kedr_mem/local_storage.h>
#include <kedr/kedr_mem/functions.h>
#include <kedr/object_types.h>
#include <kedr/fh_drd/common.h>
/* ====================================================================== */

/* The following happens-before relations are expressed here.
 *
 * 1. Start of usb_register_driver() HB start of each callback from
 * struct usb_driver. ID: (ulong)usb_driver.
 *
 * 2. End of each callback from struct usb_driver HB end of usb_deregister().
 * ID: (ulong)usb_driver + 1
 *
 * 3. Start of probe() callback HB start of any other callback called for
 * a given interface (intf). ID: (ulong)intf.
 *
 * 4. End of disconnect() for a device with the given usb_device_id HB
 * start of probe() for a device with that usb_device_id. This is to handle
 * the case when a device is unplugged and then plugged in again. The
 * addresses of the kernel structures for that device (struct device,
 * struct usb_interface, ...) might be different now but the matched
 * usb_device_id should be the same. ID: (ulong)usb_device_id.
 *
 * 5. urb::complete() callback is executed in IRQ context.
 *
 * 6. Start of usb_submit_urb() HB start of urb::complete() callback for
 * that URB. ID: (ulong)urb.
 *
 * 7. End of urb::complete() callback for an URB HB end of:
 * 	- usb_kill_urb() for that URB;
 * 	- usb_poison_urb() for that URB;
 * 	- usb_kill_anchored_urbs() for each URB in the list;
 * 	- usb_poison_anchored_urbs() for each URB in the list.
 * ID: (ulong)urb + 1 */
/* ====================================================================== */

#define KEDR_MSG_PREFIX "[kedr_fh_drd_usb] "
/* ====================================================================== */

MODULE_AUTHOR("Eugene A. Shatokhin");
MODULE_LICENSE("GPL");
/* ====================================================================== */

/* Mapping {interface => device ID}. A simple list for now, can be optimized
 * (hash map, etc.) later if needed. First, to make it work. */
struct kedr_intf_to_id
{
	struct list_head list;
	struct usb_interface *intf;
	struct usb_device_id *id;
};

static LIST_HEAD(intf_to_id);
static DEFINE_SPINLOCK(intf_to_id_lock); /* Protects 'intf_to_id' list. */

/* Create the item for 'intf' and push it to the list, setting the
 * specified ID there. */
static int
intf_to_id_push(struct usb_interface *intf, struct usb_device_id *id)
{
	unsigned long flags;
	int ret = 0;
	struct kedr_intf_to_id *iti;

	spin_lock_irqsave(&intf_to_id_lock, flags);
	list_for_each_entry(iti, &intf_to_id, list) {
		if (iti->intf == intf) {
			/* May be not an error but needs attention. */
			pr_warning(KEDR_MSG_PREFIX
				"An item with intf=%p already exists.\n",
				intf);
			iti->id = id;
			spin_unlock_irqrestore(&intf_to_id_lock, flags);
			return 0;
		}
	}

	iti = kzalloc(sizeof(*iti), GFP_ATOMIC);
	if (iti) {
		iti->intf = intf;
		iti->id = id;
		list_add(&iti->list, &intf_to_id);
	}
	else {
		ret = -ENOMEM;
	}

	spin_unlock_irqrestore(&intf_to_id_lock, flags);
	return ret;
}

/* Find the item with the specified 'intf', remove it and return the ID it
 * contained. Returns NULL if not found. */
static struct usb_device_id *
intf_to_id_pop(struct usb_interface *intf)
{
	unsigned long flags;
	struct kedr_intf_to_id *iti;
	struct kedr_intf_to_id *tmp;
	struct usb_device_id *id = NULL;

	spin_lock_irqsave(&intf_to_id_lock, flags);
	list_for_each_entry_safe(iti, tmp, &intf_to_id, list) {
		if (iti->intf != intf)
			continue;
		id = iti->id;
		list_del(&iti->list);
		kfree(iti);
	}
	spin_unlock_irqrestore(&intf_to_id_lock, flags);

	if (!id) {
		/* May be not an error but needs attention. */
		pr_warning(KEDR_MSG_PREFIX
			"Unknown interface: intf=%p.\n",
			intf);
	}
	return id;
}

static void
intf_to_id_clear(void)
{
	unsigned long flags;
	struct kedr_intf_to_id *iti;
	struct kedr_intf_to_id *tmp;

	spin_lock_irqsave(&intf_to_id_lock, flags);
	list_for_each_entry_safe(iti, tmp, &intf_to_id, list) {
		list_del(&iti->list);
		kfree(iti);
	}
	spin_unlock_irqrestore(&intf_to_id_lock, flags);
}
/* ====================================================================== */

static void
common_pre(struct kedr_local_storage *ls, struct usb_interface *intf)
{
	unsigned long tid = ls->tid;
	unsigned long pc = ls->fi->addr;
	void *data;

	/* The callback handlers are already executed within RCU read-side
	 * section, so it is not needed to use rcu_read_lock/unlock here.
	 * rcu_dereference() IS needed, however. */
	data = rcu_dereference(ls->fi->data);
	if (!data) {
		pr_warning(KEDR_MSG_PREFIX
			"common_pre(): 'data' is NULL.\n");
		return;
	}

	/* Relation #1 */
	kedr_happens_after(tid, pc, (unsigned long)data);

	/* Relation #3 */
	kedr_happens_after(tid, pc, (unsigned long)intf);
}

static void
common_post(struct kedr_local_storage *ls, struct usb_interface *intf)
{
	unsigned long tid = ls->tid;
	unsigned long pc = ls->fi->addr;
	void *data;

	data = rcu_dereference(ls->fi->data);
	if (!data) {
		pr_warning(KEDR_MSG_PREFIX
			"common_post(): 'data' is NULL.\n");
		return;
	}

	/* Relation #2 */
	kedr_happens_before(tid, pc, (unsigned long)data + 1);
}
/* ====================================================================== */

static void
on_probe_pre(struct kedr_local_storage *ls)
{
	unsigned long tid = ls->tid;
	unsigned long pc = ls->fi->addr;
	void *data;
	struct usb_interface *intf =
		(struct usb_interface *)KEDR_LS_ARG1(ls);
	struct usb_device_id *id =
		(struct usb_device_id *)KEDR_LS_ARG2(ls);
	int ret;

	if (!intf) {
		pr_warning(KEDR_MSG_PREFIX
			"on_probe_pre(): 'intf' is NULL.\n");
		return;
	}

	if (!id) {
		pr_warning(KEDR_MSG_PREFIX
			"on_probe_pre(): 'id' is NULL.\n");
		return;
	}

	data = rcu_dereference(ls->fi->data);
	if (!data) {
		pr_warning(KEDR_MSG_PREFIX
			"on_probe_pre(): 'data' is NULL.\n");
		return;
	}

	/* Relation #1 */
	kedr_happens_after(tid, pc, (unsigned long)data);

	/* Relation #3 */
	kedr_happens_before(tid, pc, (unsigned long)intf);

	ret = intf_to_id_push(intf, id);
	if (ret) {
		pr_warning(KEDR_MSG_PREFIX
		"on_probe_pre(): Failed to store data, error: %d.\n",
			ret);
		return;
	}

	/* Relation #4 */
	kedr_happens_after(tid, pc, (unsigned long)id);
}

static void
on_probe_post(struct kedr_local_storage *ls)
{
	common_post(ls, (struct usb_interface *)KEDR_LS_ARG1(ls));

}

static void
on_disconnect_pre(struct kedr_local_storage *ls)
{
	common_pre(ls, (struct usb_interface *)KEDR_LS_ARG1(ls));
}

static void
on_disconnect_post(struct kedr_local_storage *ls)
{
	unsigned long tid = ls->tid;
	unsigned long pc = ls->fi->addr;
	struct usb_interface *intf =
		(struct usb_interface *)KEDR_LS_ARG1(ls);
	struct usb_device_id *id;

	common_post(ls, (struct usb_interface *)KEDR_LS_ARG1(ls));

	id = intf_to_id_pop(intf);
	if (!id)
		return;

	/* Relation #4 */
	kedr_happens_before(tid, pc, (unsigned long)id);
}

static void
on_unlocked_ioctl_pre(struct kedr_local_storage *ls)
{
	common_pre(ls, (struct usb_interface *)KEDR_LS_ARG1(ls));
}

static void
on_unlocked_ioctl_post(struct kedr_local_storage *ls)
{
	common_post(ls, (struct usb_interface *)KEDR_LS_ARG1(ls));
}

static void
on_suspend_pre(struct kedr_local_storage *ls)
{
	common_pre(ls, (struct usb_interface *)KEDR_LS_ARG1(ls));
}

static void
on_suspend_post(struct kedr_local_storage *ls)
{
	common_post(ls, (struct usb_interface *)KEDR_LS_ARG1(ls));
}

static void
on_resume_pre(struct kedr_local_storage *ls)
{
	common_pre(ls, (struct usb_interface *)KEDR_LS_ARG1(ls));
}

static void
on_resume_post(struct kedr_local_storage *ls)
{
	common_post(ls, (struct usb_interface *)KEDR_LS_ARG1(ls));
}

static void
on_reset_resume_pre(struct kedr_local_storage *ls)
{
	common_pre(ls, (struct usb_interface *)KEDR_LS_ARG1(ls));
}

static void
on_reset_resume_post(struct kedr_local_storage *ls)
{
	common_post(ls, (struct usb_interface *)KEDR_LS_ARG1(ls));
}

static void
on_pre_reset_pre(struct kedr_local_storage *ls)
{
	common_pre(ls, (struct usb_interface *)KEDR_LS_ARG1(ls));
}

static void
on_pre_reset_post(struct kedr_local_storage *ls)
{
	common_post(ls, (struct usb_interface *)KEDR_LS_ARG1(ls));
}

static void
on_post_reset_pre(struct kedr_local_storage *ls)
{
	common_pre(ls, (struct usb_interface *)KEDR_LS_ARG1(ls));
}

static void
on_post_reset_post(struct kedr_local_storage *ls)
{
	common_post(ls, (struct usb_interface *)KEDR_LS_ARG1(ls));
}
/* ====================================================================== */

/* If the callback 'cb' is set in the object 'obj', set the handlers for it
 * and use 'data' as the data. */
#define KEDR_SET_CB_HANDLERS(obj, cb, data) do { \
	if (obj->cb) \
		kedr_set_func_handlers( \
			obj->cb, on_ ## cb ## _pre, on_ ## cb ## _post, \
			(data), 0); \
} while(0)

static void
on_register(struct kedr_local_storage *ls, struct usb_driver *drv)
{
	struct kedr_call_info *info = (struct kedr_call_info *)(ls->info);

	if (!drv)
		return;

	/* Relation #1 */
	kedr_happens_before(ls->tid, info->pc, (unsigned long)drv);

	/* Callback handlers for struct usb_driver */
	KEDR_SET_CB_HANDLERS(drv, probe, (void *)drv);
	KEDR_SET_CB_HANDLERS(drv, disconnect, (void *)drv);
	KEDR_SET_CB_HANDLERS(drv, unlocked_ioctl, (void *)drv);
	KEDR_SET_CB_HANDLERS(drv, suspend, (void *)drv);
	KEDR_SET_CB_HANDLERS(drv, resume, (void *)drv);
	KEDR_SET_CB_HANDLERS(drv, reset_resume, (void *)drv);
	KEDR_SET_CB_HANDLERS(drv, pre_reset, (void *)drv);
	KEDR_SET_CB_HANDLERS(drv, post_reset, (void *)drv);
}

static void
on_deregister(struct kedr_local_storage *ls, struct usb_driver *drv)
{
	struct kedr_call_info *info = (struct kedr_call_info *)(ls->info);
	if (!drv)
		return;

	/* Relation #2 */
	kedr_happens_after(ls->tid, info->pc, (unsigned long)drv + 1);
}
/* ====================================================================== */

/* Handlers for 'complete' callbacks in the URBs. */
static void
complete_pre(struct kedr_local_storage *ls)
{
	struct urb *urb = (struct urb *)KEDR_LS_ARG1(ls);
	unsigned long func = ls->fi->addr;
	unsigned long tid = ls->tid;

	/* Relation #5 */
	kedr_irq_start(tid, func);

	/* Relation #6 */
	kedr_happens_after(tid, func, (unsigned long)urb);
}

static void
complete_post(struct kedr_local_storage *ls)
{
	struct urb *urb = (struct urb *)KEDR_LS_ARG1(ls);
	unsigned long func = ls->fi->addr;
	unsigned long tid = ls->tid;

	/* Relation #7 */
	kedr_happens_before(tid, func, (unsigned long)urb + 1);

	/* Relation #5 */
	kedr_irq_end(tid, func);
}
/* ====================================================================== */

static void
on_kill_urb(unsigned long tid, unsigned long pc, struct urb *urb)
{
	/* Relation #7 */
	kedr_happens_after(tid, pc, (unsigned long)urb + 1);
}

static void
on_kill_anchored_urbs(unsigned long tid, unsigned long pc,
		      struct usb_anchor *anchor)
{
	struct urb *urb = NULL;

	spin_lock_irq(&anchor->lock);
	list_for_each_entry(urb, &anchor->urb_list, anchor_list) {
		on_kill_urb(tid, pc, urb);
	}
	spin_unlock_irq(&anchor->lock);
}
/* ====================================================================== */

<$if concat(function.name)$><$block : join(\n\n)$>
<$endif$>/* ====================================================================== */

static struct kedr_fh_handlers *handlers[] = {
	<$if concat(handlerItem)$><$handlerItem: join(,\n\t)$>,
	<$endif$>NULL
};
/* ====================================================================== */

static struct kedr_fh_plugin fh = {
	.owner = THIS_MODULE,
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

	/* Just in case interface-to-device-id mapping is not empty yet. */
	intf_to_id_clear();
}

module_init(func_drd_init_module);
module_exit(func_drd_exit_module);
/* ====================================================================== */
