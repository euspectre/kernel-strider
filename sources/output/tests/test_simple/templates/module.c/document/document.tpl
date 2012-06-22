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

#define generate_event_function_entry(tid, func) \
	record_function_entry(tid, func)

#define generate_event_function_exit(tid, func) \
	record_function_exit(tid, func)

#define generate_event_function_call_pre(tid, pc, func) \
	record_function_fcpre(tid, pc, func)

#define generate_event_function_call_post(tid, pc, func) \
	record_function_fcpost(tid, pc, func)

#define generate_event_begin_memory_accesses(tid, n_accesses) \
	record_memory_accesses_begin(tid, n_accesses, &data);
#define generate_subevent_memory_accesses(pc, addr, size, access_type) \
	record_memory_access_next(data, pc, addr, size, access_type)
#define generate_event_end_memory_accesses() \
	record_memory_accesses_end(data)

#define generate_event_locked_op(tid, pc, addr, size, type) \
	record_function_lma(tid, pc, addr, size, type)

#define generate_event_io_mem_op(tid, pc, addr, size, type) \
	record_function_ioma(tid, pc, addr, size, type)

#define generate_memory_barrier(tid, pc, type) \
	record_function_mb(tid, pc, type)

    
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
