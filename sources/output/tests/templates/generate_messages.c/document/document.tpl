/*
 * Test that trace sender send events and trace receiver correctly
 * receives them.
 */

#include <kedr/output/event_collector.h>
#include "core_stub_api.h"

#include <linux/module.h>
#include <linux/mutex.h>

MODULE_AUTHOR("Andrey Tsyvarev");
MODULE_LICENSE("GPL");

static int test(void)
{
    <$constant_def: join(\n\t)$>
    /* Used for generate memory accesses events */
    void* data;
    (void)data;

#define generate_message_function_entry(tid, func) \
	record_function_entry(tid, func)

#define generate_message_function_exit(tid, func) \
	record_function_exit(tid, func)

#define generate_message_call_pre(tid, pc, func) \
	record_function_call_pre(tid, pc, func)

#define generate_message_call_post(tid, pc, func) \
	record_function_call_post(tid, pc, func)

#define generate_message_begin_memory_events(tid, n_accesses) \
	record_memory_accesses_begin(tid, n_accesses, &data);
#define generate_submessage_memory_events(pc, addr, size, access_type) \
	record_memory_access_next(data, pc, addr, size, access_type)
#define generate_message_end_memory_events() \
	record_memory_accesses_end(data)

#define generate_message_locked_op(tid, pc, addr, size, type) \
	record_locked_memory_access(tid, pc, addr, size, type)

#define generate_message_io_mem_op(tid, pc, addr, size, type) \
	record_io_memory_access(tid, pc, addr, size, type)

#define generate_message_memory_barrier(tid, pc, type) \
	record_memory_barrier(tid, pc, type)

#define generate_message_alloc(tid, pc, size, addr) \
	record_alloc(tid, pc, size, addr)

#define generate_message_free(tid, pc, addr) \
	record_free(tid, pc, addr)

#define generate_message_lock(tid, pc, lock_id, type) \
    record_lock(tid, pc, lock_id, type)
    
#define generate_message_unlock(tid, pc, lock_id, type) \
    record_unlock(tid, pc, lock_id, type)

#define generate_message_signal(tid, pc, obj_id, type) \
    record_signal(tid, pc, obj_id, type)

#define generate_message_wait(tid, pc, obj_id, type) \
    record_wait(tid, pc, obj_id, type)

#define generate_message_thread_create_before(tid, pc) \
    record_thread_create_before(tid, pc)

#define generate_message_thread_create_after(tid, pc, child_tid) \
    record_thread_create_after(tid, pc, child_tid)


#define generate_message_thread_join(tid, pc, child_tid) \
    record_thread_join(tid, pc, child_tid)


    <$block: join(\n\n)$>
    
    return 0;
}


static int __init test_module_init(void)
{
    int result;
    stub_on_target_loaded(THIS_MODULE);
    
    result = test();
    if(result)
    {
        stub_on_target_about_to_unload(THIS_MODULE);
        return result;
    }
    
    return 0;
}

static void __exit test_module_exit(void)
{
    stub_on_target_about_to_unload(THIS_MODULE);
}

module_init(test_module_init);
module_exit(test_module_exit);
