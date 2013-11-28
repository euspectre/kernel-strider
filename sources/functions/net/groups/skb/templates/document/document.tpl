/* ========================================================================
 * Copyright (C) 2013, ROSA Laboratory
 * Author: 
 *      Eugene A. Shatokhin
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published
 * by the Free Software Foundation.
 ======================================================================== */

#include <linux/kernel.h>
#include <linux/skbuff.h>
#include <linux/netdevice.h>
#include <linux/spinlock.h>
#include <asm/atomic.h>

#include <kedr/kedr_mem/core_api.h>
#include <kedr/kedr_mem/local_storage.h>
#include <kedr/kedr_mem/functions.h>
#include <kedr/object_types.h>
#include <kedr/fh_drd/common.h>

#include <util/fh_plugin.h>
#include <net/drd_net_common.h>

#include "config.h"
/* ====================================================================== */

/* Allocation and deallocation of struct sk_buff is tracked here, at least
 * partially. The memory blocks pointed to by the members of struct sk_buff
 * are not tracked, for simplicity. */
/* ====================================================================== */

static void
handle_alloc(struct kedr_local_storage *ls)
{
	struct kedr_call_info *info = (struct kedr_call_info *)(ls->info);
	unsigned long addr = KEDR_LS_RET_VAL(ls);

	if (addr != 0) {
		kedr_eh_on_alloc(ls->tid, info->pc, sizeof(struct sk_buff),
				 addr);
	}
}

static void
handle_kfree_plain(struct kedr_local_storage *ls, struct sk_buff *skb)
{
	struct kedr_call_info *info = (struct kedr_call_info *)(ls->info);
	unsigned long addr = (unsigned long)skb;

	if (addr != 0)
		kedr_eh_on_free(ls->tid, info->pc, addr);
}

static void
handle_kfree_skb(struct kedr_local_storage *ls, struct sk_buff *skb)
{
	struct kedr_call_info *info = (struct kedr_call_info *)(ls->info);
	unsigned long addr = (unsigned long)skb;

	/* To be exact, if the refcount is 1 here, it does not mean that the
	 * SKB is going to be destroyed. In theory, some other thread may
	 * increase it before the test in kfree_skb(), and it will not be
	 * freed then. I am not convinced that this is a good practice,
	 * to do such things with a doomed SKB though.
	 * Let's assume such things are rare, if exist at all. */
	if (skb == NULL || atomic_read(&skb->users) == 1)
		return;

	kedr_eh_on_free(ls->tid, info->pc, addr);
}

static void
handle_skb_queue_purge(struct kedr_local_storage *ls)
{
	/* skb_queue_purge() is equivalent to calling kfree_skb() for each
	 * SKB in the list. */
	struct sk_buff_head *list = (struct sk_buff_head *)KEDR_LS_ARG1(ls);
	struct sk_buff *skb = (struct sk_buff *)list;
	unsigned long flags;

	if (list == NULL)

	/* [NB] If someone removes the current skb from the list just
	 * after unlock but before the next lock operation, skb->next will
	 * be NULL and we will not process the rest of the list.
	 * Don't know yet how to avoid this properly. Locking the whole list
	 * rather than each element sounds like a bad idea.
	 * In the worst case, we'll miss some free() events. */
	while (skb != NULL) {
		spin_lock_irqsave(&list->lock, flags);
		skb = skb->next;
		if (skb == (struct sk_buff *)list) {
			skb = NULL;
		}
		else {
			handle_kfree_skb(ls, skb);
		}
		spin_unlock_irqrestore(&list->lock, flags);
	}
}

/* ====================================================================== */
<$if concat(function.name)$>
<$block : join(\n\n)$>
/* ====================================================================== */<$endif$>

static struct kedr_fh_handlers *handlers[] = {
	<$if concat(handlerItem)$><$handlerItem: join(,\n\t)$>,
	<$endif$>NULL
};
/* ====================================================================== */

static struct kedr_fh_group fh_group = {
	.handlers = NULL,
};

struct kedr_fh_group * __init
kedr_fh_get_group_skb(void)
{
	fh_group.handlers = &handlers[0];
	fh_group.num_handlers = (unsigned int)ARRAY_SIZE(handlers) - 1;
	return &fh_group;
}
/* ====================================================================== */
