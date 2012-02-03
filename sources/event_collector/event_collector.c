#include "kedr/event_collector/event_collector.h"
#include "kedr/event_collector/event_handler.h"

#include "kedr/kedr_mem/core_api.h"

#include <linux/module.h>
#include <linux/moduleparam.h>

#include <linux/percpu.h>

#include "trace_buffer.h"

MODULE_AUTHOR("Tsyvarev Andrey");
MODULE_LICENSE("GPL");

#define TRACE_BUFFER_SIZE 10000
unsigned long message_buffer_size = TRACE_BUFFER_SIZE;
module_param(message_buffer_size, ulong, S_IRUGO);

static int execution_event_collector_init(
    struct execution_event_collector* event_collector,
    size_t size,
    struct module* m)
{
    event_collector->common_buffer = trace_buffer_alloc(size, 0);
    if(event_collector->common_buffer == NULL)
    {
        pr_err("Failed to allocate trace buffer for messages.");
        return -ENOMEM;
    }
    
    event_collector->m = m;
    event_collector->private_data = NULL;
    event_collector->is_handled = 0;
    
    return 0;
}

static void execution_event_collector_destroy(
    struct execution_event_collector* event_collector)
{
    trace_buffer_destroy(event_collector->common_buffer);
}

int execution_event_collector_read_message(
    struct execution_event_collector* collector,
    int (*process_message)(const void* msg, size_t size, int cpu,
        u64 ts, bool *consume, void* user_data),
    void* user_data)
{
    return trace_buffer_read_message(collector->common_buffer,
        process_message, 0, user_data);
}
EXPORT_SYMBOL(execution_event_collector_read_message);

/* 
 * Structure used as data which is allocated in 'begin' and used
 * in 'next' and 'end' methods for record information about consequent
 * memory accesses.
 */
struct ma_key
{
    /* Id used for trace buffer */
    void* trace_buffer_id;
    /* Pointer to the subevent which should be written at 'next' call */
    struct execution_message_ma_subevent* current_subevent;
};

/* 
 * Pre allocated callback data.
 * 
 * We use notion about trace_buffer that between 'write_lock' and
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
    struct execution_message_ma* message_ma;
    void* trace_buffer_id;
    
    trace_buffer_id = trace_buffer_write_lock(collector->common_buffer,
        sizeof(*message_ma) + n_accesses
        * sizeof(struct execution_message_ma_subevent),
        (void**)&message_ma);
    if(trace_buffer_id)
    {
        /* CPU is fixed now, so used fast version of get_cpu_var() */
        struct ma_key* key_real = &__get_cpu_var(kedr_ma_keys);
        key_real->trace_buffer_id = trace_buffer_id;
        key_real->current_subevent = message_ma->subevents;
        
        message_ma->base.type = execution_message_type_ma;
        message_ma->base.tid = tid;
        message_ma->n_subevents = (unsigned char)n_accesses;
        
        *key = key_real;
    }
    else
    {
        *key = NULL;
    }
}
EXPORT_SYMBOL(execution_event_memory_accesses_begin);

void execution_event_memory_accesses_end(
    struct execution_event_collector* collector, void* key)
{
    struct ma_key* key_real = (struct ma_key*)key;
    if(key_real)
    {
        trace_buffer_write_unlock(collector->common_buffer,
            key_real->trace_buffer_id);
    }
}
EXPORT_SYMBOL(execution_event_memory_accesses_end);

void execution_event_memory_access_one(
    struct execution_event_collector* collector,
    tid_t tid, addr_t pc,
    addr_t addr, unsigned long size, enum kedr_memory_event_type type)
{
    /* Sequence of memory accesses with 1 access */
    struct
    {
        struct execution_message_ma header;
        struct execution_message_ma_subevent first_elem;
    }message_ma1 =
    {
        .header =
        {
            .base = {.type = execution_message_type_ma, .tid = tid},
            .n_subevents = 1
        },
        .first_elem =
        {
            .pc = pc,
            .addr = addr,
            .size = size,
            .access_type = type
        }
    };
    
    trace_buffer_write_var(current_collector->common_buffer, message_ma1);
}
EXPORT_SYMBOL(execution_event_memory_access_one);

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

void execution_event_locked_memory_access(
    struct execution_event_collector* collector,
    tid_t tid, addr_t pc,
    addr_t addr, unsigned long size)
{
    struct execution_message_lma message_lma =
    {
        .base = {.type = execution_message_type_lma, .tid = tid},
        .pc = pc,
        .addr = addr,
        .size = size
    };
    
    trace_buffer_write_var(collector->common_buffer, message_lma);
}
EXPORT_SYMBOL(execution_event_locked_memory_access);

void execution_event_memory_barrier(
    struct execution_event_collector* collector,
    tid_t tid, addr_t pc,
    enum kedr_barrier_type type)
{
    struct execution_message_mb message_mb =
    {
        .base = {.tid = tid},
        .pc = pc
    };
    switch(type)
    {
    case KEDR_BT_FULL:
        message_mb.base.type = execution_message_type_mfb;
    break;
    case KEDR_BT_LOAD:
        message_mb.base.type = execution_message_type_mrb;
    break;
    case KEDR_BT_STORE:
        message_mb.base.type = execution_message_type_mwb;
    break;
    }
    
    trace_buffer_write_var(collector->common_buffer, message_mb);
}
EXPORT_SYMBOL(execution_event_memory_barrier);

/*
 * Record information about alloc and free operations.
 */
void execution_event_alloc(
    struct execution_event_collector* collector,
    tid_t tid, addr_t pc,
    unsigned long size, addr_t pointer_returned)
{
    struct execution_message_alloc message_alloc =
    {
        .base = {.type = execution_message_type_alloc, .tid = tid},
        .pc = pc,
        .size = size,
        .pointer = pointer_returned
    };
    
    trace_buffer_write_var(collector->common_buffer, message_alloc);
}
EXPORT_SYMBOL(execution_event_alloc);

void execution_event_free(
    struct execution_event_collector* collector,
    tid_t tid, addr_t pc,
    addr_t pointer_freed)
{
    struct execution_message_free message_free =
    {
        .base = {.type = execution_message_type_free, .tid = tid},
        .pc = pc,
        .pointer = pointer_freed
    };
    
    trace_buffer_write_var(collector->common_buffer, message_free);
}
EXPORT_SYMBOL(execution_event_free);

/*
 * Record information about (un)lock operation.
 */
void execution_event_lock(
    struct execution_event_collector* collector,
    tid_t tid, addr_t pc,
    addr_t lock_object, enum kedr_lock_type type)
{
    struct execution_message_lock message_lock =
    {
        .base = {.type = execution_message_type_lock, .tid = tid},
        .pc = pc,
        .obj = lock_object,
        .type = type
    };
    
    trace_buffer_write_var(collector->common_buffer, message_lock);
}
EXPORT_SYMBOL(execution_event_lock);

void execution_event_unlock(
    struct execution_event_collector* collector,
    tid_t tid, addr_t pc,
    addr_t lock_object, enum kedr_lock_type type)
{
    struct execution_message_lock message_lock =
    {
        .base = {.type = execution_message_type_unlock, .tid = tid},
        .pc = pc,
        .obj = lock_object,
        .type = type
    };
    
    trace_buffer_write_var(collector->common_buffer, message_lock);
}
EXPORT_SYMBOL(execution_event_unlock);

/*
 * Record information about signal and wait operations.
 */
void execution_event_signal(
    struct execution_event_collector* collector,
    tid_t tid, addr_t pc,
    addr_t wait_object, enum kedr_sw_object_type type)
{
    struct execution_message_sw message_sw =
    {
        .base = {.type = execution_message_type_signal, .tid = tid},
        .pc = pc,
        .obj = wait_object,
        .type = type
    };
    
    trace_buffer_write_var(collector->common_buffer, message_sw);
}
EXPORT_SYMBOL(execution_event_signal);

void execution_event_wait(
    struct execution_event_collector* collector,
    tid_t tid, addr_t pc,
    addr_t wait_object, enum kedr_sw_object_type type)
{
    struct execution_message_sw message_sw =
    {
        .base = {.type = execution_message_type_wait, .tid = tid},
        .pc = pc,
        .obj = wait_object,
        .type = type
    };
    
    trace_buffer_write_var(collector->common_buffer, message_sw);
}
EXPORT_SYMBOL(execution_event_wait);

/*
 * Record information about thread creation/joining operations.
 */
void execution_event_thread_create(
    struct execution_event_collector* collector,
    tid_t tid, addr_t pc,
    tid_t child_tid)
{
    struct execution_message_tcj message_tcj =
    {
        .base = {.type = execution_message_type_tcreate, .tid = tid},
        .pc = pc,
        .child_tid = child_tid
    };
    
    trace_buffer_write_var(collector->common_buffer, message_tcj);
}
EXPORT_SYMBOL(execution_event_thread_create);

void execution_event_thread_join(
    struct execution_event_collector* collector,
    tid_t tid, addr_t pc,
    tid_t child_tid)
{
    struct execution_message_tcj message_tcj =
    {
        .base = {.type = execution_message_type_tjoin, .tid = tid},
        .pc = pc,
        .child_tid = child_tid
    };
    
    trace_buffer_write_var(collector->common_buffer, message_tcj);
}
EXPORT_SYMBOL(execution_event_thread_join);

/*
 * Record information about function entry/exit.
 */
void execution_event_function_entry(
    struct execution_event_collector* collector,
    tid_t tid, addr_t func)
{
    struct execution_message_fee message_fee =
    {
        .base = {.type = execution_message_type_fentry, .tid = tid},
        .func = func
    };
    
    trace_buffer_write_var(collector->common_buffer, message_fee);
}
EXPORT_SYMBOL(execution_event_function_entry);

void execution_event_function_exit(
    struct execution_event_collector* collector,
    tid_t tid, addr_t func)
{
    struct execution_message_fee message_fee =
    {
        .base = {.type = execution_message_type_fentry, .tid = tid},
        .func = func
    };
    
    trace_buffer_write_var(collector->common_buffer, message_fee);
}
EXPORT_SYMBOL(execution_event_function_exit);

void execution_event_function_call_pre(
    struct execution_event_collector* collector,
    tid_t tid, addr_t pc, addr_t func)
{
    struct execution_message_fc message_fc =
    {
        .base = {.type = execution_message_type_fcpre, .tid = tid},
        .pc = pc,
        .func = func
    };
    
    trace_buffer_write_var(collector->common_buffer, message_fc);
}
EXPORT_SYMBOL(execution_event_function_call_pre);

void execution_event_function_call_post(
    struct execution_event_collector* collector,
    tid_t tid, addr_t pc, addr_t func)
{
    struct execution_message_fc message_fc =
    {
        .base = {.type = execution_message_type_fcpost, .tid = tid},
        .pc = pc,
        .func = func
    };
    
    trace_buffer_write_var(collector->common_buffer, message_fc);
}
EXPORT_SYMBOL(execution_event_function_call_post);
/**********************************************************************/
struct execution_event_collector* current_collector = NULL;
EXPORT_SYMBOL(current_collector);

/* Event handler if it is set and mutex protected it */
static struct execution_event_handler* current_event_handler = NULL;
static DEFINE_MUTEX(current_event_handler_mutex);

int execution_event_set_handler(
    struct execution_event_handler* handler)
{
    int result;
    result = mutex_lock_interruptible(&current_event_handler_mutex);
    if(result < 0) return result;
    
    if(current_event_handler != NULL)
    {
        pr_err("Attempt to set event handler while it already set.");
        result = -EBUSY;
        goto out;
    }
    
    current_event_handler = handler;
out:    
    mutex_unlock(&current_event_handler_mutex);
    return result;
}
EXPORT_SYMBOL(execution_event_set_handler);

int execution_event_unset_handler(
    struct execution_event_handler* handler)
{
    int result;
    result = mutex_lock_killable(&current_event_handler_mutex);
    if(result < 0) return result;
    
    if(current_event_handler != handler)
    {
        pr_err("Attempt to unset event handler while it not set.");
        result = -EINVAL;
        goto out;
    }
    
    current_event_handler = NULL;
out:    
    mutex_unlock(&current_event_handler_mutex);
    return result;
}
EXPORT_SYMBOL(execution_event_unset_handler);

/************************** Handlers for core *************************/
static void collector_on_target_loaded(struct kedr_event_handlers *eh, 
    struct module *target_module)
{
    /* 
     * NOTE: It is expected here, that only one target module
     * may be active in a time.
     */
    int result;
    struct execution_event_collector* event_collector;
    
    /* Protection against multiple modules at a time */
    if(current_collector) return;
    
    event_collector = kmalloc(sizeof(typeof(*event_collector)), GFP_KERNEL);
    
    if(event_collector == NULL)
    {
        pr_err("Failed to allocate event collector. "
            "No handlers will be executed for instrumentated code.");
        goto event_collector_alloc_err;
    }
    
    result = execution_event_collector_init(event_collector,
        message_buffer_size, target_module);
    if(result < 0) goto event_collector_init_err;
    
    result = mutex_lock_interruptible(&current_event_handler_mutex);
    if(result < 0) goto mutex_lock_err;
    
    if(current_event_handler == NULL)
    {
        pr_info("Event collector is used without handler because it isn't set.");
        goto out;
    }
    
    if(current_event_handler->owner
        && !try_module_get(current_event_handler->owner))
    {
        pr_info("Event collector is used without handler "
            "because it is going to unload.");
        goto out;
    }
    
    result = current_event_handler->start(event_collector);
    if(result < 0)
    {
        if(current_event_handler->owner)
            module_put(current_event_handler->owner);

        pr_info("Event collector is used without handler "
            "because it is failed to start.");
        goto out;
    }    
    
    event_collector->is_handled = 1;
out:
    mutex_unlock(&current_event_handler_mutex);
    
    current_collector = event_collector;
    return;

mutex_lock_err:
    execution_event_collector_destroy(event_collector);
event_collector_init_err:
    kfree(event_collector);
event_collector_alloc_err:
    return;
}

static void collector_on_target_about_to_unload(
    struct kedr_event_handlers *eh, struct module *target_module)
{
    int result;
    
    if(current_collector == NULL) return;/* collector wasn't started */
    
    /* Protection against multiple modules at a time */
    if(current_collector->m != target_module) return;
    
    if(!current_collector->is_handled) goto not_handled;
    
    result = mutex_lock_killable(&current_event_handler_mutex);
    if(result < 0) return;
    
    result = current_event_handler->stop(current_collector);
    if(result < 0) goto out;
    
    if(current_event_handler->owner)
        module_put(current_event_handler->owner);
    
    current_collector->is_handled = 0;
out:    
    mutex_unlock(&current_event_handler_mutex);

    if(current_collector->is_handled)
    {
        pr_err("Error occures in handler while stop. Event collector is not freed.");
        return;
    }

not_handled:
    execution_event_collector_destroy(current_collector);
    kfree(current_collector);
    current_collector = NULL;
    
    return;
}

static void collector_on_function_entry(struct kedr_event_handlers *eh, 
    unsigned long tid, unsigned long func)
{
    record_function_entry(tid, func);
}

static void collector_on_function_exit(struct kedr_event_handlers *eh, 
    unsigned long tid, unsigned long func)
{
    record_function_exit(tid, func);
}

static void collector_on_call_pre(struct kedr_event_handlers *eh, 
	unsigned long tid, unsigned long pc, unsigned long func)
{
    record_function_call_pre(tid, pc, func);
}

static void collector_on_call_post(struct kedr_event_handlers *eh, 
	unsigned long tid, unsigned long pc, unsigned long func)
{
    record_function_call_post(tid, pc, func);
}

static void collector_begin_memory_events(
    struct kedr_event_handlers *eh, 
    unsigned long tid, unsigned long num_events, 
    void **pdata /* out param*/)
{
    record_memory_accesses_begin(tid, (int)num_events, pdata);
}

static void collector_end_memory_events(
    struct kedr_event_handlers *eh, 
	unsigned long tid, void *data)
{
    record_memory_accesses_end(data);
}

static void collector_on_memory_event(
    struct kedr_event_handlers *eh, 
    unsigned long tid, 
    unsigned long pc, unsigned long addr, unsigned long size, 
    enum kedr_memory_event_type memory_event_type,
    void *data)
{
    record_memory_access_next(data, pc, addr, size,
        (unsigned char)memory_event_type);
}

static DEFINE_SPINLOCK(locked_access_lock);

static void collector_on_locked_op_pre(struct kedr_event_handlers *eh, 
    unsigned long tid, unsigned long pc, 
    void **pdata)
{
    /* 
     * Not only memory operation should be atomic, but also its
     * writing into trace buffer should be in same atomic sequence.
     * (timestamp).
     * 
     * Use global spinlock for emulate this atomicity.
     */

    unsigned long flags;
    
    /* write barrier before the operation */
    record_memory_barrier(tid, pc, KEDR_BT_STORE);
    
    spin_lock_irqsave(&locked_access_lock, flags);
    
    /* Store flags as pointer(on x86 and x86_64 it is acceptable) */
    *pdata = (void*)flags;
}
static void collector_on_locked_op_post(struct kedr_event_handlers *eh, 
    unsigned long tid, unsigned long pc, 
    unsigned long addr, unsigned long size, 
    enum kedr_memory_event_type type, void *data)
{
    /* Restore flags */
    unsigned long flags = (unsigned long)data;

    if(type == KEDR_ET_MUPDATE)
    {
        /* Normal locked operation */
        record_locked_memory_access(tid, pc, addr, size);
    }
    else
    {
        /*
         *  Instruction like cmpxchg found value is not expected.
         * 
         * Record about it as normal memory access(not as locked).
         */
        record_memory_access_one(tid, pc, addr, size, type);
    }
    spin_unlock_irqrestore(&locked_access_lock, flags);
    /* Read barrier after the operation */
    record_memory_barrier(tid, pc, KEDR_BT_LOAD);
}


static void collector_on_io_mem_op_pre(struct kedr_event_handlers *eh, 
    unsigned long tid, unsigned long pc, 
    void **pdata)
{
    /* Write barrier before operation */
    record_memory_barrier(tid, pc, KEDR_BT_STORE);
}

static void collector_on_io_mem_op_post(struct kedr_event_handlers *eh, 
    unsigned long tid, unsigned long pc, 
    unsigned long addr, unsigned long size, 
    enum kedr_memory_event_type type, void *data)
{
    /* Simple memory access plus read barrier */
    record_memory_access_one(tid, pc, addr, size, type);
    record_memory_barrier(tid, pc, KEDR_BT_LOAD);
}

/*
 * Record information about barrier after operation,
 * which don't access memory.
 */
static void collector_on_memory_barrier_post(struct kedr_event_handlers *eh, 
    unsigned long tid, unsigned long pc, 
    enum kedr_barrier_type type)
{
    record_memory_barrier(tid, pc, type);
}

static void collector_on_alloc_post(struct kedr_event_handlers *eh, 
		unsigned long tid, unsigned long pc, 
		unsigned long size, unsigned long addr)
{
    record_alloc(tid, pc, size, addr);
}

static void collector_on_free_pre(struct kedr_event_handlers *eh, 
    unsigned long tid, unsigned long pc, 
    unsigned long addr)
{
    record_free(tid, pc, addr);
}

static void collector_on_lock_post(struct kedr_event_handlers *eh, 
    unsigned long tid, unsigned long pc, 
    unsigned long lock_id, enum kedr_lock_type type)
{
    record_lock(tid, pc, lock_id, type);
}

static void collector_on_unlock_pre(struct kedr_event_handlers *eh, 
    unsigned long tid, unsigned long pc, 
    unsigned long lock_id, enum kedr_lock_type type)
{
    record_unlock(tid, pc, lock_id, type);
}

static void collector_on_signal_pre(struct kedr_event_handlers *eh, 
		unsigned long tid, unsigned long pc, 
		unsigned long obj_id, enum kedr_sw_object_type type)
{
    record_signal(tid, pc, obj_id, type);
}

static void collector_on_wait_post(struct kedr_event_handlers *eh, 
    unsigned long tid, unsigned long pc, 
    unsigned long obj_id, enum kedr_sw_object_type type)
{
    record_wait(tid, pc, obj_id, type);
}

// TODO: others events from KEDR(thread create/thread join)

static struct kedr_event_handlers collector_event_handlers =
{
    .owner = THIS_MODULE,
    .on_target_loaded =             collector_on_target_loaded,
    .on_target_about_to_unload =    collector_on_target_about_to_unload,
    
    .on_function_entry =            collector_on_function_entry,
    .on_function_exit =             collector_on_function_exit,
    
    .on_call_pre =                  collector_on_call_pre,
    .on_call_post =                 collector_on_call_post,
    
    .begin_memory_events =          collector_begin_memory_events,
    .end_memory_events =            collector_end_memory_events,
    .on_memory_event =              collector_on_memory_event,
    
    .on_locked_op_pre =             collector_on_locked_op_pre,
    .on_locked_op_post =            collector_on_locked_op_post,
    
    .on_io_mem_op_pre =             collector_on_io_mem_op_pre,
    .on_io_mem_op_post =            collector_on_io_mem_op_post,
    
    .on_memory_barrier_pre =        collector_on_memory_barrier_post,
    
    .on_alloc_post =                collector_on_alloc_post,
    .on_free_pre =                  collector_on_free_pre,
    
    .on_lock_post =                 collector_on_lock_post,
    .on_unlock_pre =                collector_on_unlock_pre,
    
    .on_signal_pre =                collector_on_signal_pre,
    .on_wait_post =                 collector_on_wait_post,
    // TODO: others handlers(thread create/join)
};

static int __init event_collector_init(void)
{
    int result = kedr_register_event_handlers(&collector_event_handlers);
    if(result < 0)
    {
        pr_err("Failed to register collector's event handlers.");
        return result;
    }
    
    return 0;
}

static void __exit event_collector_exit(void)
{
    kedr_unregister_event_handlers(&collector_event_handlers);
}

module_init(event_collector_init);
module_exit(event_collector_exit);