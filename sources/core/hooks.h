/* hooks.h - support for hooks in the core. The hooks, if set, are called at
 * the different stages of the instrumentation. They can be used for testing
 * and debugging as well as for other purposes. */

#ifndef HOOKS_H_1203_INCLUDED
#define HOOKS_H_1203_INCLUDED

struct kedr_i13n;
struct module;

/* A collection of callbacks to be called at the particular phases of the 
 * instrumentation. Each callback receives the pointer to kedr_core_hooks 
 * instance as the first argument, the same pointer as was used when 
 * setting the hooks. If this instance is a part of a larger structure, you
 * can use container_of() to get the ointer to that structure. 
 * 
 * Any callback can be NULL, which would mean "not set". 
 * 
 * Only one set of hooks can be active at a time. */
struct kedr_core_hooks
{
	/* The kernel module that provides the hooks. Most of the time, this
	 * field should be set to THIS_MODULE. */
	struct module *owner;
	
	/* Called after function lookup has completed. 'i13n' is the 
	 * corresponding instrumentation object, 'i13n->ifuncs' is the list 
	 * of kedr_ifunc instances for the functions. */
	void (*on_func_lookup_completed)(struct kedr_core_hooks *hooks,
		struct kedr_i13n *i13n);
		
	/* [NB] Add more hooks here as needed */
};

/* Set the core hooks. If 'hooks' is NULL, the hooks will be reset to their 
 * default. It is not allowed to change hooks if the target module is 
 * loaded. */
void
kedr_set_core_hooks(struct kedr_core_hooks *hooks);

#endif /* HOOKS_H_1203_INCLUDED */
