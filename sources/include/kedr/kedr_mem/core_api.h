/* core_api.h: API provided by the core of our system. */

#ifndef CORE_API_H_1049_INCLUDED
#define CORE_API_H_1049_INCLUDED

#include <linux/stddef.h>
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
	
	/* Session start and end events. All other events from the "analysis
	 * session" will be reported after "session start" event but before 
	 * "session end" event.
	 * 
	 * A session starts when a target module has just loaded but no 
	 * other targets are currently loaded. The session ends when a 
	 * target module is about to unload but no other targets are 
	 * currently loaded. A session can be viewed as a period when one or
	 * more target modules are in the memory.
	 * 
	 * [NB] These two handlers are executed in a non-atomic context. */
	void (*on_session_start)(struct kedr_event_handlers *eh);
	void (*on_session_end)(struct kedr_event_handlers *eh);
	
	/* Target module: loading and unloading 
	 * [NB] These two handlers are executed in a non-atomic context. */
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
	
	/* Alloc/free events
	 *
	 * [NB] If an allocation fails, on_alloc_post() will not be called
	 * for it. on_alloc_pre() is called anyway (if set), before the
	 * allocation. 
	 * The similar rules hold for on_lock_* and on_wait_*. That is,
	 * the pre handlers are called anyway, the post handlers - only if
	 * the relevant operation completes successfully. For example, if
	 * spin_trylock() fails, on_lock_post() should not be called for it.
	 * Same if something mutex_lock_interruptible() is interrupted. */
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
	
	/* Thread create / thread join events
	 * [NB] If thread creation has failed, on_thread_create_post() must
	 * be called with 0 as 'child_tid'. */
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

	/* Thread start / thread end events.
	 * "thread start" is generated for each new thread that enters one
	 * of the target modules.
	 * 'comm' - name of the thread (a null-terminated string). To be
	 * exact, it is &task_struct::comm[0] (see <linux/sched.h>).
	 *
	 * "thread end" is generated for the threads that executed the code
	 * of one or more target modules but have ended. 
	 * This event is generated only if the core is sure the thread has
	 * ended. So, not all "thread start" events may have matching
	 * "thread end" events. */
	void (*on_thread_start)(struct kedr_event_handlers *eh,
		unsigned long tid, const char *comm);
	void (*on_thread_end)(struct kedr_event_handlers *eh,
		unsigned long tid);
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

/* These functions should be used if it is needed to obtain the current set 
 * of handlers and call some of these handlers. The functions have no effect
 * if the corresponding handlers are not set. 
 *
 * It is not recommended to call the handlers directly in the new code. 
 * kedr_eh*() functions should be used instead. */

static inline void
kedr_eh_begin_memory_events(unsigned long tid, unsigned long num_events, 
	void **pdata /* out param*/)
{
	struct kedr_event_handlers *eh = kedr_get_event_handlers();
	if (eh->begin_memory_events != NULL)
		eh->begin_memory_events(eh, tid, num_events, pdata);
}

static inline void
kedr_eh_end_memory_events(unsigned long tid, void *data)
{
	struct kedr_event_handlers *eh = kedr_get_event_handlers();
	if (eh->end_memory_events != NULL)
		eh->end_memory_events(eh, tid, data);
}

void
kedr_eh_on_memory_event(unsigned long tid, unsigned long pc, 
	unsigned long addr, unsigned long size, 
	enum kedr_memory_event_type type,
	void *data);

/* A convenience function to report a single memory event. */
static inline void
kedr_eh_on_single_memory_event(unsigned long tid, unsigned long pc, 
	unsigned long addr, unsigned long size, 
	enum kedr_memory_event_type type)
{
	void *data = NULL;
	kedr_eh_begin_memory_events(tid, 1, &data);
	kedr_eh_on_memory_event(tid, pc, addr, size, type, data);
	kedr_eh_end_memory_events(tid, data);
}

static inline void
kedr_eh_on_alloc_pre(unsigned long tid, unsigned long pc, 
	unsigned long size)
{
	struct kedr_event_handlers *eh = kedr_get_event_handlers();
	if (eh->on_alloc_pre != NULL)
		eh->on_alloc_pre(eh, tid, pc, size);
}

static inline void
kedr_eh_on_alloc_post(unsigned long tid, unsigned long pc, 
	unsigned long size, unsigned long addr)
{
	struct kedr_event_handlers *eh = kedr_get_event_handlers();
	if (eh->on_alloc_post != NULL)
		eh->on_alloc_post(eh, tid, pc, size, addr);
}

/* "alloc pre" + "alloc post" */
static inline void
kedr_eh_on_alloc(unsigned long tid, unsigned long pc, unsigned long size, 
	unsigned long addr)
{
	kedr_eh_on_alloc_pre(tid, pc, size);
	kedr_eh_on_alloc_post(tid, pc, size, addr);
}

static inline void
kedr_eh_on_free_pre(unsigned long tid, unsigned long pc, 
	unsigned long addr)
{
	struct kedr_event_handlers *eh = kedr_get_event_handlers();
	if (eh->on_free_pre != NULL)
		eh->on_free_pre(eh, tid, pc, addr);
}

static inline void
kedr_eh_on_free_post(unsigned long tid, unsigned long pc, 
	unsigned long addr)
{
	struct kedr_event_handlers *eh = kedr_get_event_handlers();
	if (eh->on_free_post != NULL)
		eh->on_free_post(eh, tid, pc, addr);
}

/* "free pre" + "free post" */
static inline void
kedr_eh_on_free(unsigned long tid, unsigned long pc, unsigned long addr)
{
	kedr_eh_on_free_pre(tid, pc, addr);
	kedr_eh_on_free_post(tid, pc, addr);
}

static inline void
kedr_eh_on_lock_pre(unsigned long tid, unsigned long pc, 
	unsigned long lock_id, enum kedr_lock_type type)
{
	struct kedr_event_handlers *eh = kedr_get_event_handlers();
	if (eh->on_lock_pre != NULL)
		eh->on_lock_pre(eh, tid, pc, lock_id, type);
}

static inline void
kedr_eh_on_lock_post(unsigned long tid, unsigned long pc, 
	unsigned long lock_id, enum kedr_lock_type type)
{
	struct kedr_event_handlers *eh = kedr_get_event_handlers();
	if (eh->on_lock_post != NULL)
		eh->on_lock_post(eh, tid, pc, lock_id, type);
}

/* "lock pre" + "lock post" */
static inline void
kedr_eh_on_lock(unsigned long tid, unsigned long pc, unsigned long lock_id, 
	enum kedr_lock_type type)
{
	kedr_eh_on_lock_pre(tid, pc, lock_id, type);
	kedr_eh_on_lock_post(tid, pc, lock_id, type);
}

static inline void
kedr_eh_on_unlock_pre(unsigned long tid, unsigned long pc, 
	unsigned long lock_id, enum kedr_lock_type type)
{
	struct kedr_event_handlers *eh = kedr_get_event_handlers();
	if (eh->on_unlock_pre != NULL)
		eh->on_unlock_pre(eh, tid, pc, lock_id, type);
}

static inline void
kedr_eh_on_unlock_post(unsigned long tid, unsigned long pc, 
	unsigned long lock_id, enum kedr_lock_type type)
{
	struct kedr_event_handlers *eh = kedr_get_event_handlers();
	if (eh->on_unlock_post != NULL)
		eh->on_unlock_post(eh, tid, pc, lock_id, type);
}

/* "unlock pre" + "unlock post" */
static inline void
kedr_eh_on_unlock(unsigned long tid, unsigned long pc, 
	unsigned long lock_id, enum kedr_lock_type type)
{
	kedr_eh_on_unlock_pre(tid, pc, lock_id, type);
	kedr_eh_on_unlock_post(tid, pc, lock_id, type);
}

static inline void
kedr_eh_on_signal_pre(unsigned long tid, unsigned long pc, 
	unsigned long obj_id, enum kedr_sw_object_type type)
{
	struct kedr_event_handlers *eh = kedr_get_event_handlers();
	if (eh->on_signal_pre != NULL)
		eh->on_signal_pre(eh, tid, pc, obj_id, type);
}

static inline void
kedr_eh_on_signal_post(unsigned long tid, unsigned long pc, 
	unsigned long obj_id, enum kedr_sw_object_type type)
{
	struct kedr_event_handlers *eh = kedr_get_event_handlers();
	if (eh->on_signal_post != NULL)
		eh->on_signal_post(eh, tid, pc, obj_id, type);
}

/* "signal pre" + "signal post" */
static inline void
kedr_eh_on_signal(unsigned long tid, unsigned long pc, 
	unsigned long obj_id, enum kedr_sw_object_type type)
{
	kedr_eh_on_signal_pre(tid, pc, obj_id, type);
	kedr_eh_on_signal_post(tid, pc, obj_id, type);
}

static inline void
kedr_eh_on_wait_pre(unsigned long tid, unsigned long pc, 
	unsigned long obj_id, enum kedr_sw_object_type type)
{
	struct kedr_event_handlers *eh = kedr_get_event_handlers();
	if (eh->on_wait_pre != NULL)
		eh->on_wait_pre(eh, tid, pc, obj_id, type);
}

static inline void
kedr_eh_on_wait_post(unsigned long tid, unsigned long pc, 
	unsigned long obj_id, enum kedr_sw_object_type type)
{
	struct kedr_event_handlers *eh = kedr_get_event_handlers();
	if (eh->on_wait_post != NULL)
		eh->on_wait_post(eh, tid, pc, obj_id, type);
}

/* "wait pre" + "wait post" */
static inline void
kedr_eh_on_wait(unsigned long tid, unsigned long pc, unsigned long obj_id, 
	enum kedr_sw_object_type type)
{
	kedr_eh_on_wait_pre(tid, pc, obj_id, type);
	kedr_eh_on_wait_post(tid, pc, obj_id, type);
}

static inline void
kedr_eh_on_thread_create_pre(unsigned long tid, unsigned long pc)
{
	struct kedr_event_handlers *eh = kedr_get_event_handlers();
	if (eh->on_thread_create_pre != NULL)
		eh->on_thread_create_pre(eh, tid, pc);
}

static inline void
kedr_eh_on_thread_create_post(unsigned long tid, unsigned long pc, 
	unsigned long child_tid)
{
	struct kedr_event_handlers *eh = kedr_get_event_handlers();
	if (eh->on_thread_create_post != NULL)
		eh->on_thread_create_post(eh, tid, pc, child_tid);
}

static inline void
kedr_eh_on_thread_join_pre(unsigned long tid, unsigned long pc, 
	unsigned long child_tid)
{
	struct kedr_event_handlers *eh = kedr_get_event_handlers();
	if (eh->on_thread_join_pre != NULL)
		eh->on_thread_join_pre(eh, tid, pc, child_tid);
}

static inline void
kedr_eh_on_thread_join_post(unsigned long tid, unsigned long pc, 
	unsigned long child_tid)
{
	struct kedr_event_handlers *eh = kedr_get_event_handlers();
	if (eh->on_thread_join_post != NULL)
		eh->on_thread_join_post(eh, tid, pc, child_tid);
}

/* "thread join pre" + "thread join post" */
static inline void
kedr_eh_on_thread_join(unsigned long tid, unsigned long pc, 
	unsigned long child_tid)
{
	kedr_eh_on_thread_join_pre(tid, pc, child_tid);
	kedr_eh_on_thread_join_post(tid, pc, child_tid);
}

static inline void
kedr_eh_on_thread_start(unsigned long tid, const char *comm)
{
	struct kedr_event_handlers *eh = kedr_get_event_handlers();
	if (eh->on_thread_start != NULL)
		eh->on_thread_start(eh, tid, comm);
}

static inline void
kedr_eh_on_thread_end(unsigned long tid)
{
	struct kedr_event_handlers *eh = kedr_get_event_handlers();
	if (eh->on_thread_end != NULL)
		eh->on_thread_end(eh, tid);
}

/* Convenience functions to express happens-before relationships. */
static inline void
kedr_happens_before(unsigned long tid, unsigned long pc, unsigned long id)
{
	kedr_eh_on_signal(tid, pc, id, KEDR_SWT_COMMON);
}

static inline void
kedr_happens_after(unsigned long tid, unsigned long pc, unsigned long id)
{
	kedr_eh_on_wait(tid, pc, id, KEDR_SWT_COMMON);
}

/* ====================================================================== */

/* Creates an identifier which is guaranteed to be unique during a session
 * with the target module, i.e. from the moment the target module is about 
 * to begin its initialization to the moment when its cleanup is completed.
 * 
 * The function returns a non-zero value on success, 0 if the ID cannot be
 * obtained.
 * 
 * Note that if the target is unloaded and then loaded again, the IDs 
 * should be requested again too. The ones obtained before (during the
 * previous session with the target) can no longer be used. 
 *
 * The ID is an address of a dynamcally allocated object that is only 
 * deallocated when the target module has been unloaded. The ID is therefore 
 * guaranteed to differ from the addresses of other dynamically allocated 
 * objects. This can be helpful if one obtains some of the IDs using this
 * function and besides that uses the addresses of some objects (e.g. struct
 * file, struct device, etc.) as IDs too. The former group of IDs will never
 * collide with the latter.
 *
 * There is another consequence of the IDs being the addresses. The objects
 * they refer to are guaranteed to be at least sizeof(unsigned long) bytes
 * each. So kedr_get_unique_id() actually allows to obtain a group of IDs:
 * id, id + 1, ... id + sizeof(unsigned long) - 1, where 'id' is the return
 * value of the function.
 *
 * [NB] The function cannot be called from atomic context. */
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
