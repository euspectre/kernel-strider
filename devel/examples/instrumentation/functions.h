/* functions.h: declarations of the main operations with the functions in
 * the target module: enumeration, instrumentation, etc. */

#ifndef FUNCTIONS_H_1337_INCLUDED
#define FUNCTIONS_H_1337_INCLUDED

#include <linux/module.h>

/* Parameter of the module: name of the function in the target module to
 * process in a special way. */
extern char *target_function;

/* Initialize the function processing subsystem for the given target module. 
 * This function should be called from 'on_module_load' handler.
 * The function returns 0 on success, error code otherwise. */
int
kedr_init_function_subsystem(struct module *mod);

/* Finalize the function processing subsystem. 
 * This function should be called from 'on_module_unload' handler.*/
void
kedr_cleanup_function_subsystem(void);

/* Process the target module: load the list of its functions, instrument
 * them, etc. */
int 
kedr_process_target(struct module *target_module);

#endif // FUNCTIONS_H_1337_INCLUDED
