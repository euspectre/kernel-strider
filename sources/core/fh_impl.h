/* fh_impl.h - basic operations needed to support function handling
 * plugins. */
#ifndef FH_IMPL_H_1920_INCLUDED
#define FH_IMPL_H_1920_INCLUDED

struct kedr_call_info;
struct kedr_fh_plugin;
struct module;

/* Unless specifically stated, the functions listed below must be called
 * with 'target_mutex' locked. */

/* These two functions implement registration/deregistration of the plugin.
 */
int
kedr_fh_plugin_register_impl(struct kedr_fh_plugin *fh);

void
kedr_fh_plugin_unregister_impl(struct kedr_fh_plugin *fh);

/* Calls try_module_get() for each module that provides a registered
 * FH plugin. Returns 0 if successful, a negative error code otherwise.
 * When it returns, either the refcounts for all the modules have been
 * incremented (on success) or are left unchanged (on failure). */
int
kedr_fh_plugins_get(void);

/* Calls module_put() for each module that provides a registered FH plugin.
 */
void
kedr_fh_plugins_put(void);

/* These two functions are called when the target module has just loaded and
 * when it is about to unload, respectively, and call appropriate handlers
 * provided by the plugins. */
void
kedr_fh_on_target_load(struct module *mod);

void
kedr_fh_on_target_unload(struct module *mod);

/* Handlers for "session start/end" events. They can be used to perform 
 * session-specific initialization (before the first event has been 
 * generated for the target module) and cleanup (after the last event has 
 * been generated for it). */
void 
kedr_fh_on_session_start(struct kedr_session *session);

void
kedr_fh_on_session_end(struct kedr_session *session);

/* If there are some handlers and/or a replacement function for the target
 * function (info->target), this function will set them in 'info'. Other 
 * fields of 'info' are left unchanged.
 * This function does not require 'target_mutex' to be locked. */
void 
kedr_fh_fill_call_info(struct kedr_call_info *info);

#endif /* FH_IMPL_H_1920_INCLUDED */
