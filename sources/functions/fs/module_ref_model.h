/* 
 * Declarations for module references model
 * (module_try_get(), module_put).
 * 
 * module_try_get() model is empty.
 * module_put() model is
 * 	`kedr_eh_on_signal(MODULE_MODEL_STATE_POST_INITIALIZED(m));`
 * 
 * At the start of module exit handler
 * 	`kedr_eh_on_wait(MODULE_MODEL_STATE_POST_INITIALIZED(m));`
 * should be executed.
 * 
 */

#ifndef MODULE_REF_MODEL_H
#define MODULE_REF_MODEL_H

/* 
 * Signal-wait id for module object.
 *
 * NOTE: Only 'POST' id is exist. 'PRE' id has no sence because
 * module is initialized before first instruction is executed.
 */

static inline unsigned long MODULE_MODEL_STATE_POST_INITIALIZED(
	struct module* mod)
{
	//TODO: field of the module structure should be used.
	return (unsigned long)mod;
}

#endif /* MODULE_REF_MODEL_H */