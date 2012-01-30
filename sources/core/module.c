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
#include <linux/slab.h>
#include <linux/mutex.h>

#include <kedr/kedr_mem/core_api.h>

#include "core_impl.h"

#include "config.h"
/* ====================================================================== */

MODULE_AUTHOR("Eugene A. Shatokhin");
MODULE_LICENSE("GPL");
/* ====================================================================== */

//#define KEDR_UMH_GET_SECTIONS KEDR_UM_HELPER_PATH "/kedr_get_sections.sh"
/* ====================================================================== */

static struct kedr_event_handlers *eh_default = NULL;

/* The current set of event handlers. If no set is registered, 'eh_current' 
 * must be the address of the default set, i.e. eh_current == eh_default. 
 * Except the initial assignment, all accesses to 'eh_current' pointer must
 * be protected with 'target_mutex'. This way, we make sure the instrumented 
 * code will see the set of handlers in a consistent state. 
 * 
 * Note that calling the handlers from '*eh_current' is expected to be done 
 * without locking 'target_mutex'. As long as the structure pointed to by
 * 'eh_current' stays unchanged since its registration till its 
 * de-registration, this makes no harm. Only the changes in the pointer 
 * itself must be protected. */
struct kedr_event_handlers *eh_current = NULL;

/* The module being analyzed. NULL if the module is not currently loaded. 
 * The accesses to this variable must be protected with 'target_mutex'. */
static struct module *target_module = NULL;

/* If nonzero, module load and unload notifications will be handled,
 * if 0, they will not. */
static int handle_module_notifications = 0;

/* A mutex to protect the data related to the target module. */
static DEFINE_MUTEX(target_mutex);
/* ====================================================================== */

/* Non-zero if some set of event handlers has already been registered, 
 * 0 otherwise. 
 * Must be called with 'target_mutex' locked. */
static int
event_handlers_registered(void)
{
	return (eh_current != eh_default);
}

static int 
target_module_loaded(void)
{
	return (target_module != NULL);
}

int 
kedr_register_event_handlers(struct kedr_event_handlers *eh)
{
	int ret = 0;
	BUG_ON(eh == NULL || eh->owner == NULL);
	
	if (mutex_lock_killable(&target_mutex) != 0) {
		pr_warning(KEDR_MSG_PREFIX 
		"kedr_register_event_handlers(): failed to lock mutex\n");
		return -EINTR;
	}
	
	if (target_module_loaded()) {
		pr_warning(KEDR_MSG_PREFIX 
		"Unable to register event handlers: target module is "
		"loaded\n");
		ret = -EBUSY;
		goto out_unlock;
	}
	
	if (event_handlers_registered()) {
		pr_warning(KEDR_MSG_PREFIX 
		"Attempt to register event handlers while some set of "
		"handlers is already registered\n");
		ret = -EINVAL;
		goto out_unlock;
	}
	
	eh_current = eh;
	mutex_unlock(&target_mutex);
	return 0; /* success */

out_unlock:
	mutex_unlock(&target_mutex);
	return ret;
}
EXPORT_SYMBOL(kedr_register_event_handlers);

void 
kedr_unregister_event_handlers(struct kedr_event_handlers *eh)
{
	BUG_ON(eh == NULL || eh->owner == NULL);
	
	if (mutex_lock_killable(&target_mutex) != 0) {
		pr_warning(KEDR_MSG_PREFIX 
		"kedr_unregister_event_handlers(): failed to lock mutex\n");
		return;
	}
	
	if (target_module_loaded()) {
		pr_warning(KEDR_MSG_PREFIX 
		"Unable to unregister event handlers: target module is "
		"loaded\n");
		goto out;
	}
	
	if (eh != eh_current) {
		pr_warning(KEDR_MSG_PREFIX 
		"Attempt to unregister event handlers that are not "
		"registered\n");
		goto out;
	}
	
	eh_current = eh_default;

out:	
	mutex_unlock(&target_mutex);
	return;
}
EXPORT_SYMBOL(kedr_unregister_event_handlers);
/* ====================================================================== */

static int __init
core_init_module(void)
{
	eh_default = (struct kedr_event_handlers *)kzalloc(
		sizeof(struct kedr_event_handlers), GFP_KERNEL);
	if (eh_default == NULL)
		return -ENOMEM;
	
	eh_default->owner = THIS_MODULE;
	/* Initialize 'eh_current' before registering with the notification 
	 * system. */
	eh_current = eh_default;
	
	// TODO: more initialization here
	

	return 0; /* success */
// TODO: put error handling code here, e.g. kfree(eh_default)
}

static void __exit
core_exit_module(void)
{
	// TODO: more cleanup here
	
	kfree(eh_default);
	return;
}

module_init(core_init_module);
module_exit(core_exit_module);
/* ================================================================ */
