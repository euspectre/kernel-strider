/* module.c - initialization, cleanup, parameters and other common stuff. */

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
#include <linux/moduleparam.h>
#include <linux/init.h>

#include <linux/kernel.h>
#include <linux/errno.h>

#include <kedr/kedr_mem/core_api.h>

#include "config.h"

MODULE_AUTHOR("Eugene A. Shatokhin");
MODULE_LICENSE("GPL");

#define KEDR_UMH_GET_SECTIONS KEDR_UM_HELPER_PATH "/kedr_get_sections.sh"

static int __init
core_init_module(void)
{
	// TODO	
	
	//<>
	pr_info("[DBG] The user-mode helper: %s\n", KEDR_UMH_GET_SECTIONS);
	//<>
	return 0; /* success */
}

static void __exit
core_exit_module(void)
{
	// TODO
	return;
}

module_init(core_init_module);
module_exit(core_exit_module);
/* ================================================================ */
