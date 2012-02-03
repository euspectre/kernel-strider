/* core_impl.h - Implementation-specific declarations for the core. */

#ifndef CORE_IMPL_H_1628_INCLUDED
#define CORE_IMPL_H_1628_INCLUDED

#include <kedr/kedr_mem/core_api.h>
#include <kedr/kedr_mem/local_storage.h>
#include <kedr/asm/insn.h> /* instruction analysis facilities */

struct module;

/* The prefix for diagnostic messages. */
#define KEDR_MSG_PREFIX "[kedr_mem_core] "

/* The current set of event handlers. */
extern struct kedr_event_handlers *eh_current;

/* The current allocator for local storage instances */
extern struct kedr_ls_allocator *ls_allocator;

#endif // CORE_IMPL_H_1628_INCLUDED
