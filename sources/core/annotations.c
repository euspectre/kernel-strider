/* annotations.c - definition of the annotation functions. 
 * These functions are exported but they do nothing themselves. Our system 
 * will intercept the calls to these functions. It is in the handlers,
 * where the real work is performed. */

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
#include <kedr/annotations.h>
/* ====================================================================== */

void 
kedr_annotate_happens_before(unsigned long obj)
{
	(void)obj;
}
EXPORT_SYMBOL(kedr_annotate_happens_before);

void 
kedr_annotate_happens_after(unsigned long obj)
{
	(void)obj;
}
EXPORT_SYMBOL(kedr_annotate_happens_after);
/* ================================================================ */
