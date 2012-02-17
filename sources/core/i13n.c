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
/* ====================================================================== */

struct kedr_i13n *
kedr_i13n_process_module(struct module *target)
{
	struct kedr_i13n *i13n;
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
	
	// TODO: initialize other stuff
	// TODO: perform instrumentation
	// TODO: create detour buffer and deploy the code
	return i13n;
// TODO: more cleanup in error path as needed

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
