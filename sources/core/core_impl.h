/* core_impl.h - Implementation-specific declarations for the core. */

#ifndef CORE_IMPL_H_1628_INCLUDED
#define CORE_IMPL_H_1628_INCLUDED

#include <kedr/kedr_mem/core_api.h>

struct module;

/* The prefix for diagnostic messages. */
#define KEDR_MSG_PREFIX "[kedr_mem_core] "

/* The current set of event handlers. */
extern struct kedr_event_handlers *eh_current;

/* The kernel module under analysis (NULL if not loaded). */
//extern struct module *target_module;

#endif // CORE_IMPL_H_1628_INCLUDED
