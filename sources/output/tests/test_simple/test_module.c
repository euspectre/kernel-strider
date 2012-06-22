/*
 * Test that trace sender can send events from event collector.
 */

#include <kedr/output/event_collector.h>
#include "core_stub_api.h"

#include <linux/module.h>
#include <linux/mutex.h>

MODULE_AUTHOR("Andrey Tsyvarev");
MODULE_LICENSE("GPL");

static int test0(void)
{
    unsigned long tid1 = 0x123456;
    unsigned long tid2 = 0x654321;
    
    unsigned long func1 = 0x4000;
    unsigned long func2_1 = 0x6000;
    unsigned long func2_2 = 0x8000;
    
    void* data;
    
    /* External call in T2 */
    //record_function_call_pre(tid2, 0x543, func2_1);

    /* Internal call in T1*/
    //record_function_call_pre(tid1, 0x500, func1);
    //record_function_entry(tid1, func1);
    
    /* Some memory accesses in T1 */
    record_memory_accesses_begin(tid1, 3, &data);
    record_memory_access_next(data, 0x4056, 0x10000, 123, KEDR_ET_MREAD);
    record_memory_access_next(data, 0x4060, 0x3000, 2, KEDR_ET_MWRITE);
    record_memory_access_next(data, 0x4100, 0x1002, 8, KEDR_ET_MUPDATE);
    record_memory_accesses_end(data);
    
    /* Call from outside in T2 */
    //record_function_entry(tid2, func2_2);

    /* Memory allocation in T2 */
    //record_alloc(tid2, 0x6100, 345, 0x7654);

    /* Return to outside in T2 */
    
    //record_function_exit(tid2, func2_2);
    
    /* Lock in T1 */
    //record_lock(tid1, 0x834, 0x100, KEDR_LT_SPINLOCK);
    
    /* Free inside lock in T1 */
    //record_free(tid1, 0x9432, 0x1234);
    
    /* Release lock in T1 */
    record_unlock(tid1, 0x9876, 0x100, KEDR_LT_SPINLOCK);
    
    /* Locked memory access in T2 */
    //record_locked_memory_access(tid2, 0x543, 0x2567, 543);
    
    /* cmpxchng-like operation in T1, unexpected value */
    record_memory_access_one(tid1, 0x543, 0x2567, 4, KEDR_ET_MREAD);
    
    /* External call in T2 returns */
    //record_function_call_post(tid2, 0x543, func2_1);
    
    /* IO operation (with barrier) in T2 */
    //stub_on_io_mem_op_pre(tid2, 0x3945, &data);
    //stub_on_io_mem_op_post(tid2, 0x3945, 0x4532, 1000, KEDR_ET_MWRITE, data);
    
    //TODO: other operations
    
    return 0;
}

static int test(void)
{
    unsigned long tid1 = 0x123456;
    unsigned long tid2 = 0x654321;
    
    unsigned long func1 = 0x4000;
    unsigned long func2_1 = 0x6000;
    unsigned long func2_2 = 0x8000;
    
    void* data;
    
    /* External call in T2 */
    //record_function_call_pre(tid2, 0x543, func2_1);

    /* Internal call in T1*/
    //record_function_call_pre(tid1, 0x500, func1);
    record_function_entry(tid1, func1);
    
    /* Some memory accesses in T1 */
    record_memory_accesses_begin(tid1, 3, &data);
    record_memory_access_next(data, 0x4056, 0x10000, 123, KEDR_ET_MREAD);
    record_memory_access_next(data, 0x4060, 0x3000, 2, KEDR_ET_MWRITE);
    record_memory_access_next(data, 0x4100, 0x1002, 8, KEDR_ET_MUPDATE);
    record_memory_accesses_end(data);
    
    /* Call from outside in T2 */
    record_function_entry(tid2, func2_2);

    /* Memory allocation in T2 */
    record_alloc(tid2, 0x6100, 345, 0x7654);

    /* Return to outside in T2 */
    
    record_function_exit(tid2, func2_2);
    
    /* Lock in T1 */
    record_lock(tid1, 0x834, 0x100, KEDR_LT_SPINLOCK);
    
    /* Free inside lock in T1 */
    record_free(tid1, 0x9432, 0x1234);
    
    /* Release lock in T1 */
    record_unlock(tid1, 0x9876, 0x100, KEDR_LT_SPINLOCK);
    
    /* Locked memory access in T2 */
    record_locked_memory_access(tid2, 0x543, 0x2567, 543);
    
    /* cmpxchng-like operation in T1, unexpected value */
    record_memory_access_one(tid1, 0x543, 0x2567, 4, KEDR_ET_MREAD);
    
    /* External call in T2 returns */
    //record_function_call_post(tid2, 0x543, func2_1);
    
    /* IO operation (with barrier) in T2 */
    //stub_on_io_mem_op_pre(tid2, 0x3945, &data);
    //stub_on_io_mem_op_post(tid2, 0x3945, 0x4532, 1000, KEDR_ET_MWRITE, data);
    
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
