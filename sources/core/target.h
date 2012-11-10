/* target.h - definitions related to the target modules. */
#ifndef TARGET_H_1835_INCLUDED
#define TARGET_H_1835_INCLUDED

struct kedr_i13n;
struct module;

/* Everything needed for the analysis of a particular target module. */
struct kedr_target
{
	struct list_head list;
	
	/* Name of the target module. */
	char *name;
	
	/* The module itself, NULL if not loaded. */
	struct module *mod;
	
	/* The instrumentation object for the target module, NULL if the
	 * module is not loaded. */
	struct kedr_i13n *i13n;
};

/* Calls func(target, data) for each currently loaded (and instrumented) 
 * target module. 
 * If 'func' returns 0 for a target, kedr_for_each_loaded_target() continues
 * to the next target and if there are no more target - returns 0 (success).
 * If 'func' returns a positive value, kedr_for_each_loaded_target() does 
 * not process the remaining targets and returns 0 (also success). 
 * If 'func' returns a negative value it is considered an error code. In 
 * this case, kedr_for_each_loaded_target() does not process the remaining 
 * targets and returns that value indicating an error. 
 * 
 * The function must be called with 'session_mutex' locked. */
int
kedr_for_each_loaded_target(int (*func)(struct kedr_target *, void *), 
	void *data);

#endif /* TARGET_H_1835_INCLUDED */
