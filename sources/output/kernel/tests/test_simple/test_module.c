/*
 * Test that trace sender can send events from event collector.
 */

#include "core_stub_api.h"

#include <linux/module.h>
#include <linux/mutex.h>

MODULE_AUTHOR("Andrey Tsyvarev");
MODULE_LICENSE("GPL");

static int test(void)
{
    unsigned long tid1 = 0x123456;
    unsigned long tid2 = 0x654321;
    
    unsigned long func1 = 0x4000;
    unsigned long func2_1 = 0x6000;
    unsigned long func2_2 = 0x8000;
    
    void* data;
    
    /* External call in T2 */
    stub_on_call_pre(tid2, 0x543, func2_1);

    /* Internal call in T1*/
    stub_on_call_pre(tid1, 0x500, func1);
    stub_on_function_entry(tid1, func1);
    
    /* Some memory accesses in T1 */
    stub_begin_memory_events(tid1, 3, &data);
    stub_on_memory_event(tid1, 0x4056, 0x10000, 123, KEDR_ET_MREAD, data);
    stub_on_memory_event(tid1, 0x4060, 0x3000, 2, KEDR_ET_MWRITE, data);
    stub_on_memory_event(tid1, 0x4100, 0x1002, 8, KEDR_ET_MUPDATE, data);
    stub_end_memory_events(tid1, data);
    
    /* Call from outside in T2 */
    stub_on_function_entry(tid2, func2_2);

    /* Memory allocation in T2 */
    stub_on_alloc_pre(tid2, 0x6100, 345);
    stub_on_alloc_post(tid2, 0x6100, 345, 0x7654);

    /* Return to outside in T2 */
    stub_on_function_exit(tid2, func2_2);
    
    /* Lock in T1 */
    stub_on_lock_pre(tid1, 0x834, 0x100, KEDR_LT_SPINLOCK);
    stub_on_lock_post(tid1, 0x834, 0x100, KEDR_LT_SPINLOCK);
    
    /* Free inside lock in T1 */
    stub_on_free_pre(tid1, 0x9432, 0x1234);
    stub_on_free_post(tid1, 0x9432, 0x1234);
    
    /* Release lock in T1 */
    stub_on_unlock_pre(tid1, 0x9876, 0x100, KEDR_LT_SPINLOCK);
    stub_on_unlock_post(tid1, 0x9876, 0x100, KEDR_LT_SPINLOCK);
    
    /* Locked memory access in T2 */
    stub_on_locked_op_pre(tid2, 0x543, &data);
    stub_on_locked_op_post(tid2, 0x543, 0x2567, 543, KEDR_ET_MUPDATE, data);
    
    /* cmpxchng-like operation in T1, unexpected value */
    stub_on_locked_op_pre(tid1, 0x543, &data);
    stub_on_locked_op_post(tid1, 0x543, 0x2567, 4, KEDR_ET_MREAD, data);
    
    /* External call in T2 returns */
    stub_on_call_post(tid2, 0x543, func2_1);
    
    /* IO operation (with barrier) in T2 */
    stub_on_io_mem_op_pre(tid2, 0x3945, &data);
    stub_on_io_mem_op_post(tid2, 0x3945, 0x4532, 1000, KEDR_ET_MWRITE, data);
    
    //TODO: other operations
    
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