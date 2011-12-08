/* demo.h - demonstration of the instrumentation system. */

#ifndef DEMO_H_2106_INCLUDED
#define DEMO_H_2106_INCLUDED

struct module;

/* Initializes the demo operations system.
 * This function is intended to be called when the target module is loaded 
 * (typically, in on_load() handler). */
int
kedr_demo_init(struct module *mod);

/* Outputs the collected data to a file in debugfs and performs cleanup of 
 * the demo operations system. 
 * This function is intended to be called when the target module is about to 
 * unload (typically, in on_unload() handler). */
void
kedr_demo_fini(struct module *mod);
/* ====================================================================== */

/* The event handlers listed below can have the following values as their
 * arguments:
 * - tid - thread ID;
 * - func - address of the original function the event refers to (e.g. an 
 * entry to the instrumented instance of which function has been detected);
 * - pc - position in the original code the event refers to (e.g. which 
 * original instruction corresponds to the memory access event); 
 * - addr - start address of the accessed memory area;
 * - size - size of the accessed memory area, in bytes. */

/* Handles "function entry" event. */
void 
kedr_demo_on_function_entry(unsigned long tid, unsigned long func);

/* Handles "function exit" event. */
void 
kedr_demo_on_function_exit(unsigned long tid, unsigned long func);

/* Handles "read from memory" event. */
void 
kedr_demo_on_mem_read(unsigned long tid, unsigned long pc, 
	unsigned long addr, unsigned long size);

/* Handles "write to memory" event.
 * [NB] Normal (not locked) update should be reported as 2 events: read and
 * then write. */
void 
kedr_demo_on_mem_write(unsigned long tid, unsigned long pc, 
	unsigned long addr, unsigned long size);

/* Handles "locked update of memory" event. Update = read + write. */
void 
kedr_demo_on_mem_locked_update(unsigned long tid, unsigned long pc, 
	unsigned long addr, unsigned long size);
/* ====================================================================== */
#endif /* DEMO_H_2106_INCLUDED */
