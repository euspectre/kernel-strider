/* module_ms_alloc.c - API to allocate and deallocate memory in the
 * module mapping space. */

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
#include <linux/version.h>
#include <linux/kallsyms.h>
#include <linux/errno.h>
#include <linux/string.h>

#include "core_impl.h"
#include "module_ms_alloc.h"

/* ====================================================================== */
/* It is needed to allocate memory close enough to the areas occupied by  
 * the kernel modules (within +/- 2Gb). Otherwise, RIP-relative addressing 
 * could be a problem on x86-64. It is used, for example, when the module 
 * accesses its global data. 
 *
 * For now, I cannot see a good way to ensure the memory is allocated 
 * properly. 
 * It seems from the memory layout (Documentation/x86/x86_64/mm.txt) that 
 * the only way is to use memory mapped to exactly the same region of 
 * addresses where the modules reside. The most clear way I currently see is
 * to use module_alloc() like the module loader and kernel probes do. 
 * 
 * Of course, that function is not exported and was never meant to. I look
 * for its address via kallsyms subsystem and use this address then. This
 * is an "ugly hack" and will definitely be frowned upon by kernel 
 * developers. 
 * I hope I will find a better way in the future. */

void *(*module_alloc_func)(unsigned long) = NULL;

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3, 19, 0))
void (*module_memfree_func)(void *) = NULL;
#else
void (*module_free_func)(struct module *, void *) = NULL;
#endif
/* ====================================================================== */

/* This function will be called for each symbol known to the system.
 * We need to find only the particular functions.
 *
 * If this function returns 0, kallsyms_on_each_symbol() will continue
 * walking the symbols. If non-zero - it will stop. */
static int
symbol_walk_callback(void *data, const char *name, struct module *mod, 
	unsigned long addr)
{
	/* Skip the symbol if it belongs to a module rather than to 
	 * the kernel proper. */
	if (mod != NULL) 
		return 0;
	
	if (strcmp(name, "module_alloc") == 0) {
		if (module_alloc_func != NULL) {
			pr_warning(KEDR_MSG_PREFIX
"Found two \"module_alloc\" symbols in the kernel, unable to continue\n");
			return -EFAULT;
		}
		module_alloc_func = (typeof(module_alloc_func))addr;
		return 0;
	}
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3, 19, 0))
	if (strcmp(name, "module_memfree") == 0) {
		if (module_memfree_func != NULL) {
			pr_warning(KEDR_MSG_PREFIX
"Found two \"module_memfree\" symbols in the kernel, unable to continue\n");
			return -EFAULT;
		}
		module_memfree_func = (typeof(module_memfree_func))addr;
	}
#else
	if (strcmp(name, "module_free") == 0) {
		if (module_free_func != NULL) {
			pr_warning(KEDR_MSG_PREFIX
"Found two \"module_free\" symbols in the kernel, unable to continue\n");
			return -EFAULT;
		}
		module_free_func = (typeof(module_free_func))addr;
	}
#endif
	return 0;
}

/* ====================================================================== */
int
kedr_init_module_ms_alloc(void)
{
	int ret = kallsyms_on_each_symbol(symbol_walk_callback, NULL);
	if (ret)
		return ret;
	
	if (module_alloc_func == NULL) {
		pr_warning(KEDR_MSG_PREFIX
		"Unable to find \"module_alloc\" function\n");
		return -EFAULT;
	}

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3, 19, 0))
	if (module_memfree_func == NULL) {
		pr_warning(KEDR_MSG_PREFIX
		"Unable to find \"module_memfree\" function.\n");
		return -EFAULT;
	}
#else
	if (module_free_func == NULL) {
		pr_warning(KEDR_MSG_PREFIX
		"Unable to find \"module_free\" function.\n");
		return -EFAULT;
	}
#endif
	return 0; /* success */
}

void
kedr_cleanup_module_ms_alloc(void)
{
	module_alloc_func = NULL;
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3, 19, 0))
	module_memfree_func = NULL;
#else
	module_free_func = NULL;
#endif
}

void *
kedr_module_alloc(unsigned long size)
{
	BUG_ON(module_alloc_func == NULL);
	return module_alloc_func(size);
}

void 
kedr_module_free(void *buf)
{
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3, 19, 0))
	BUG_ON(module_memfree_func == NULL);
	if (buf != NULL)
		module_memfree_func(buf);
#else
	BUG_ON(module_free_func == NULL);
	if (buf != NULL)
		module_free_func(NULL, buf);
#endif
}
/* ====================================================================== */
