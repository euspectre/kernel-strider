/*
 * API for recording events concerning synchronization issues
 * (memory reads, writes, lockings, allocations and other events, needed
 * for detecting data races).
 */

#ifndef EVENT_COLLECTOR_H
#define EVENT_COLLECTOR_H

typedef unsigned long tid_t;
typedef unsigned long addr_t;

#include <linux/module.h>

#include "kedr/object_types.h"

struct execution_event_collector;

/*
 * Begin to record information about series of memory accesses.
 * All recorded memory accesses will share same thread and timestamp.
 * 
 * key is returned by the function and should be used in next functions.
 */
void execution_event_memory_accesses_begin(
    struct execution_event_collector* collector,
    tid_t tid, int n_accesses, void** key);

/*
 * Record information about next memory access.
 */
void execution_event_memory_access_next(
    struct execution_event_collector* collector,
    void* key,
    addr_t pc,
    addr_t addr, unsigned long size,
    enum kedr_memory_event_type type);

/*
 * End writting series of memory accesses.
 * key become invalid after this function call.
 */
void execution_event_memory_accesses_end(
    struct execution_event_collector* collector,
    void* key);

/* Shortcat for write one event access */
void execution_event_memory_access_one(
    struct execution_event_collector* collector,
    tid_t tid, addr_t pc,
    addr_t addr, unsigned long size, enum kedr_memory_event_type type);

/*
 * Record information about locked memory access(update).
 */
void execution_event_locked_memory_access(
    struct execution_event_collector* collector,
    tid_t tid, addr_t pc,
    addr_t addr, unsigned long size);

/*
 * Record information about memory barrier(read, write of full).
 */
void execution_event_memory_barrier(
    struct execution_event_collector* collector,
    tid_t tid, addr_t pc,
    enum kedr_barrier_type type);

/*
 * Record information about alloc and free operations.
 */
void execution_event_alloc(
    struct execution_event_collector* collector,
    tid_t tid, addr_t pc,
    unsigned long size, addr_t pointer_returned);

void execution_event_free(
    struct execution_event_collector* collector,
    tid_t tid, addr_t pc,
    addr_t pointer_freed);

/*
 * Record information about (un)lock operation.
 */
void execution_event_lock(
    struct execution_event_collector* collector,
    tid_t tid, addr_t pc,
    addr_t lock_object, enum kedr_lock_type type);

void execution_event_unlock(
    struct execution_event_collector* collector,
    tid_t tid, addr_t pc,
    addr_t lock_object, enum kedr_lock_type type);


/*
 * Record information about signal and wait operations.
 */
void execution_event_signal(
    struct execution_event_collector* collector,
    tid_t tid, addr_t pc,
    addr_t wait_object, enum kedr_sw_object_type type);

void execution_event_wait(
    struct execution_event_collector* collector,
    tid_t tid, addr_t pc,
    addr_t wait_object, enum kedr_sw_object_type type);


/*
 * Record information about thread creation/joining operations.
 */
void execution_event_thread_create(
    struct execution_event_collector* collector,
    tid_t tid, addr_t pc,
    tid_t child_tid);

void execution_event_thread_join(
    struct execution_event_collector* collector,
    tid_t tid, addr_t pc,
    tid_t child_tid);

/*
 * Record information about function entry/exit.
 */
void execution_event_function_entry(
    struct execution_event_collector* collector,
    tid_t tid, addr_t func);

void execution_event_function_exit(
    struct execution_event_collector* collector,
    tid_t tid, addr_t func);

/*
 * Record information about function call(before or after).
 */
void execution_event_function_call_pre(
    struct execution_event_collector* collector,
    tid_t tid, addr_t pc, addr_t func);

void execution_event_function_call_post(
    struct execution_event_collector* collector,
    tid_t tid, addr_t pc, addr_t func);

/***********************  Current collector ***************************/
/*
 * In the current implementation there is at most one event collector
 * at a time. This collector is exported via global variable, and
 * for every event there is a construction, which write this event into
 * current collector.
 * 
 * NOTE: It is caller who responsible for enforce collector to exist
 * while write events.
 */
extern struct execution_event_collector* current_collector;

/*
 * Begin to record information about series of memory accesses.
 * All recorded memory accesses will share same thread and timestamp.
 * 
 * key is returned by the function and should be used in next functions.
 */
static inline void record_memory_accesses_begin(tid_t tid,
    int n_accesses, void** key)
{
    execution_event_memory_accesses_begin(current_collector,
        tid, n_accesses, key);
}

/*
 * Record information about next memory access.
 */
static inline void record_memory_access_next(void* key, addr_t pc,
    addr_t addr, unsigned long size, enum kedr_memory_event_type type)
{
    execution_event_memory_access_next(current_collector,
        key, pc, addr, size, type);
}

/*
 * End writting series of memory accesses.
 * key become invalid after this function call.
 */
static inline void record_memory_accesses_end(void* key)
{
    execution_event_memory_accesses_end(current_collector, key);
}

/* Shortcat for one memory access */
void record_memory_access_one(
    tid_t tid, addr_t pc,
    addr_t addr, unsigned long size, enum kedr_memory_event_type type)
{
    execution_event_memory_access_one(current_collector,
        tid, pc, addr, size, type);
}


/*
 * Record information about locked memory access(update).
 */
static inline void record_locked_memory_access(tid_t tid, addr_t pc,
    addr_t addr, unsigned long size)
{
    execution_event_locked_memory_access(current_collector,
        tid, pc, addr, size);
}

/*
 * Record information about memory barrier.
 */
static inline void record_memory_barrier(tid_t tid, addr_t pc,
    enum kedr_barrier_type type)
{
    execution_event_memory_barrier(current_collector, tid, pc, type);
}

/*
 * Record information about alloc and free operations.
 */
static inline void record_alloc(tid_t tid, addr_t pc,
    unsigned long size, addr_t pointer_returned)
{
    execution_event_alloc(current_collector,
        tid, pc, size, pointer_returned);
}

static inline void record_free(tid_t tid, addr_t pc,
    addr_t pointer_freed)
{
    execution_event_free(current_collector,
        tid, pc, pointer_freed);
}

/*
 * Record information about (un)lock operation.
 */
static inline void record_lock(tid_t tid, addr_t pc,
    addr_t lock_object, enum kedr_lock_type type)
{
    execution_event_lock(current_collector,
        tid, pc, lock_object, type);
}

static inline void record_unlock(tid_t tid, addr_t pc,
    addr_t lock_object, enum kedr_lock_type type)
{
    execution_event_unlock(current_collector,
        tid, pc, lock_object, type);
}


/*
 * Record information about signal and wait operations.
 */
static inline void record_signal(tid_t tid, addr_t pc,
    addr_t wait_object, enum kedr_sw_object_type type)
{
    execution_event_signal(current_collector, tid, pc, wait_object, type);
}

static inline void record_wait(tid_t tid, addr_t pc,
    addr_t wait_object, enum kedr_sw_object_type type)
{
    execution_event_wait(current_collector, tid, pc, wait_object, type);
}


/*
 * Record information about thread creation/joining operations.
 */
static inline void record_thread_create(tid_t tid, addr_t pc,
    tid_t child_tid)
{
    execution_event_thread_create(current_collector,
        tid, pc, child_tid);
}

static inline void record_thread_join(tid_t tid, addr_t pc,
    tid_t child_tid)
{
    execution_event_thread_join(current_collector,
        tid, pc, child_tid);
}

/*
 * Record information about function entry/exit.
 */
static inline void record_function_entry(tid_t tid, addr_t func)
{
    execution_event_function_entry(current_collector, tid, func);
}
static inline void record_function_exit(tid_t tid, addr_t func)
{
    execution_event_function_exit(current_collector, tid, func);
}


/*
 * Record information about function call(before and after).
 */
static inline void record_function_call_pre(tid_t tid, addr_t pc,
    addr_t func)
{
    execution_event_function_call_pre(current_collector, tid, pc, func);
}

static inline void record_function_call_post(tid_t tid, addr_t pc,
    addr_t func)
{
    execution_event_function_call_post(current_collector, tid, pc, func);
}

#endif /* EVENT_COLLECTOR_H */