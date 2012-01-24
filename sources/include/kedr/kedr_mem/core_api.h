#ifndef CORE_API_H_1049_INCLUDED
#define CORE_API_H_1049_INCLUDED

struct module;

enum kedr_memory_event_type {
	KEDR_ET_MREAD,		/* read from memory */
	KEDR_ET_MWRITE,		/* write to memory */
	KEDR_ET_MUPDATE		/* update of memory */
	/* [NB] Not necessarily locked update */
};

/* The meaning of the arguments:
 *	user_data - the pointer passed during registration
 *	target_module - the target module
 *	tid - ID of the relevant thread
 *	func - address if the original function
 *	data - additional pointer heeded when recording memory events
 *	num_events - number of the memory events to record
 *	pc - "program counter", the address in the original code 
 *		corresponding to the event
 *	addr - address of the affected (allocated, freed, read from or 
 *		written to) memory area
 *	size - size of the affected memory area, in bytes
 *	barrier_type - type of the memory barrier
 *	memory_event_type - type of the memory event: 
 *		see 'enum kedr_memory_event_type'
 *	lock_id - usually the address of the lock object/variable, or some
 *		other kind of ID of the lock object.
 *	lock_type - type of the lock.
 *	obj_id - ID of the signaled entity (e.g. memory address, etc.)
 *	obj_type - type of the synchronization object
 *	child_tid - ID of the thread being created / joined
 */

/* [NB] If a field of this structure is NULL, that means the corresponding
 *  handler is not set. It is OK to leave some fields as NULL. */
struct kedr_event_handlers
{
	/* Target module: loading and unloading 
	 * [NB] Unlike all other handlers, these two are executed in a 
	 * non-atomic context*/
	void (*on_target_loaded)(struct kedr_event_handlers *eh, 
		struct module *target_module);
	void (*on_target_about_to_unload)(struct kedr_event_handlers *eh, 
		struct module *target_module);
	
	/* [NB] The handlers listed below can be executed in atomic context,
	 * so they must be written appropriately. */
		
	/* Function entry & exit */
	void (*on_function_entry)(struct kedr_event_handlers *eh, 
		unsigned long tid, unsigned long func);
	void (*on_function_exit)(struct kedr_event_handlers *eh, 
		unsigned long tid, unsigned long func);
	
	/* Memory events: read & write 
	 * These handlers are called as follows (when the block of 
	 * operations ends):
	 * 	void *data = NULL;
	 *	begin_memory_events(eh, tid, num_events, &data);
	 *	for each event happened in the block {
	 *		on_memory_event(eh, tid, pc, addr, size, type,
	 *			data);
	 *	}
	 *	end_memory_events(eh, tid, data);
	 */
	void (*begin_memory_events)(struct kedr_event_handlers *eh, 
		unsigned long tid, unsigned long num_events, 
		void **pdata /* out param*/);
	void (*end_memory_events)(struct kedr_event_handlers *eh, 
		unsigned long tid, void *data);
	void (*on_memory_event)(struct kedr_event_handlers *eh, 
		unsigned long tid, 
		unsigned long pc, unsigned long addr, unsigned long size, 
		unsigned long memory_event_type,
		void *data);
	
	/* Memory barriers (pre & post handlers) */
	/* MB1: locked operations */
	void (*on_locked_op_pre)(struct kedr_event_handlers *eh, 
		unsigned long tid, unsigned long pc, 
		void **pdata);
	
	void (*on_locked_op_post)(struct kedr_event_handlers *eh, 
		unsigned long tid, unsigned long pc, 
		unsigned long addr, unsigned long size, 
		unsigned long memory_event_type, void *data);
	
	/* MB2: I/O operations that access memory */
	void (*on_io_mem_op_pre)(struct kedr_event_handlers *eh, 
		unsigned long tid, unsigned long pc, 
		void **pdata);

	void (*on_io_mem_op_post)(struct kedr_event_handlers *eh, 
		unsigned long tid, unsigned long pc, 
		unsigned long addr, unsigned long size, 
		unsigned long memory_event_type, void *data);
	
	/* MB3: Other kinds of memory barriers including I/O operations 
	 * that do not access memory */
	void (*on_memory_barrier_pre)(struct kedr_event_handlers *eh, 
		unsigned long tid, unsigned long pc, 
		unsigned long barrier_type);
	
	void (*on_memory_barrier_post)(struct kedr_event_handlers *eh, 
		unsigned long tid, unsigned long pc, 
		unsigned long barrier_type);
	
	/* Alloc/free events */
	void (*on_alloc_pre)(struct kedr_event_handlers *eh, 
		unsigned long tid, unsigned long pc, 
		unsigned long size);
	void (*on_alloc_post)(struct kedr_event_handlers *eh, 
		unsigned long tid, unsigned long pc, 
		unsigned long size, unsigned long addr);
	
	void (*on_free_pre)(struct kedr_event_handlers *eh, 
		unsigned long tid, unsigned long pc, 
		unsigned long addr);
	void (*on_free_post)(struct kedr_event_handlers *eh, 
		unsigned long tid, unsigned long pc, 
		unsigned long addr);
	
	/* Lock/unlock events */
	void (*on_lock_pre)(struct kedr_event_handlers *eh, 
		unsigned long tid, unsigned long pc, 
		unsigned long lock_id, unsigned long lock_type);
	void (*on_lock_post)(struct kedr_event_handlers *eh, 
		unsigned long tid, unsigned long pc, 
		unsigned long lock_id, unsigned long lock_type);

	void (*on_unlock_pre)(struct kedr_event_handlers *eh, 
		unsigned long tid, unsigned long pc, 
		unsigned long lock_id, unsigned long lock_type);
	void (*on_unlock_post)(struct kedr_event_handlers *eh, 
		unsigned long tid, unsigned long pc, 
		unsigned long lock_id, unsigned long lock_type);
	
	/* Signal/wait events */
	void (*on_signal_pre)(struct kedr_event_handlers *eh, 
		unsigned long tid, unsigned long pc, 
		unsigned long obj_id, unsigned long obj_type);
	void (*on_signal_post)(struct kedr_event_handlers *eh, 
		unsigned long tid, unsigned long pc, 
		unsigned long obj_id, unsigned long obj_type);
	void (*on_wait_pre)(struct kedr_event_handlers *eh, 
		unsigned long tid, unsigned long pc, 
		unsigned long obj_id, unsigned long obj_type);
	void (*on_wait_post)(struct kedr_event_handlers *eh, 
		unsigned long tid, unsigned long pc, 
		unsigned long obj_id, unsigned long obj_type);
	
	/* Thread create / thread join events */
	void (*on_thread_create_pre)(struct kedr_event_handlers *eh, 
		unsigned long tid, unsigned long pc);
	void (*on_thread_create_post)(struct kedr_event_handlers *eh, 
		unsigned long tid, unsigned long pc, 
		unsigned long child_tid);
	void (*on_thread_join_pre)(struct kedr_event_handlers *eh, 
		unsigned long tid, unsigned long pc, 
		unsigned long child_tid);
	void (*on_thread_join_post)(struct kedr_event_handlers *eh,
		unsigned long tid, unsigned long pc, 
		unsigned long child_tid);
};

/* Registers the output system with the core.
 * Returns 0 on success, a negative error code on failure. */
int 
kedr_register_output_system(struct kedr_event_handlers *eh);

/* Unregisters the output system. */
void 
kedr_unregister_output_system(struct kedr_event_handlers *eh);

#endif /* CORE_API_H_1049_INCLUDED */
