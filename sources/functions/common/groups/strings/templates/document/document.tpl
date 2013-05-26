/* ========================================================================
 * Copyright (C) 2013, ROSA Laboratory
 * Copyright (C) 2012, KEDR development team
 * Authors: 
 *      Eugene A. Shatokhin
 *      Andrey V. Tsyvarev
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published
 * by the Free Software Foundation.
 ======================================================================== */

#include <linux/kernel.h>
#include <linux/string.h>
#include <linux/ctype.h> /* tolower() */
#include <linux/sort.h>
#include <linux/kallsyms.h>

#include <kedr/kedr_mem/core_api.h>
#include <kedr/kedr_mem/local_storage.h>
#include <kedr/kedr_mem/functions.h>
#include <kedr/object_types.h>

#include <util/fh_plugin.h>

#include "config.h"
/* ====================================================================== */

#define KEDR_MSG_PREFIX "[kedr_fh_drd_common] "
/* ====================================================================== */

/* A convenience function. */
static void
report_events_strsep(struct kedr_local_storage *ls)
{
	struct kedr_call_info *info = (struct kedr_call_info *)(ls->info);
	void *data = NULL;
	char **pstr = (char **)KEDR_LS_ARG1(ls);
	const char *seps = (const char *)KEDR_LS_ARG2(ls);
	char *sbegin = NULL;
	char *end = NULL;
	unsigned long size;
	unsigned long sep_len;
	
	/* First, strsep() reads the pointer the first argument
	 * points to. */
	kedr_eh_on_single_memory_event(ls->tid, info->pc, 
		(unsigned long)pstr, 
		(unsigned long)sizeof(char *), 
		KEDR_ET_MREAD);
	
	sbegin = *pstr;
	if (sbegin == NULL)
		goto out;
	
	sep_len = (unsigned long)strlen(seps);
	if (sep_len == 0)
		goto out;
	
	/* Report reading of the whole 'seps' string, including
	 * the terminating 0. */
	kedr_eh_on_single_memory_event(ls->tid, info->pc, 
		(unsigned long)seps, sep_len + 1, KEDR_ET_MREAD);
	
	end = strpbrk(sbegin, seps);
	if (end == NULL) {
		/* The whole 'sbegin' will be read; NULL will be
		 * written to *pstr. */
		size = (unsigned long)strlen(sbegin) + 1;
		kedr_eh_begin_memory_events(ls->tid, 2, &data);
		kedr_eh_on_memory_event(ls->tid, info->pc, 
			(unsigned long)sbegin, size,
			KEDR_ET_MREAD, data);
		kedr_eh_on_memory_event(ls->tid, info->pc, 
			(unsigned long)pstr, 
			(unsigned long)sizeof(char *),
			KEDR_ET_MWRITE, data);
		kedr_eh_end_memory_events(ls->tid, data);
	}
	else {
		/* The first 'size' bytes of 'sbegin' will be read;
		 * '\0' will be written to *end; end+1 will be 
		 * written to *pstr. */
		size = (unsigned long)(end - sbegin) + 1;
		kedr_eh_begin_memory_events(ls->tid, 3, &data);
		kedr_eh_on_memory_event(ls->tid, info->pc, 
			(unsigned long)sbegin, size,
			KEDR_ET_MREAD, data);
		kedr_eh_on_memory_event(ls->tid, info->pc, 
			(unsigned long)end, 1,
			KEDR_ET_MWRITE, data);
		kedr_eh_on_memory_event(ls->tid, info->pc, 
			(unsigned long)pstr, 
			(unsigned long)sizeof(char *),
			KEDR_ET_MWRITE, data);
		kedr_eh_end_memory_events(ls->tid, data);
	}
out:
	return;
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

/* For some of the functions we need to process, we cannot take the address
 * explicitly. For example, a compiler may not allow to use '&memset' if 
 * "memset" function, "memset" macro and/or "memset" builtin co-exist for
 * the sake of optimization, etc. However, to properly process the 
 * situations where the function is actually called, we need the address
 * of that function. This module will lookup the addresses of such 
 * functions via "kallsyms" during the initialization (see below). 
 * Note that the instances of "struct kedr_fh_drd_to_lookup" must not
 * be used after this module has completed its initialization. */
struct kedr_fh_drd_to_lookup
{
	const char *name;
	struct kedr_fh_handlers *h;
};

/* [NB] The last ("NULL") element is here for the array not to be empty */
static struct kedr_fh_drd_to_lookup __initdata to_lookup[] = {
<$lookupItem: join(\n)$>	{NULL, NULL} 
};
/* ====================================================================== */

/* A comparator, a swap function and a search function for 'to_lookup[]' 
 * array. Needed only during module init. */
static int __init
to_lookup_compare(const void *lhs, const void *rhs)
{
	const struct kedr_fh_drd_to_lookup *left = 
		(const struct kedr_fh_drd_to_lookup *)lhs;
	const struct kedr_fh_drd_to_lookup *right = 
		(const struct kedr_fh_drd_to_lookup *)rhs;
	int result;
	
	BUILD_BUG_ON(ARRAY_SIZE(to_lookup) < 1);
	
	result = strcmp(left->name, right->name);
	if (result > 0)
		return 1;
	else if (result < 0)
		return -1;
	return 0;
}

static void __init
to_lookup_swap(void *lhs, void *rhs, int size)
{
	struct kedr_fh_drd_to_lookup *left = 
		(struct kedr_fh_drd_to_lookup *)lhs;
	struct kedr_fh_drd_to_lookup *right = 
		(struct kedr_fh_drd_to_lookup *)rhs;
	struct kedr_fh_drd_to_lookup t;
	
	BUG_ON(size != (int)sizeof(struct kedr_fh_drd_to_lookup));
	
	t.name = left->name;
	t.h = left->h;
	
	left->name = right->name;
	left->h = right->h;
	
	right->name = t.name;
	right->h = t.h;
}

/* Checks if 'to_lookup[]' contains a record for a function with the given
 * name. Returns the pointer to the record if found, NULL otherwise. 
 * 'to_lookup[]' must be sorted by function name in ascending order by this
 *  time */
static struct kedr_fh_drd_to_lookup * __init
to_lookup_search(const char *name)
{
	size_t beg = 0;
	size_t end = ARRAY_SIZE(to_lookup) - 1;
		
	BUILD_BUG_ON(ARRAY_SIZE(to_lookup) < 1);
	
	while (beg < end) {
		int result;
		size_t mid = beg + (end - beg) / 2;

		result = strcmp(name, to_lookup[mid].name);
		if (result < 0)
			end = mid;
		else if (result > 0)
			beg = mid + 1;
		else 
			return &to_lookup[mid];
	}
	return NULL;
}
/* ====================================================================== */

/* This function will be called for each symbol known to the system to 
 * find the addresses of the functions listed in 'to_lookup[]'.
 *
 * If this function returns 0, kallsyms_on_each_symbol() will continue
 * walking the symbols. If non-zero - it will stop. */
static int __init
symbol_walk_callback(void *data, const char *name, struct module *mod, 
	unsigned long addr)
{
	struct kedr_fh_drd_to_lookup *item;
	
	/* Skip the symbol if it belongs to a module rather than to 
	 * the kernel proper. */
	if (mod != NULL) 
		return 0;
	
	item = to_lookup_search(name);
	if (item == NULL)
		return 0;
	
	item->h->orig = (void *)addr;
	return 0;
}
/* ====================================================================== */

static struct kedr_fh_group fh_group = {
	.handlers = NULL, /* Just to make sure all fields are zeroed. */
};

struct kedr_fh_group * __init
kedr_fh_get_group_strings(void)
{
	size_t size = ARRAY_SIZE(to_lookup) - 1;
	size_t i;
	int ret = 0;
	
	/* Sort 'to_lookup[]' array (except the "NULL" element) in 
	 * ascending order by the function names. */
	sort(&to_lookup[0], size, sizeof(struct kedr_fh_drd_to_lookup),
		to_lookup_compare, to_lookup_swap);
	
	ret = kallsyms_on_each_symbol(symbol_walk_callback, NULL);
	if (ret != 0) {
		pr_warning(KEDR_MSG_PREFIX 
			"kallsyms_on_each_symbol() exited with code %d",
				ret);
		goto fail;
	}
	
	/* Check that all the required addresses have been found. */
	for (i = 0; i < size; ++i) {
		if (to_lookup[i].h->orig == NULL) {
			pr_warning(KEDR_MSG_PREFIX 
			"Unable to find the address of function %s",
				to_lookup[i].name);
			goto fail;
		}
	}
	
	fh_group.handlers = &handlers[0];
	fh_group.num_handlers = (unsigned int)ARRAY_SIZE(handlers) - 1;
	
	return &fh_group;

fail:	
	/* The error has already been reported, "disable" the group by
	 * specifying 0 as the number of handlers and go on. */
	fh_group.handlers = &handlers[0];
	fh_group.num_handlers = 0;
	return &fh_group;
}
/* ====================================================================== */
