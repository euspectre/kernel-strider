/*
 * API for recording events concerning synchronization issues
 * (memory reads, writes, lockings, allocations and other events, needed
 * for detecting data races).
 * 
 * This header is mainly used by implementation and testing.
 * But also may be used for record some events without core module.
 */

#ifndef EVENT_COLLECTOR_H
#define EVENT_COLLECTOR_H

typedef unsigned long tid_t;
typedef unsigned long addr_t;

#include <linux/module.h>

#include "kedr/object_types.h" /* memory access types, lock types...*/

#include <linux/ring_buffer.h> /* ring buffers */


/*
 * For some reason, standard clock for ring buffer is not sufficient
 * for interprocessor message ordering.
 * 
 * Instead of implementing our clocks, use atomic counter for ordering
 * messages.
 * 
 * Because some architecture has atomics with low bits number, counter
 * may not be used for order all messages.
 * It should be used only for messages, which are 'near' to each other
 * accorging to their timestamps.
 * 
 * This macro make 'near' conception clear:
 * if abs(ts1 - ts2) > KEDR_CLOCK_PRECISION, then messages are ordered
 * according to their timestamps;
 * otherwise messages are ordered according to counter.
 */

//TODO: check correctness of this value
#define KEDR_CLOCK_PRECISION 100000


struct execution_event_collector
{
    /*
     * Ring buffers contained messages about read/writes and sync.
     * 
     * 'buffer_normal' is for messages, which may be lost without
     * affecting on interpretation of others,
     * 'buffer_critical' is for messages, loss of which is very
     * undesirable.
     * 
     * Currently, normal buffer contains only messages about read/writes
     * (without locks), critical buffer containes all other messages.
     */

    struct ring_buffer* buffer_normal;
    struct ring_buffer* buffer_critical;
    /* Message counter. See description above. */
    atomic_t message_counter;
};

/*************** Formats of messages collected ************************/

/* Types of messages */
enum execution_message_type
{
    /* 
     * Message contains array of information about consequent
     * memory accesses.
     */
    execution_message_type_ma = 0,
    /* Message contains information about one "true" locked memory access */
    execution_message_type_lma_update,
    /* Message contains information about one "failed" locked memory access */
    execution_message_type_lma_read,
    /* Message contains information about one strange locked memory access(for completeness) */
    execution_message_type_lma_write,
    /* Message contains information about I/O operation which access memory */
    execution_message_type_ioma,
    /* 
     * Message contains information about one memory barrier
     * (read, write, full).
     */
    execution_message_type_mrb,
    execution_message_type_mwb,
    execution_message_type_mfb,
    /* 
     * Message contains information about one memory management operation
     * (alloc/free).
     */
    execution_message_type_alloc,
    execution_message_type_free,
    /* 
     * Message contains information about one lock operation
     * (lock/unlock or its read variants).
     */
    execution_message_type_lock,
    execution_message_type_unlock,
    
    execution_message_type_rlock,
    execution_message_type_runlock,
    /* Message contains information about one signal/wait operation */
    execution_message_type_signal,
    execution_message_type_wait,
    /* Message contains information about thread create/join operation */
    execution_message_type_tc_before,
    execution_message_type_tc_after,
    execution_message_type_tjoin,
    /* Message contains information about function entry/exit */
    execution_message_type_fentry,
    execution_message_type_fexit,
    /* Message contain information about function call(pre and post)*/
    execution_message_type_fcpre,
    execution_message_type_fcpost,
};

/* Beginning of every message */
struct execution_message_base
{
    tid_t tid;
    uint64_t ts;/* Own timestamp */
    uint16_t counter;
    char type;
};

/* Messages of concrete types */
struct execution_message_ma_subevent
{
    addr_t pc;
    addr_t addr;
    size_t size;
    unsigned char access_type;
};

struct execution_message_ma
{
    struct execution_message_base base; /* .type = ma */
    unsigned char n_subevents;
    struct execution_message_ma_subevent subevents[0];
};

struct execution_message_lma
{
    struct execution_message_base base; /* .type = lma_* */
    addr_t pc;
    addr_t addr;
    size_t size;
};

struct execution_message_ioma
{
    struct execution_message_base base; /* .type = ioma */
    addr_t pc;
    addr_t addr;
    size_t size;
    unsigned char access_type;
};


struct execution_message_mb
{
    struct execution_message_base base; /* .type = m*b */
    addr_t pc;
};

struct execution_message_alloc
{
    struct execution_message_base base; /* .type = alloc */
    addr_t pc;
    size_t size;
    addr_t pointer;
};

struct execution_message_free
{
    struct execution_message_base base; /* .type = free */
    addr_t pc;
    addr_t pointer;
};

struct execution_message_lock
{
    struct execution_message_base base; /* .type = (un)lock */
    unsigned char type;
    addr_t pc;
    addr_t obj;
};

struct execution_message_sw
{
    struct execution_message_base base; /* .type = signal/wait */
    addr_t pc;
    addr_t obj;
    unsigned char type;
};

struct execution_message_tc_before
{
    struct execution_message_base base; /* .type = tc_before */
    addr_t pc;
};


struct execution_message_tc_after
{
    struct execution_message_base base; /* .type = tc_after */
    addr_t pc;
    tid_t child_tid; // TODO: error mark, currently it is '-1'
    // (0 is used for interrupt handler on cpu0)
};

struct execution_message_tjoin
{
    struct execution_message_base base; /* .type = tjoin */
    addr_t pc;
    tid_t child_tid;
};


struct execution_message_fee
{
    struct execution_message_base base; /* .type = fentry/fexit */
    addr_t func;
};

struct execution_message_fc
{
    struct execution_message_base base; /* .type = fcpre/fcpost */
    addr_t pc;
    addr_t func;
};

/************** Functions for writting messages into collector*********/

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
    addr_t addr, unsigned long size,
    enum kedr_memory_event_type type);

/*
 * Record information about locked memory access.
 */
void execution_event_locked_memory_access(
    struct execution_event_collector* collector,
    tid_t tid, addr_t pc,
    addr_t addr, unsigned long size,
    enum kedr_memory_event_type type);

/*
 * Record information about I/O with memory access.
 */
void execution_event_io_memory_access(
    struct execution_event_collector* collector,
    tid_t tid, addr_t pc,
    addr_t addr, unsigned long size,
    enum kedr_memory_event_type type);


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
 * Record information about thread creation.
 */
void execution_event_thread_create_before(
    struct execution_event_collector* collector,
    tid_t tid, addr_t pc);

void execution_event_thread_create_after(
    struct execution_event_collector* collector,
    tid_t tid, addr_t pc,
    tid_t child_tid);

/* Cancel thread creation, beginning in 'create_before'*/
static inline void execution_event_thread_create_cancel(
    struct execution_event_collector* collector,
    tid_t tid, addr_t pc)
{
    execution_event_thread_create_after(collector, tid, pc, -1);
}

/*
 * Record information about thread joining.
 */
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
static inline void record_memory_access_one(
    tid_t tid, addr_t pc,
    addr_t addr, unsigned long size, enum kedr_memory_event_type type)
{
    execution_event_memory_access_one(current_collector,
        tid, pc, addr, size, type);
}


/*
 * Record information about locked memory access.
 */
static inline void record_locked_memory_access(tid_t tid, addr_t pc,
    addr_t addr, unsigned long size, enum kedr_memory_event_type type)
{
    execution_event_locked_memory_access(current_collector,
        tid, pc, addr, size, type);
}

/*
 * Record information about I/O operation with memory access.
 */
static inline void record_io_memory_access(tid_t tid, addr_t pc,
    addr_t addr, unsigned long size, enum kedr_memory_event_type type)
{
    execution_event_io_memory_access(current_collector,
        tid, pc, addr, size, type);
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
static inline void record_thread_create_before(tid_t tid, addr_t pc)
{
    execution_event_thread_create_before(current_collector, tid, pc);
}

static inline void record_thread_create_after(tid_t tid, addr_t pc,
    tid_t child_tid)
{
    execution_event_thread_create_after(current_collector,
        tid, pc, child_tid);
}

static inline void record_thread_create_cancel(tid_t tid, addr_t pc)
{
    execution_event_thread_create_cancel(current_collector, tid, pc);
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

/************** Collector initialization/finalization *****************/
int execution_event_collector_init(
    struct execution_event_collector* event_collector,
    size_t buffer_normal_size, size_t buffer_critical_size);

void execution_event_collector_destroy(
    struct execution_event_collector* event_collector);


#endif /* EVENT_COLLECTOR_H */
