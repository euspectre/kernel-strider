/* i13n.h - definitions for the instrumentation ("i13n") subsystem */
#ifndef I13N_H_1122_INCLUDED
#define I13N_H_1122_INCLUDED

#include <linux/list.h>

struct module;

/* An instance of struct kedr_i13n contains everything related to the 
 * instrumentation of a particular kernel module ("instrumentation object"). 
 */
struct kedr_i13n
{
	/* The module to be instrumented. */
	struct module *target; 
	
	/* The list of the loaded ELF sections of the target. */
	struct list_head sections;

	/* The list of functions to be instrumented. */
	struct list_head ifuncs;

	/* Number of functions to be instrumented. */
	unsigned int num_ifuncs;
	
	/* "Detour" buffer for the target module. The instrumented code of
	 * the functions will be placed there. It is that code that will 
	 * actually be executed. A jump to the start of the instrumented
	 * function will be placed at the beginning of the original
	 * function, so the rest of the latter should never be executed. */
	void *detour_buffer; 

	/* Memory areas for fallback functions. A fallback function is a 
	 * copy of the original function relocated to the appropriate 
	 * position. It is called from the instrumented function if 
	 * allocation of the local storage fails. */
	void *fallback_init_area;
	void *fallback_core_area;
};

/* Create an instrumentation object for the given target module and 
 * instrument that module. The function returns the created instrumentation
 * object if successful, ERR_PTR(-errno) on failure. NULL is never returned.
 * Call this function after the target module has been loaded but before it
 * begins its initialization. 
 * Note that depending on the target module and on some other factors, the 
 * instrumentation can be quite a lengthy process. */
struct kedr_i13n *
kedr_i13n_process_module(struct module *target);

/* Cleanup the instrumentation object created by kedr_i13n_process_module().
 * Call this function when the instrumented instance of the target module 
 * is no longer needed. This is typically when the target module has 
 * finished cleaning up and is about to be unloaded. */
void
kedr_i13n_cleanup(struct kedr_i13n *i13n);

#endif /* I13N_H_1122_INCLUDED */
