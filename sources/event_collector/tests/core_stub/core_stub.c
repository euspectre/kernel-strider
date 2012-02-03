/*
 * Simple implementation of core module, used for check event collector
 * in a role of kedr event handler.
 */

//#include "core_stub_api.h"
#include <kedr/kedr_mem/core_api.h>

#include <linux/module.h>
#include <linux/mutex.h>

MODULE_AUTHOR("Andrey Tsyvarev");
MODULE_LICENSE("GPL");

static struct kedr_event_handlers* current_handler = NULL;
int is_target_loaded = 0;
int is_handler_used = 0;

static DEFINE_MUTEX(current_handler_mutex);

int 
kedr_register_event_handlers(struct kedr_event_handlers *eh)
{
    int result = mutex_lock_interruptible(&current_handler_mutex);
    if(result < 0) return result;
    
    if(current_handler != NULL)
    {
        pr_err("Attempt to register event handler while there is already registered one.");
        result = -EBUSY;
        goto out;
    }
    
    if(is_target_loaded)
    {
        pr_err("Attempt to register event handler while target is loaded.");
        result = -EBUSY;
        goto out;
    }
    
    current_handler = eh;
out:
    mutex_unlock(&current_handler_mutex);
    
    return result;
}
EXPORT_SYMBOL(kedr_register_event_handlers);

void 
kedr_unregister_event_handlers(struct kedr_event_handlers *eh)
{
    int result = mutex_lock_killable(&current_handler_mutex);
    if(result < 0) return;
    
    BUG_ON(current_handler != eh);
    BUG_ON(is_handler_used);
    
    current_handler = NULL;
    mutex_unlock(&current_handler_mutex);
}
EXPORT_SYMBOL(kedr_unregister_event_handlers);

/* Pattern for callback wrappers */
#define CALLBACK_CALL(__callback, ...) do {                         \
if(is_handler_used && current_handler->__callback)                \
    current_handler->__callback(current_handler, ##__VA_ARGS__);    \
}while(0)

void stub_on_target_loaded(struct module* m)
{
    int result = mutex_lock_killable(&current_handler_mutex);
    if(result < 0) return;
    
    BUG_ON(is_target_loaded);
    is_target_loaded = 1;
    
    if(current_handler != NULL)
    {
        if(current_handler->owner && !try_module_get(current_handler->owner))
        {
            pr_info("Failed to fix module contained event handler.");
            goto out;
        }
        is_handler_used = 1;
    }
out:
    mutex_unlock(&current_handler_mutex);
    
    CALLBACK_CALL(on_target_loaded, m);
}
EXPORT_SYMBOL(stub_on_target_loaded);


void stub_on_target_about_to_unload(struct module* m)
{
    int result;
    
    CALLBACK_CALL(on_target_about_to_unload, m);

    result = mutex_lock_killable(&current_handler_mutex);
    if(result < 0) return;
    
    BUG_ON(!is_target_loaded);
    is_target_loaded = 0;
    
    if(is_handler_used)
    {
        if(current_handler->owner)
            module_put(current_handler->owner);
        is_handler_used = 0;
    }
    
    mutex_unlock(&current_handler_mutex);
}
EXPORT_SYMBOL(stub_on_target_about_to_unload);

void stub_on_function_entry(unsigned long tid, unsigned long func)
{
    CALLBACK_CALL(on_function_entry, tid, func);
}
EXPORT_SYMBOL(stub_on_function_entry);

void stub_on_function_exit(unsigned long tid, unsigned long func)
{
    CALLBACK_CALL(on_function_exit, tid, func);
}
EXPORT_SYMBOL(stub_on_function_exit);

void stub_on_call_pre(unsigned long tid, unsigned long pc,
    unsigned long func)
{
    CALLBACK_CALL(on_call_pre, tid, pc, func);
}
EXPORT_SYMBOL(stub_on_call_pre);
void stub_on_call_post(unsigned long tid, unsigned long pc,
    unsigned long func)
{
    CALLBACK_CALL(on_call_post, tid, pc, func);
}
EXPORT_SYMBOL(stub_on_call_post);

void stub_begin_memory_events(unsigned long tid, unsigned long num_events, 
    void **pdata /* out param*/)
{
    CALLBACK_CALL(begin_memory_events, tid, num_events, pdata);
}
EXPORT_SYMBOL(stub_begin_memory_events);
void stub_end_memory_events(unsigned long tid, void *data)
{
    CALLBACK_CALL(end_memory_events, tid, data);
}
EXPORT_SYMBOL(stub_end_memory_events);
void stub_on_memory_event(unsigned long tid, 
    unsigned long pc, unsigned long addr, unsigned long size, 
    enum kedr_memory_event_type type,
    void *data)
{
    CALLBACK_CALL(on_memory_event, tid, pc, addr, size, type, data);
}
EXPORT_SYMBOL(stub_on_memory_event);

void stub_on_locked_op_pre(unsigned long tid, unsigned long pc, 
    void **pdata)
{
    CALLBACK_CALL(on_locked_op_pre, tid, pc, pdata);
}
EXPORT_SYMBOL(stub_on_locked_op_pre);
void stub_on_locked_op_post(unsigned long tid, unsigned long pc, 
    unsigned long addr, unsigned long size, 
    enum kedr_memory_event_type type, void *data)
{
    CALLBACK_CALL(on_locked_op_post, tid, pc, addr, size, type, data);
}
EXPORT_SYMBOL(stub_on_locked_op_post);

void stub_on_io_mem_op_pre(unsigned long tid, unsigned long pc, 
    void **pdata)
{
    CALLBACK_CALL(on_io_mem_op_pre, tid, pc, pdata);
}
EXPORT_SYMBOL(stub_on_io_mem_op_pre);
void stub_on_io_mem_op_post(unsigned long tid, unsigned long pc, 
    unsigned long addr, unsigned long size, 
    enum kedr_memory_event_type type, void *data)
{
    CALLBACK_CALL(on_io_mem_op_post, tid, pc, addr, size, type, data);
}
EXPORT_SYMBOL(stub_on_io_mem_op_post);

void stub_on_memory_barrier_pre(unsigned long tid, unsigned long pc, 
    enum kedr_barrier_type type)
{
    CALLBACK_CALL(on_memory_barrier_pre, tid, pc, type);
}
EXPORT_SYMBOL(stub_on_memory_barrier_pre);

void stub_on_memory_barrier_post(unsigned long tid, unsigned long pc, 
    enum kedr_barrier_type type)
{
    CALLBACK_CALL(on_memory_barrier_post, tid, pc, type);
}
EXPORT_SYMBOL(stub_on_memory_barrier_post);

void stub_on_alloc_pre(unsigned long tid, unsigned long pc, 
    unsigned long size)
{
    CALLBACK_CALL(on_alloc_pre, tid, pc, size);
}
EXPORT_SYMBOL(stub_on_alloc_pre);
void stub_on_alloc_post(unsigned long tid, unsigned long pc, 
    unsigned long size, unsigned long addr)
{
    CALLBACK_CALL(on_alloc_post, tid, pc, size, addr);
}
EXPORT_SYMBOL(stub_on_alloc_post);
void stub_on_free_pre(unsigned long tid, unsigned long pc, 
    unsigned long addr)
{
    CALLBACK_CALL(on_free_pre, tid, pc, addr);
}
EXPORT_SYMBOL(stub_on_free_pre);
void stub_on_free_post(unsigned long tid, unsigned long pc, 
    unsigned long addr)
{
    CALLBACK_CALL(on_free_post, tid, pc, addr);
}
EXPORT_SYMBOL(stub_on_free_post);

void stub_on_lock_pre(unsigned long tid, unsigned long pc, 
    unsigned long lock_id, enum kedr_lock_type type)
{
    CALLBACK_CALL(on_lock_pre, tid, pc, lock_id, type);
}
EXPORT_SYMBOL(stub_on_lock_pre);
void stub_on_lock_post(unsigned long tid, unsigned long pc, 
    unsigned long lock_id, enum kedr_lock_type type)
{
    CALLBACK_CALL(on_lock_post, tid, pc, lock_id, type);
}
EXPORT_SYMBOL(stub_on_lock_post);
void stub_on_unlock_pre(unsigned long tid, unsigned long pc, 
    unsigned long lock_id, enum kedr_lock_type type)
{
    CALLBACK_CALL(on_unlock_pre, tid, pc, lock_id, type);
}
EXPORT_SYMBOL(stub_on_unlock_pre);
void stub_on_unlock_post(unsigned long tid, unsigned long pc, 
    unsigned long lock_id, enum kedr_lock_type type)
{
    CALLBACK_CALL(on_lock_post, tid, pc, lock_id, type);
}
EXPORT_SYMBOL(stub_on_unlock_post);

void stub_on_signal_pre(unsigned long tid, unsigned long pc, 
    unsigned long obj_id, enum kedr_sw_object_type type)
{
    CALLBACK_CALL(on_signal_pre, tid, pc, obj_id, type);
}
EXPORT_SYMBOL(stub_on_signal_pre);
void stub_on_signal_post(unsigned long tid, unsigned long pc, 
    unsigned long obj_id, enum kedr_sw_object_type type)
{
    CALLBACK_CALL(on_signal_post, tid, pc, obj_id, type);
}
EXPORT_SYMBOL(stub_on_signal_post);
void stub_on_wait_pre(unsigned long tid, unsigned long pc, 
    unsigned long obj_id, enum kedr_sw_object_type type)
{
    CALLBACK_CALL(on_wait_pre, tid, pc, obj_id, type);
}
EXPORT_SYMBOL(stub_on_wait_pre);
void stub_on_wait_post(unsigned long tid, unsigned long pc, 
	unsigned long obj_id, enum kedr_sw_object_type type)
{
    CALLBACK_CALL(on_wait_post, tid, pc, obj_id, type);
}
EXPORT_SYMBOL(stub_on_wait_post);

void stub_on_thread_create_pre(unsigned long tid, unsigned long pc)
{
    CALLBACK_CALL(on_thread_create_pre, tid, pc);
}
EXPORT_SYMBOL(stub_on_thread_create_pre);
void stub_on_thread_create_post(unsigned long tid, unsigned long pc, 
    unsigned long child_tid)
{
    CALLBACK_CALL(on_thread_create_post, tid, pc, child_tid);
}
EXPORT_SYMBOL(stub_on_thread_create_post);
void stub_on_thread_join_pre(unsigned long tid, unsigned long pc, 
    unsigned long child_tid)
{
    CALLBACK_CALL(on_thread_join_pre, tid, pc, child_tid);
}
EXPORT_SYMBOL(stub_on_thread_join_pre);
void stub_on_thread_join_post(unsigned long tid, unsigned long pc, 
    unsigned long child_tid)
{
    CALLBACK_CALL(on_thread_join_post, tid, pc, child_tid);
}
EXPORT_SYMBOL(stub_on_thread_join_post);

static int __init core_stub_init(void)
{
    return 0;
}

static void __exit core_stub_exit(void)
{
    return;
}

module_init(core_stub_init);
module_exit(core_stub_exit);