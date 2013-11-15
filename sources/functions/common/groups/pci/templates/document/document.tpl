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
#include <linux/pci.h>
#include <linux/device.h>

#include <kedr/kedr_mem/core_api.h>
#include <kedr/kedr_mem/local_storage.h>
#include <kedr/kedr_mem/functions.h>
#include <kedr/fh_drd/common.h>
#include <kedr/object_types.h>

#include <util/fh_plugin.h>

#include "config.h"
/* ====================================================================== */

#define KEDR_MSG_PREFIX "[kedr_fh_drd_common] "
/* ====================================================================== */

/* The following happens-before relations are expressed here:
 *
 * 1. Start of __pci_register_driver() HB start of each callback. 
 *    ID: &pci_driver.
 * 
 * 2. End of each callback HB end of pci_unregister_driver().
 *    ID: &pci_driver + 1.
 *    [NB] This is needed in case the driver does not provide remove(). If 
 *    it provides it, a more strict relation (#4) will come into play.
 * 
 * 3. Start of probe() HB start of all other callbacks (incl. remove()).
 *    ID: &pci_dev.
 * 
 * 4. End of each callback except remove() HB end of remove().
 *    ID: &pci_dev + 1.
 *
 * 5. The callbacks from struct pci_driver are synchronized with dev_pm_ops
 *    callbacks, they cannot execute concurrently for a given device.
 *    They are actually executed under device_lock() mutex. Not sure about
 *    sriov_configure though.
 *    ID: &pci_dev.dev
 *
 * "Each callback" means each callback from struct pci_driver, struct
 * pci_error_handlers and struct dev_pm_ops. */
/* ====================================================================== */

/* [NB] Provided by "device" group. */
void
kedr_set_pm_ops_handlers(const struct dev_pm_ops *pm, 
	struct kedr_fh_drd_handlers *hs);

/* Provided by "pci_driver" subgroup. */
void 
kedr_set_pci_driver_handlers(const struct pci_driver *drv);

/* Provided by "pci_error_handlers" subgroup. */
void 
kedr_set_pci_error_handlers(const struct pci_error_handlers *errh);
/* ====================================================================== */

/* Handlers for PM operations. */
static void 
pm_op_pre(struct kedr_local_storage *ls, void *data)
{
	unsigned long tid = ls->tid;
	unsigned long pc = ls->fi->addr;
	struct device *dev = (struct device *)KEDR_LS_ARG1(ls);
	struct pci_dev *pdev;
	
	pdev = to_pci_dev(dev);

	/* Relation #3 */
	kedr_happens_after(tid, pc, (unsigned long)pdev);
	
	/* Relation #1 */
	kedr_happens_after(tid, pc, (unsigned long)(pdev->driver));
}

static void 
pm_op_post(struct kedr_local_storage *ls, void *data)
{
	unsigned long tid = ls->tid;
	unsigned long pc = ls->fi->addr;
	struct device *dev = (struct device *)KEDR_LS_ARG1(ls);
	struct pci_dev *pdev;
	
	pdev = to_pci_dev(dev);

	/* Relation #4 */
	kedr_happens_before(tid, pc, (unsigned long)pdev + 1);
	
	/* Relation #2 */
	kedr_happens_before(tid, pc, (unsigned long)(pdev->driver) + 1);
}

static struct kedr_fh_drd_handlers pm_op_handlers = {
	.pre = pm_op_pre,
	.post = pm_op_post,
	.data = NULL
};

static void
on_register(struct pci_driver *drv, unsigned long tid, unsigned long pc)
{
	/* Relation #1 */
	kedr_happens_before(tid, pc, (unsigned long)drv);
	
	if (drv->driver.pm)
		kedr_set_pm_ops_handlers(drv->driver.pm, &pm_op_handlers);
	
	kedr_set_pci_driver_handlers(drv);
	
	if (drv->err_handler)
		kedr_set_pci_error_handlers(drv->err_handler);
}

static void 
on_unregister(struct pci_driver *drv, unsigned long tid, unsigned long pc)
{
	/* Relation #2 */
	kedr_happens_after(tid, pc, (unsigned long)drv + 1);
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
	.handlers = NULL, /* Just to make sure all fields are zeroed. */
};

struct kedr_fh_group * __init
kedr_fh_get_group_pci(void)
{
	fh_group.handlers = &handlers[0];
	fh_group.num_handlers = (unsigned int)ARRAY_SIZE(handlers) - 1;
	
	return &fh_group;
}
/* ====================================================================== */
