/* A module to test support for dynamic annotations. */

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
#include <linux/kernel.h>

#define KEDR_ANNOTATIONS_ENABLED 1

#include "kedr_annotations.h"
/* ====================================================================== */

MODULE_AUTHOR("Eugene A. Shatokhin");
MODULE_LICENSE("GPL");
/* ====================================================================== */

static void __exit
test_cleanup_module(void)
{
}

static int __init
test_init_module(void)
{
#ifdef CONFIG_X86_64
	unsigned long id = 0xf1234567baadf00d;
	unsigned long addr = 0xaedf1234deadbeef;
#else /* CONFIG_X86_32 */
	unsigned long id = 0xbaadf00d;
	unsigned long addr = 0xdeadbeef;
#endif
	KEDR_ANNOTATE_HAPPENS_BEFORE(id);
	KEDR_ANNOTATE_HAPPENS_AFTER(id);
	KEDR_ANNOTATE_MEMORY_ACQUIRED((void *)addr, 18);
	KEDR_ANNOTATE_MEMORY_RELEASED((void *)addr);
	return 0;
}

module_init(test_init_module);
module_exit(test_cleanup_module);
/* ====================================================================== */
