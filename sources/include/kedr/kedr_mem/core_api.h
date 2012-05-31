/* core_api.h: API provided by the core of our system. */

#ifndef CORE_API_H_1049_INCLUDED
#define CORE_API_H_1049_INCLUDED

#include <kedr/object_types.h>

struct module;

/* The meaning of the arguments:
 *	eh - the pointer passed during registration
 *	target_module - the target module
 *	tid - ID of the relevant thread
 *	func - address of the original function (except in on_call_pre/post)
 *	data - additional pointer needed when recording memory events
 *	num_events - maximum number of the memory events that could happen
 *		in the block (the number of the actually happened events can
 * 		be smaller)
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
 * handler is not set. It is OK to leave some fields as NULL, 
 * except 'owner'.*/
struct kedr_event_handlers
{
	/* The module providing the handlers. */
	struct module *owner;
	
	/* Target module: loading and unloading 
	 * [NB] Unlike all other handlers, these two are executed in a 
	 * non-atomic context*/
	void (*on_target_loaded)(struct kedr_event_handlers *eh, 
		struct module *target_module);
	void (*on_target_about_to_unload)(struct kedr_event_handlers *eh, 
		struct module *target_module);
	
	/* [NB] The handlers listed below can be executed in atomic context,
	 * so they must be written appropriately. */
		
	/* Function entry & exit. The handlers are called just after the 
	 * entry and just before the exit, respectively. */
	void (*on_function_entry)(struct kedr_event_handlers *eh, 
		unsigned long tid, unsigned long func);
	void (*on_function_exit)(struct kedr_event_handlers *eh, 
		unsigned long tid, unsigned long func);
	
	/* Function call events. The handlers are called before and after 
	 * the instruction performing the function call, respectively. 
	 * In this case, 'pc' is the address of the corresponding 
	 * instruction in the original code. Note that it is not the 
	 * return address for that function call. 
	 * 'func' is the address of the function called.
	 * Unlike function entry/exit events, the call events are generated 
	 * both for the calls to the functions defined in the target module 
	 * and for the calls to the external functions. */
	void (*on_call_pre)(struct kedr_event_handlers *eh, 
		unsigned long tid, unsigned long pc, unsigned long func);
	void (*on_call_post)(struct kedr_event_handlers *eh, 
		unsigned long tid, unsigned long pc, unsigned long func);
	
	/* Memory events: reads, writes, updates
	 * These handlers are called as follows when the block of 
	 * operations ends:
	 * 	void *data = NULL;
	 *	begin_memory_events(eh, tid, num_events, &data);
	 *	for each event that could happen in the block {
	 *		on_memory_event(eh, tid, pc, addr, size, type,
	 *			data);
	 *	}
	 *	end_memory_events(eh, tid, data);
	 *
	 * Note that if an event did not actually happen, on_memory_event()
	 * will be called for it with 0 as 'addr'. This is makes the 
	 * implementation of the core a bit simpler. */
	void (*begin_memory_events)(struct kedr_event_handlers *eh, 
		unsigned long tid, unsigned long num_events, 
		void **pdata /* out param*/);
	void (*end_memory_events)(struct kedr_event_handlers *eh, 
		unsigned long tid, void *data);
	void (*on_memory_event)(struct kedr_event_handlers *eh, 
		unsigned long tid, 
		unsigned long pc, unsigned long addr, unsigned long size, 
		enum kedr_memory_event_type type,
		void *data);
	
	/* Memory barriers (pre & post handlers) */
	/* MB1: locked operations */
	void (*on_locked_op_pre)(struct kedr_event_handlers *eh, 
		unsigned long tid, unsigned long pc, 
		void **pdata);
	
	void (*on_locked_op_post)(struct kedr_event_handlers *eh, 
		unsigned long tid, unsigned long pc, 
		unsigned long addr, unsigned long size, 
		enum kedr_memory_event_type type, void *data);
	
	/* MB2: I/O operations that access memory */
	void (*on_io_mem_op_pre)(struct kedr_event_handlers *eh, 
		unsigned long tid, unsigned long pc, 
		void **pdata);

	void (*on_io_mem_op_post)(struct kedr_event_handlers *eh, 
		unsigned long tid, unsigned long pc, 
		unsigned long addr, unsigned long size, 
		enum kedr_memory_event_type type, void *data);
	
	/* MB3: Other kinds of memory barriers including I/O operations 
	 * that do not access memory */
	void (*on_memory_barrier_pre)(struct kedr_event_handlers *eh, 
		unsigned long tid, unsigned long pc, 
		enum kedr_barrier_type type);
	
	void (*on_memory_barrier_post)(struct kedr_event_handlers *eh, 
		unsigned long tid, unsigned long pc, 
		enum kedr_barrier_type type);
	
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
		unsigned long lock_id, enum kedr_lock_type type);
	void (*on_lock_post)(struct kedr_event_handlers *eh, 
		unsigned long tid, unsigned long pc, 
		unsigned long lock_id, enum kedr_lock_type type);

	void (*on_unlock_pre)(struct kedr_event_handlers *eh, 
		unsigned long tid, unsigned long pc, 
		unsigned long lock_id, enum kedr_lock_type type);
	void (*on_unlock_post)(struct kedr_event_handlers *eh, 
		unsigned long tid, unsigned long pc, 
		unsigned long lock_id, enum kedr_lock_type type);
	
	/* Signal/wait events */
	void (*on_signal_pre)(struct kedr_event_handlers *eh, 
		unsigned long tid, unsigned long pc, 
		unsigned long obj_id, enum kedr_sw_object_type type);
	void (*on_signal_post)(struct kedr_event_handlers *eh, 
		unsigned long tid, unsigned long pc, 
		unsigned long obj_id, enum kedr_sw_object_type type);
	void (*on_wait_pre)(struct kedr_event_handlers *eh, 
		unsigned long tid, unsigned long pc, 
		unsigned long obj_id, enum kedr_sw_object_type type);
	void (*on_wait_post)(struct kedr_event_handlers *eh, 
		unsigned long tid, unsigned long pc, 
		unsigned long obj_id, enum kedr_sw_object_type type);
	
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

/* Registers the set of event handlers with the core. 
 * Returns 0 on success, a negative error code on failure.
 *
 * [NB] The structure pointed to by 'eh' must live and remain the same until 
 * kedr_unregister_event_handlers() is called for it.
 *
 * No more than one set of handlers can be registered at any given moment. 
 * That is, it is not allowed to register a set of event handlers if some
 * set of event handlers is already registered. The function will return
 * -EINVAL in this case.
 * 
 * If some (or even all) handlers specified in 'eh' are NULL, this is OK. It
 * means that the provider of the handlers is not interested in handling the
 * respective events. 
 *
 * 'eh->owner' must be the module providing the handlers. 
 * 
 * [NB] The function cannot be called in atomic context. 
 * In addition, the function cannot be called while the target module is 
 * loaded. */
int 
kedr_register_event_handlers(struct kedr_event_handlers *eh);

/* Unregisters the event handlers. 'eh' should be the same pointer as it was 
 * passed to kedr_register_event_handlers(). 
 * 
 * [NB] The function cannot be called in atomic context. 
 * In addition, the function cannot be called while the target module is 
 * loaded. */
void 
kedr_unregister_event_handlers(struct kedr_event_handlers *eh);

/* Returns the current set of event handlers. It is only safe to call this 
 * function and use its result when the target is in memory and hence the 
 * provider of the event handlers is not unloadable. During that period, the 
 * handlers remain valid and do not change. */
struct kedr_event_handlers *
kedr_get_event_handlers(void);
/* ====================================================================== */

/* Creates an identifier which is guaranteed to be unique during a session
 * with the target module, i.e. from the moment the target module is about 
 * to begin its initialization to the moment when its cleanup is completed.
 * 
 * The function returns a non-zero value on success, 0 if the ID cannot be
 * obtained.
 * 
 * Normally, one would call this function when handling "module loaded" 
 * notification. 
 * 
 * Note that if the target is unloaded and then loaded again, the IDs 
 * should be requested again too. The ones obtained before (during the
 * previous session with the target) can no longer be used. 
 *
 * The function cannot be called from atomic context. */
unsigned long
kedr_get_unique_id(void);
/* ====================================================================== */

/* Returns the ID of the current thread. The caller should not rely on it
 * being some address or whatever, this is an implementation detail and is
 * subject to change. 
 * In addition to the "regular" threads, the function can be called in the 
 * interrupt service routines (ISRs). The IDs it returns for ISRs can never 
 * collide with the IDs it returns for the regular threads. */
unsigned long
kedr_get_thread_id(void);
/* ====================================================================== */

#endif /* CORE_API_H_1049_INCLUDED */
