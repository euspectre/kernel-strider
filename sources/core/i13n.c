/* i13n.c - the top-level component of the instrumentation subsystem. */

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
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/err.h>
#include <linux/slab.h>

#include "config.h"
#include "core_impl.h"

#include "sections.h"
#include "module_ms_alloc.h"
#include "i13n.h"
#include "ifunc.h"
#include "util.h"
/* ====================================================================== */

static void
free_fallback_areas(struct kedr_i13n *i13n)
{
	/* kedr_module_free(NULL) is a no-op anyway */
	kedr_module_free(i13n->fallback_init_area);
	i13n->fallback_init_area = NULL;
	kedr_module_free(i13n->fallback_core_area);
	i13n->fallback_core_area = NULL;
}

static int
alloc_fallback_areas(struct kedr_i13n *i13n)
{
	struct module *mod = i13n->target;
	/* Here we copy the code of the target module to some areas in the
	 * module mapping space. The functions contained there will be fixed
	 * up later and will serve as fallback functions in case something
	 * bad is detected by the instrumented code in runtime. For example,
	 * If the function call allocating the local storage fails, it is
	 * not an option to let the instrumented function continue. Calling
	 * BUG() is not quite user-friendly. So, in such situations, control
	 * will be transferred to a fallback instance of the original 
	 * function and it should execute as usual. 
	 * The original function itself will be modified, a jump to the 
	 * instrumented code will be placed at its beginning, so we cannot 
	 * let the control to pass to it. That's why we need these fallback
	 * instances.
	 * Note that after module loading notifications are handled, the
	 * module loader may make the code of the module read only, so we 
	 * cannot uninstrument it and pass control there in runtime either.
	 */
	if (mod->module_init != NULL) {
		i13n->fallback_init_area = kedr_module_alloc(
			mod->init_text_size);
		if (i13n->fallback_init_area == NULL)
			goto no_mem;
		
		memcpy(i13n->fallback_init_area, mod->module_init, 
			mod->init_text_size);
	}
	
	if (mod->module_core != NULL) {
		i13n->fallback_core_area = kedr_module_alloc(
			mod->core_text_size);
		if (i13n->fallback_core_area == NULL)
			goto no_mem;
		
		memcpy(i13n->fallback_core_area, mod->module_core,
			mod->core_text_size);
	}
	return 0; /* success */

no_mem:
	free_fallback_areas(i13n);	
	return -ENOMEM;
}
/* ====================================================================== */

// TODO

/* Creates an instrumented instance of function specified by 'func' and 
 * prepares the corresponding fallback function for later usage. Note that
 * this function does not prepare jump tables for the fallback instance. */
static int
do_process_function(struct kedr_ifunc *func, struct kedr_i13n *i13n)
{
	BUG_ON(func == NULL || func->addr == NULL);
	
	/* Small functions should have been removed from the list */
	BUG_ON(func->size < KEDR_SIZE_JMP_REL32);
	
	// TODO
	return 0;
}
/* ====================================================================== */

/* Computes the needed size of the detour buffer (the instrumented instances
 * of the functions must have been prepared by this time) and allocates the 
 * buffer. */
static int 
create_detour_buffer(struct kedr_i13n *i13n)
{
	// TODO
	return 0;
}
/* ====================================================================== */

/* Deploys the instrumented code of each function to an appropriate place in
 * the detour buffer. Releases the temporary buffer and sets 'i_addr' to the
 * final address of the instrumented instance. */
static void
deploy_instrumented_code(struct kedr_i13n *i13n)
{
	// TODO
}
/* ====================================================================== */

/* Fix up the jump tables for the given function so that the fallback 
 * instance could use them. */
static void
fixup_fallback_jump_tables(struct kedr_ifunc *func, struct kedr_i13n *i13n)
{
	// TODO
}
/* ====================================================================== */

/* For each original function, place a jump to the instrumented instance at
 * the beginning and fill the rest with '0xcc' (breakpoint) instructions. */
static void
detour_original_functions(struct kedr_i13n *i13n)
{
	// TODO
}
/* ====================================================================== */

struct kedr_i13n *
kedr_i13n_process_module(struct module *target)
{
	struct kedr_i13n *i13n;
	struct kedr_ifunc *func;
	int ret = 0;
	
	BUG_ON(target == NULL);
	
	i13n = kzalloc(sizeof(*i13n), GFP_KERNEL);
	if (i13n == NULL) 
		return ERR_PTR(-ENOMEM);
	
	i13n->target = target;
	INIT_LIST_HEAD(&i13n->sections);
	INIT_LIST_HEAD(&i13n->ifuncs);
	i13n->num_ifuncs = 0;
	
	i13n->detour_buffer = NULL; 
	i13n->fallback_init_area = NULL;
	i13n->fallback_core_area = NULL;
	
	ret = alloc_fallback_areas(i13n);
	if (ret != 0) {
		pr_warning(KEDR_MSG_PREFIX
	"Failed to allocate memory for fallback functions.\n");
		goto out;
	}
	
	ret = kedr_get_sections(target, &i13n->sections);
	if (ret != 0) {
		pr_warning(KEDR_MSG_PREFIX
	"Failed to obtain names and addresses of the target's sections.\n");
		goto out_free_fb;
	}
	
	ret = kedr_get_functions(i13n);
	if (ret != 0) {
		pr_warning(KEDR_MSG_PREFIX
	"Failed to prepare the list of functions to be processed.\n");
		goto out_free_sections;
	}
	/* If there are no instrumentable functions, nothing to do. */
	if (list_empty(&i13n->ifuncs)) 
		return i13n;
	
	list_for_each_entry(func, &i13n->ifuncs, list) {
		ret = do_process_function(func, i13n);
		if (ret != 0)
			goto out_free_functions;
	}
	
	/* Calculate the total size of the original functions and of their
	 * instrumented instances (for statistics). Both values are 
	 * initially 0. */
	list_for_each_entry(func, &i13n->ifuncs, list) {
		i13n->total_size += func->size;
		i13n->total_i_size += func->i_size;
	}
	pr_info(KEDR_MSG_PREFIX "Total size of the functions before "
		"instrumentation (bytes): %lu, after: %lu\n",
		i13n->total_size, i13n->total_i_size);
	
	ret = create_detour_buffer(i13n);
	if (ret != 0)
		goto out_free_functions;

	deploy_instrumented_code(i13n);

	/* At this point, nothing more should fail, so we can finally 
	 * fixup the jump tables to be applicable for the fallback instances
	 * rather than for the original one. */
	list_for_each_entry(func, &i13n->ifuncs, list)
		fixup_fallback_jump_tables(func, i13n);
	
	detour_original_functions(i13n);
	return i13n;

out_free_functions:
	kedr_release_functions(i13n);
out_free_sections:
	kedr_release_sections(&i13n->sections);
out_free_fb:
	free_fallback_areas(i13n);
out:
	kedr_module_free(i13n->detour_buffer); /* just in case */
	kfree(i13n);
	return ERR_PTR(ret);
}

void
kedr_i13n_cleanup(struct kedr_i13n *i13n)
{
	BUG_ON(i13n == NULL);
	
	// TODO: more cleanup here
	
	kedr_release_functions(i13n);
	kedr_module_free(i13n->detour_buffer);
	i13n->detour_buffer = NULL;
	
	kedr_release_sections(&i13n->sections);
	free_fallback_areas(i13n);
	kfree(i13n);
}
