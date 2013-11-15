/* Handling of the callbacks from struct pci_driver is performed here. */

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

#include <kedr/kedr_mem/core_api.h>
#include <kedr/kedr_mem/local_storage.h>
#include <kedr/kedr_mem/functions.h>
#include <kedr/fh_drd/common.h>

#include "config.h"
/* ====================================================================== */

/* A common pre handler. */
static void
handle_pre_common(struct kedr_local_storage *ls)
{
	struct pci_dev *dev = (struct pci_dev *)KEDR_LS_ARG1(ls);
	unsigned long tid = ls->tid;
	unsigned long pc = ls->fi->addr;
	
	/* Relation #1 */
	kedr_happens_after(tid, pc, (unsigned long)(dev->driver));
	
	/* Relation #3 */
	kedr_happens_after(tid, pc, (unsigned long)dev);

	/* Relation #5 */
	kedr_happens_after(tid, pc, (unsigned long)(&dev->dev));
}

/* Pre handler for probe() */
static void
handle_pre_probe(struct kedr_local_storage *ls)
{
	struct pci_dev *dev = (struct pci_dev *)KEDR_LS_ARG1(ls);
	unsigned long tid = ls->tid;
	unsigned long pc = ls->fi->addr;
	
	/* Relation #1 */
	kedr_happens_after(tid, pc, (unsigned long)(dev->driver));
	
	/* Relation #3 */
	kedr_happens_before(tid, pc, (unsigned long)dev);

	/* Relation #5 */
	kedr_happens_after(tid, pc, (unsigned long)(&dev->dev));
}

/* A common post handler. */
static void
handle_post_common(struct kedr_local_storage *ls)
{
	struct pci_dev *dev = (struct pci_dev *)KEDR_LS_ARG1(ls);
	unsigned long tid = ls->tid;
	unsigned long pc = ls->fi->addr;
	
	/* Relation #2 */
	kedr_happens_before(tid, pc, (unsigned long)(dev->driver) + 1);
	
	/* Relation #4 */
	kedr_happens_before(tid, pc, (unsigned long)dev + 1);

	/* Relation #5 */
	kedr_happens_before(tid, pc, (unsigned long)(&dev->dev));
}

/* Post handler for remove(). */
static void
handle_post_remove(struct kedr_local_storage *ls)
{
	struct pci_dev *dev = (struct pci_dev *)KEDR_LS_ARG1(ls);
	unsigned long tid = ls->tid;
	unsigned long pc = ls->fi->addr;
		
	/* Relation #2 */
	kedr_happens_before(tid, pc, (unsigned long)(dev->driver) + 1);
	
	/* Relation #4 */
	kedr_happens_after(tid, pc, (unsigned long)dev + 1);

	/* Relation #5 */
	kedr_happens_before(tid, pc, (unsigned long)(&dev->dev));
}
/* ====================================================================== */
<$if concat(function.name)$>
<$block : join(\n\n)$>
/* ====================================================================== */<$endif$>

void 
kedr_set_pci_driver_handlers(const struct pci_driver *drv)
{
<$set_handlers : join(\n)$>}
/* ====================================================================== */
