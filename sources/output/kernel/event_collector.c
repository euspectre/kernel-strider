#include <kedr/output/event_collector.h>

/* 
 * Use per-cpu variable for store state in
 * execution_event_memory_accesses_begin().
 */
#include <linux/percpu.h> 

#include <linux/module.h> /* Export functions for write message */

#include <linux/hrtimer.h> /* high resolution timer for clock*/

int execution_event_collector_init(
    struct execution_event_collector* event_collector,
    size_t buffer_normal_size, size_t buffer_critical_size)
{
    event_collector->buffer_normal
        = ring_buffer_alloc(buffer_normal_size, 0);
    if(event_collector->buffer_normal == NULL)
    {
        pr_err("Failed to allocate ring buffer for normal messages.");
        return -ENOMEM;
    }
    
    event_collector->buffer_critical
        = ring_buffer_alloc(buffer_critical_size, RB_FL_OVERWRITE);
    if(event_collector->buffer_critical == NULL)
    {
        pr_err("Failed to allocate ring buffer for critical messages.");
        ring_buffer_free(event_collector->buffer_normal);
        return -ENOMEM;
    }

    atomic_set(&event_collector->message_counter, 0);

    return 0;
}

void execution_event_collector_destroy(
    struct execution_event_collector* event_collector)
{
    ring_buffer_free(event_collector->buffer_normal);
    ring_buffer_free(event_collector->buffer_critical);
}

/* 
 * Need own clocks for sorting events between cpu-buffers.
 * Original clocks of ring_buffer is not always sufficient for that purpose.
 */
static u64 kedr_clock(void)
{
    //should be correct with garantee
    return ktime_to_ns(ktime_get());
}
/*************** Writting messages into buffer ************************/

/* 
 * Structure used as data which is allocated in 'begin' and used
 * in 'next' and 'end' methods for record information about consequent
 * memory accesses.
 */
struct ma_key
{
    /* Event which should be commited at the end */
    struct ring_buffer_event* event;
    /* Pointer to the subevent which should be written at 'next' call */
    struct execution_message_ma_subevent* current_subevent;
};

/* 
 * Pre allocated callback data.
 * 
 * We use notion about ring_buffer that between 'write_lock' and
 * 'write_unlock' cpu is fixed.
 * 
 * So, per-cpu variables is sufficient to be used as such callback data.
 * 
 * Even when several collectors exists, them may use global data,
 * because two collectors may not execute critical section on same cpu
 * simultaneously.
 */
DEFINE_PER_CPU(struct ma_key, kedr_ma_keys);

void execution_event_memory_accesses_begin(
    struct execution_event_collector* collector,
    tid_t tid, int n_accesses, void** key)
{
    struct ring_buffer* buffer = collector->buffer_normal;
    struct ma_key* key_real;
    struct execution_message_ma* message_ma;
    struct ring_buffer_event* event = ring_buffer_lock_reserve(buffer,
        sizeof(struct execution_message_ma)
        + n_accesses * sizeof(struct execution_message_ma_subevent));
    if(event == NULL)
    {
        *key = NULL;
        return;
    }
    message_ma = ring_buffer_event_data(event);
    
    message_ma->base.type = execution_message_type_ma;
    message_ma->base.tid = tid;
    message_ma->base.ts = kedr_clock();
    message_ma->base.counter = atomic_inc_return(&collector->message_counter);
    message_ma->n_subevents = n_accesses;

    key_real = &(__get_cpu_var(kedr_ma_keys));
    key_real->event = event;
    key_real->current_subevent = message_ma->subevents;

    *key = key_real;
}
EXPORT_SYMBOL(execution_event_memory_accesses_begin);

void execution_event_memory_accesses_end(
    struct execution_event_collector* collector, void* key)
{
    struct ma_key* key_real = (struct ma_key*)key;
    if(key_real)
    {
        ring_buffer_unlock_commit(collector->buffer_normal, key_real->event);
    }
}
EXPORT_SYMBOL(execution_event_memory_accesses_end);

void execution_event_memory_access_next(
    struct execution_event_collector* collector,
    void* key,
    addr_t pc,
    addr_t addr, unsigned long size,
    enum kedr_memory_event_type type)
{
    struct ma_key* key_real = (struct ma_key*)key;
    if(key_real)
    {
        struct execution_message_ma_subevent* subevent =
            key_real->current_subevent++;
        subevent->pc = pc;
        subevent->addr = addr;
        subevent->size = size;
        subevent->access_type = type;
    }
}
EXPORT_SYMBOL(execution_event_memory_access_next);

void execution_event_memory_access_one(
    struct execution_event_collector* collector,
    tid_t tid, addr_t pc,
    addr_t addr, unsigned long size, enum kedr_memory_event_type type)
{
    void* key;
    execution_event_memory_accesses_begin(collector, tid, 1, &key);
    execution_event_memory_access_next(collector, key, pc, addr, size, type);
    execution_event_memory_accesses_end(collector, key);
}
EXPORT_SYMBOL(execution_event_memory_access_one);

#define WRITE_CRITICAL_MESSAGE_BEGIN(struct_suffix, type_suffix)        \
struct ring_buffer* buffer = collector->buffer_critical;                \
struct execution_message_##struct_suffix* message_##struct_suffix;      \
struct ring_buffer_event* event = ring_buffer_lock_reserve(buffer,      \
    sizeof(struct execution_message_##struct_suffix));                  \
if(event == NULL) return;                                               \
message_##struct_suffix = ring_buffer_event_data(event);                \
message_##struct_suffix->base.type = execution_message_type_##type_suffix;  \
message_##struct_suffix->base.tid = tid;                                \
message_##struct_suffix->base.ts = kedr_clock();                        \
message_##struct_suffix->base.counter = atomic_inc_return(&collector->message_counter);

/* Between macros message_##struct_suffix contains pointer to typed message. */

#define WRITE_CRITICAL_MESSAGE_END() ring_buffer_unlock_commit(buffer, event)


void execution_event_locked_memory_access(
    struct execution_event_collector* collector,
    tid_t tid, addr_t pc,
    addr_t addr, unsigned long size,
    enum kedr_memory_event_type type)
{
    switch(type)
    {
    case KEDR_ET_MUPDATE:
        {
            WRITE_CRITICAL_MESSAGE_BEGIN(lma, lma_update);
            message_lma->pc = pc;
            message_lma->addr = addr;
            message_lma->size = size;
            WRITE_CRITICAL_MESSAGE_END();
        }
    break;
    case KEDR_ET_MREAD:
        {
            WRITE_CRITICAL_MESSAGE_BEGIN(lma, lma_read);
            message_lma->pc = pc;
            message_lma->addr = addr;
            message_lma->size = size;
            WRITE_CRITICAL_MESSAGE_END();
        }
    break;
    case KEDR_ET_MWRITE:
        {
            WRITE_CRITICAL_MESSAGE_BEGIN(lma, lma_write);
            message_lma->pc = pc;
            message_lma->addr = addr;
            message_lma->size = size;
            WRITE_CRITICAL_MESSAGE_END();
        }
    break;
    }
}
EXPORT_SYMBOL(execution_event_locked_memory_access);

void execution_event_io_memory_access(
    struct execution_event_collector* collector,
    tid_t tid, addr_t pc,
    addr_t addr, unsigned long size,
    enum kedr_memory_event_type type)
{
    WRITE_CRITICAL_MESSAGE_BEGIN(ioma, ioma);
    message_ioma->pc = pc;
    message_ioma->addr = addr;
    message_ioma->size = size;
    message_ioma->access_type = type;
    WRITE_CRITICAL_MESSAGE_END();
}


EXPORT_SYMBOL(execution_event_io_memory_access);

void execution_event_memory_barrier(
    struct execution_event_collector* collector,
    tid_t tid, addr_t pc,
    enum kedr_barrier_type type)
{
    switch(type)
    {
    case KEDR_BT_FULL:
        {
            WRITE_CRITICAL_MESSAGE_BEGIN(mb, mfb);
            message_mb->pc = pc;
            WRITE_CRITICAL_MESSAGE_END();
        }
    break;
    case KEDR_BT_LOAD:
        {
            WRITE_CRITICAL_MESSAGE_BEGIN(mb, mrb);
            message_mb->pc = pc;
            WRITE_CRITICAL_MESSAGE_END();
        }
    break;
    case KEDR_BT_STORE:
        {
            WRITE_CRITICAL_MESSAGE_BEGIN(mb, mwb);
            message_mb->pc = pc;
            WRITE_CRITICAL_MESSAGE_END();
        }
    break;
    }
}
EXPORT_SYMBOL(execution_event_memory_barrier);

void execution_event_alloc(
    struct execution_event_collector* collector,
    tid_t tid, addr_t pc,
    unsigned long size, addr_t pointer_returned)
{
    WRITE_CRITICAL_MESSAGE_BEGIN(alloc, alloc);
    message_alloc->pc = pc;
    message_alloc->size = size;
    message_alloc->pointer = pointer_returned;
    WRITE_CRITICAL_MESSAGE_END();
}
EXPORT_SYMBOL(execution_event_alloc);

void execution_event_free(
    struct execution_event_collector* collector,
    tid_t tid, addr_t pc,
    addr_t pointer_freed)
{
    WRITE_CRITICAL_MESSAGE_BEGIN(free, free);
    message_free->pc = pc;
    message_free->pointer = pointer_freed;
    WRITE_CRITICAL_MESSAGE_END();
}
EXPORT_SYMBOL(execution_event_free);

void execution_event_lock(
    struct execution_event_collector* collector,
    tid_t tid, addr_t pc,
    addr_t lock_object, enum kedr_lock_type type)
{
    if(type == KEDR_LT_RLOCK)
    {
        WRITE_CRITICAL_MESSAGE_BEGIN(lock, rlock);
        message_lock->type = type;
        message_lock->pc = pc;
        message_lock->obj = lock_object;
        WRITE_CRITICAL_MESSAGE_END();
    }
    else
    {
        WRITE_CRITICAL_MESSAGE_BEGIN(lock, lock);
        message_lock->type = type;
        message_lock->pc = pc;
        message_lock->obj = lock_object;
        WRITE_CRITICAL_MESSAGE_END();
    }
}
EXPORT_SYMBOL(execution_event_lock);

void execution_event_unlock(
    struct execution_event_collector* collector,
    tid_t tid, addr_t pc,
    addr_t lock_object, enum kedr_lock_type type)
{
    if(type == KEDR_LT_RLOCK)
    {
        WRITE_CRITICAL_MESSAGE_BEGIN(lock, runlock);
        message_lock->type = type;
        message_lock->pc = pc;
        message_lock->obj = lock_object;
        WRITE_CRITICAL_MESSAGE_END();
    }
    else
    {
        WRITE_CRITICAL_MESSAGE_BEGIN(lock, unlock);
        message_lock->type = type;
        message_lock->pc = pc;
        message_lock->obj = lock_object;
        WRITE_CRITICAL_MESSAGE_END();
    }
}
EXPORT_SYMBOL(execution_event_unlock);

void execution_event_signal(
    struct execution_event_collector* collector,
    tid_t tid, addr_t pc,
    addr_t wait_object, enum kedr_sw_object_type type)
{
    WRITE_CRITICAL_MESSAGE_BEGIN(sw, signal);
    message_sw->type = type;
    message_sw->pc = pc;
    message_sw->obj = wait_object;
    WRITE_CRITICAL_MESSAGE_END();
}
EXPORT_SYMBOL(execution_event_signal);

void execution_event_wait(
    struct execution_event_collector* collector,
    tid_t tid, addr_t pc,
    addr_t wait_object, enum kedr_sw_object_type type)
{
    WRITE_CRITICAL_MESSAGE_BEGIN(sw, wait);
    message_sw->type = type;
    message_sw->pc = pc;
    message_sw->obj = wait_object;
    WRITE_CRITICAL_MESSAGE_END();
}
EXPORT_SYMBOL(execution_event_wait);

void execution_event_thread_create_before(
    struct execution_event_collector* collector,
    tid_t tid, addr_t pc)
{
    WRITE_CRITICAL_MESSAGE_BEGIN(tc_before, tc_before);
    message_tc_before->pc = pc;
    WRITE_CRITICAL_MESSAGE_END();
}
EXPORT_SYMBOL(execution_event_thread_create_before);

void execution_event_thread_create_after(
    struct execution_event_collector* collector,
    tid_t tid, addr_t pc,
    tid_t child_tid)
{
    WRITE_CRITICAL_MESSAGE_BEGIN(tc_after, tc_after);
    message_tc_after->pc = pc;
    message_tc_after->child_tid = child_tid;
    WRITE_CRITICAL_MESSAGE_END();
}
EXPORT_SYMBOL(execution_event_thread_create_after);


void execution_event_thread_join(
    struct execution_event_collector* collector,
    tid_t tid, addr_t pc,
    tid_t child_tid)
{
    WRITE_CRITICAL_MESSAGE_BEGIN(tjoin, tjoin);
    message_tjoin->pc = pc;
    message_tjoin->child_tid = child_tid;
    WRITE_CRITICAL_MESSAGE_END();
}
EXPORT_SYMBOL(execution_event_thread_join);

void execution_event_function_entry(
    struct execution_event_collector* collector,
    tid_t tid, addr_t func)
{
    WRITE_CRITICAL_MESSAGE_BEGIN(fee, fentry);
    message_fee->func = func;
    WRITE_CRITICAL_MESSAGE_END();
}
EXPORT_SYMBOL(execution_event_function_entry);

void execution_event_function_exit(
    struct execution_event_collector* collector,
    tid_t tid, addr_t func)
{
    WRITE_CRITICAL_MESSAGE_BEGIN(fee, fexit);
    message_fee->func = func;
    WRITE_CRITICAL_MESSAGE_END();
}
EXPORT_SYMBOL(execution_event_function_exit);

void execution_event_function_call_pre(
    struct execution_event_collector* collector,
    tid_t tid, addr_t pc, addr_t func)
{
    WRITE_CRITICAL_MESSAGE_BEGIN(fc, fcpre);
    message_fc->pc = pc;
    message_fc->func = func;
    WRITE_CRITICAL_MESSAGE_END();
}
EXPORT_SYMBOL(execution_event_function_call_pre);

void execution_event_function_call_post(
    struct execution_event_collector* collector,
    tid_t tid, addr_t pc, addr_t func)
{
    WRITE_CRITICAL_MESSAGE_BEGIN(fc, fcpost);
    message_fc->pc = pc;
    message_fc->func = func;
    WRITE_CRITICAL_MESSAGE_END();
}
EXPORT_SYMBOL(execution_event_function_call_post);
