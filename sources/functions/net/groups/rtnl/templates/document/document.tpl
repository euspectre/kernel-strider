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
#include <linux/rtnetlink.h>

#include <kedr/kedr_mem/core_api.h>
#include <kedr/kedr_mem/local_storage.h>
#include <kedr/kedr_mem/functions.h>
#include <kedr/object_types.h>

#include <util/fh_plugin.h>
#include <net/drd_net_common.h>

#include "config.h"
/* ====================================================================== */

// TODO: Is the following right? May be - lock/unlock somehow still?
/* rtnl_lock() is handled in a pure happens-before mode, for simplicity,
 * "signal/wait" events are generated rather than "lock/unlock". This allows
 * to avoid problems with nested locking: assume a callback calls another
 * callback and there is a rule for both that they are called under
 * rtnl_lock(). If the second callback can also be called by the kernel
 * itself, it is hard to model rtnl_lock() for it: one needs to avoid
 * generating nested "lock/unlock" events for the lock (they may confuse the
 * offline race detector).
 *
 * Nested "signal/wait" pairs are legal, however, so a pure happens-before
 * approach is cleaner and easier to implement for this lock.
 *
 * 
 *
 * TODO
 */

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
kedr_fh_get_group_rtnl(void)
{
	fh_group.handlers = &handlers[0];
	fh_group.num_handlers = (unsigned int)ARRAY_SIZE(handlers) - 1;
	return &fh_group;
}
/* ====================================================================== */
