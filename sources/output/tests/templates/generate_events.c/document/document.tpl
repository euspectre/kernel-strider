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
    unsigned long _tid_copy;
	(void)data;
	(void)_tid_copy;

#define generate_event_function_entry(tid, func) \
	stub_on_function_entry(tid, func)

#define generate_event_function_exit(tid, func) \
	stub_on_function_exit(tid, func)

#define generate_event_call_pre(tid, pc, func) \
	stub_on_call_pre(tid, pc, func)

#define generate_event_call_post(tid, pc, func) \
	stub_on_call_post(tid, pc, func)

#define generate_event_begin_memory_events(tid, n_accesses) \
	stub_on_begin_memory_events(tid, n_accesses, &data); \
	_tid_copy = tid;
#define generate_subevent_memory_events(pc, addr, size, access_type) \
	stub_on_memory_event(_tid_copy, pc, addr, size, access_type, data)
#define generate_event_end_memory_events() \
	stub_on_end_memory_events(_tid_copy, data)

#define generate_event_locked_op(tid, pc, addr, size, type) \
	stub_on_locked_op_pre(tid, pc, &data); \
	stub_on_locked_op_post(tid, pc, addr, size, type, data)

#define generate_event_io_mem_op(tid, pc, addr, size, type) \
	stub_on_io_mem_op_pre(tid, pc, &data); \
	stub_on_io_mem_op_post(tid, pc, addr, size, type, data)

#define generate_event_memory_barrier(tid, pc, type) \
	stub_on_memory_barrier_pre(tid, pc, type); \
	stub_on_memory_barrier_post(tid, pc, type)

#define generate_event_alloc(tid, pc, size, addr) \
	stub_on_alloc_pre(tid, pc, size); \
	stub_on_alloc_post(tid, pc, size, addr)

#define generate_event_free(tid, pc, addr) \
	stub_on_free_pre(tid, pc, addr); \
	stub_on_free_post(tid, pc, addr)

#define generate_event_lock(tid, pc, lock_id, type) \
    stub_on_lock_pre(tid, pc, lock_id, type); \
	stub_on_lock_post(tid, pc, lock_id, type)
    
#define generate_event_unlock(tid, pc, lock_id, type) \
    stub_on_unlock_pre(tid, pc, lock_id, type); \
	stub_on_unlock_post(tid, pc, lock_id, type)

#define generate_event_signal(tid, pc, obj_id, type) \
    stub_on_signal_pre(tid, pc, obj_id, type); \
	stub_on_signal_post(tid, pc, obj_id, type)

#define generate_event_wait(tid, pc, obj_id, type) \
    stub_on_wait_pre(tid, pc, obj_id, type); \
	stub_on_wait_post(tid, pc, obj_id, type)

#define generate_event_thread_create(tid, pc, child_tid) \
    stub_on_thread_create_pre(tid, pc); \
	stub_on_thread_create_post(tid, pc, child_tid)

#define generate_event_thread_join(tid, pc, child_tid) \
    stub_on_thread_join_pre(tid, pc, child_tid); \
	stub_on_thread_join_post(tid, pc, child_tid)


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
