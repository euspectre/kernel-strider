/*
 * Test that event collector really collect needed messages with
 * some callback and API calls.
 */

#include "kedr/event_collector/event_collector.h"
#include "core_stub_api.h"
#include "handler_stub_api.h"

#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/slab.h>

MODULE_AUTHOR("Andrey Tsyvarev");
MODULE_LICENSE("GPL");


/************************* Helpers *********************************/


static int current_error = 0;

/* 
 * Set error in test without breaking its execution.
 * 
 * Do not change error if it is already set.
 */
static void set_error(int error)
{
    if(!current_error) current_error = error;
}

/*
 * Return error if it was set or 0.
 */
static int get_error(void)
{
    return current_error;
}

/* Make buffer empty */
static void empty_buffer(void)
{
    int size;
    struct execution_message_base* msg;

    for(size = handler_stub_get_message(&msg);
        size >= 0;
        size = handler_stub_get_message(&msg))
    {
        enum execution_message_type type =
            handler_stub_get_msg_type(msg, size);
        if(type == execution_message_type_invalid)
        {
            pr_err("Incorrect format of message in buffer.");
            set_error(-EINVAL);
        }
        kfree(msg);
    }
}

/* 
 * Extract message of one of the given types from buffer.
 * 
 * Ignore all others messages.
 * 
 * Array of types may contain execution_message_type_invalid,
 * this type is not checked.
 * 
 * Return index of type for the message extracted or negative error code.
 * If no message is found return -EAGAIN.
 */
static int
extract_typed_message(enum execution_message_type* types,
    int n_types, struct execution_message_base** msg_p)
{
    int size;
    struct execution_message_base* msg;

    for(size = handler_stub_get_message(&msg);
        size >= 0;
        size = handler_stub_get_message(&msg))
    {
        int i;
        enum execution_message_type type =
            handler_stub_get_msg_type(msg, size);
        if(type == execution_message_type_invalid)
        {
            pr_err("Incorrect format of message in buffer.");
            set_error(-EINVAL);
            /* Non-fatal error */
            continue;
        }
        for(i = 0; i < n_types; i++)
        {
            if(types[i] == execution_message_type_invalid) continue;
            if(type == types[i])
            {
                *msg_p = msg;
                return i;
            }
        }
        kfree(msg);
    }
    return size;
}

/* Shortcat for one message type */
#define extract_typed_message1(type_suff, msg) ({                          \
    enum execution_message_type _type = execution_message_type_##type_suff;      \
    extract_typed_message(&_type, 1, (struct execution_message_base**)&msg);\
})


static int test(void)
{
    int result;
    
    unsigned long tid1 = 0x123456;
    unsigned long tid2 = 0x654321;
    
    unsigned long func1 = 0x4000;
    unsigned long func2_1 = 0x6000;
    unsigned long func2_2 = 0x8000;
    
    void* data;

    /* Check that handler is active */
    if(!handler_stub_is_used())
    {
        pr_err("Event handler failed to detect module loading.");
        return -EINVAL;
    }
    
/*
 * Check that given field of typed message has given value.
 * 
 * If no - free message and return -EINVAL.
 */
#define CHECK_MSG_FIELD(_msg, _field, _val) do {            \
if(_msg->_field != _val){                                   \
    pr_err("Incorrect field '%s' of message of type %d.",   \
        #_field, (int)_msg->base.type);                     \
    kfree(_msg);                                            \
    return -EINVAL;                                         \
}}while(0)

    /* Empty buffer before all operations */
    empty_buffer();
    
    /* External call in T2 */
    stub_on_call_pre(tid2, 0x543, func2_1);
    {
        struct execution_message_fc* msg_fc;
        result = extract_typed_message1(fcpre, msg_fc);
        if(result < 0)
        {
            pr_err("After 'on_call_pre' callback message of type 'fcpre' should be in buffer");
            return -EINVAL;
        }
        CHECK_MSG_FIELD(msg_fc, base.tid, tid2);
        CHECK_MSG_FIELD(msg_fc, pc, 0x543);
        CHECK_MSG_FIELD(msg_fc, func, func2_1);
        kfree(msg_fc);
    }
    
    empty_buffer();
    /* Internal call in T1*/
    stub_on_call_pre(tid1, 0x500, func1);
    {
        struct execution_message_fc* msg_fc;
        result = extract_typed_message1(fcpre, msg_fc);
        if(result < 0)
        {
            pr_err("After 'on_call_pre' callback message of type 'fcpre' should be in buffer");
            return -EINVAL;
        }
        CHECK_MSG_FIELD(msg_fc, base.tid, tid1);
        CHECK_MSG_FIELD(msg_fc, pc, 0x500);
        CHECK_MSG_FIELD(msg_fc, func, func1);
        kfree(msg_fc);
    }
    
    
    empty_buffer();
    stub_on_function_entry(tid1, func1);
    {
        struct execution_message_fee* msg_fee;
        result = extract_typed_message1(fentry, msg_fee);
        if(result < 0)
        {
            pr_err("After 'on_function_entry' callback message of type 'fentry' should be in buffer");
            return -EINVAL;
        }
        CHECK_MSG_FIELD(msg_fee, base.tid, tid1);
        CHECK_MSG_FIELD(msg_fee, func, func1);
        kfree(msg_fee);
    }
    
    empty_buffer();
    /* Some memory accesses in T1 */
    stub_begin_memory_events(tid1, 3, &data);
    stub_on_memory_event(tid1, 0x4056, 0x10000, 123, KEDR_ET_MREAD, data);
    stub_on_memory_event(tid1, 0x4060, 0x3000, 2, KEDR_ET_MWRITE, data);
    stub_on_memory_event(tid1, 0x4100, 0x1002, 8, KEDR_ET_MUPDATE, data);
    stub_end_memory_events(tid1, data);
    {
        struct execution_message_ma* msg_ma;
        result = extract_typed_message1(ma, msg_ma);
        if(result < 0)
        {
            pr_err("Message of type 'ma' should be in buffer "
                "after registering memory accesses.");
            return -EINVAL;
        }
        CHECK_MSG_FIELD(msg_ma, base.tid, tid1);
        CHECK_MSG_FIELD(msg_ma, n_subevents, 3);
        kfree(msg_ma);
    }
    

    /* Call from outside in T2 */
    stub_on_function_entry(tid2, func2_2);

    empty_buffer();
    /* Memory allocation in T2 */
    stub_on_alloc_pre(tid2, 0x6100, 345);
    stub_on_alloc_post(tid2, 0x6100, 345, 0x7654);
    {
        struct execution_message_alloc* msg_alloc;
        result = extract_typed_message1(alloc, msg_alloc);
        if(result < 0)
        {
            pr_err("Message of type 'alloc' should be in buffer "
                "after registering memory allocation.");
            return -EINVAL;
        }
        CHECK_MSG_FIELD(msg_alloc, base.tid, tid2);
        CHECK_MSG_FIELD(msg_alloc, size, 345);
        CHECK_MSG_FIELD(msg_alloc, pointer, 0x7654);
        kfree(msg_alloc);
    }

    /* Return to outside in T2 */
    stub_on_function_exit(tid2, func2_2);
    
    empty_buffer();
    /* Lock in T1 */
    stub_on_lock_pre(tid1, 0x834, 0x100, KEDR_LT_SPINLOCK);
    stub_on_lock_post(tid1, 0x834, 0x100, KEDR_LT_SPINLOCK);
    {
        struct execution_message_lock* msg_lock;
        result = extract_typed_message1(lock, msg_lock);
        if(result < 0)
        {
            pr_err("Message of type 'lock' should be in buffer "
                "after registering lock.");
            return -EINVAL;
        }
        CHECK_MSG_FIELD(msg_lock, base.tid, tid1);
        CHECK_MSG_FIELD(msg_lock, pc, 0x834);
        CHECK_MSG_FIELD(msg_lock, obj, 0x100);
        CHECK_MSG_FIELD(msg_lock, type, KEDR_LT_SPINLOCK);
        kfree(msg_lock);
    }
    
    empty_buffer();
    /* Free inside lock in T1 */
    stub_on_free_pre(tid1, 0x9432, 0x1234);
    stub_on_free_post(tid1, 0x9432, 0x1234);
    {
        struct execution_message_free* msg_free;
        result = extract_typed_message1(free, msg_free);
        if(result < 0)
        {
            pr_err("Message of type 'free' should be in buffer "
                "after registering freeing memory.");
            return -EINVAL;
        }
        CHECK_MSG_FIELD(msg_free, base.tid, tid1);
        CHECK_MSG_FIELD(msg_free, pc, 0x9432);
        CHECK_MSG_FIELD(msg_free, pointer, 0x1234);
        kfree(msg_free);
    }

    empty_buffer();
    /* Release lock in T1 */
    stub_on_unlock_pre(tid1, 0x9876, 0x100, KEDR_LT_SPINLOCK);
    stub_on_unlock_post(tid1, 0x9876, 0x100, KEDR_LT_SPINLOCK);
    {
        struct execution_message_lock* msg_lock;
        result = extract_typed_message1(unlock, msg_lock);
        if(result < 0)
        {
            pr_err("Message of type 'unlock' should be in buffer "
                "after registering unlock.");
            return -EINVAL;
        }
        CHECK_MSG_FIELD(msg_lock, base.tid, tid1);
        CHECK_MSG_FIELD(msg_lock, pc, 0x9876);
        CHECK_MSG_FIELD(msg_lock, obj, 0x100);
        CHECK_MSG_FIELD(msg_lock, type, KEDR_LT_SPINLOCK);
        kfree(msg_lock);
    }

    
    /* Locked memory access in T2 */
    stub_on_locked_op_pre(tid2, 0x543, &data);
    stub_on_locked_op_post(tid2, 0x543, 0x2567, 543, KEDR_ET_MUPDATE, data);
    {
        struct execution_message_lma* msg_lma;
        result = extract_typed_message1(lma, msg_lma);
        if(result < 0)
        {
            pr_err("Message of type 'lma' should be in buffer "
                "after registering locked memory access.");
            return -EINVAL;
        }
        CHECK_MSG_FIELD(msg_lma, base.tid, tid2);
        CHECK_MSG_FIELD(msg_lma, pc, 0x543);
        CHECK_MSG_FIELD(msg_lma, addr, 0x2567);
        CHECK_MSG_FIELD(msg_lma, size, 543);
        kfree(msg_lma);
    }
    
    empty_buffer();
    /* cmpxchng-like operation in T1, unexpected value */
    stub_on_locked_op_pre(tid1, 0x543, &data);
    stub_on_locked_op_post(tid1, 0x543, 0x2567, 4, KEDR_ET_MREAD, data);
    {
        struct execution_message_ma* msg_ma;
        result = extract_typed_message1(ma, msg_ma);
        if(result < 0)
        {
            pr_err("Message of type 'ma' should be in buffer "
                "after registering locked memory access which do not write.");
            return -EINVAL;
        }
        CHECK_MSG_FIELD(msg_ma, base.tid, tid1);
        CHECK_MSG_FIELD(msg_ma, n_subevents, 1);
        CHECK_MSG_FIELD(msg_ma, subevents[0].pc, 0x543);
        CHECK_MSG_FIELD(msg_ma, subevents[0].addr, 0x2567);
        CHECK_MSG_FIELD(msg_ma, subevents[0].size, 4);
        CHECK_MSG_FIELD(msg_ma, subevents[0].access_type, KEDR_ET_MREAD);
        kfree(msg_ma);
    }
    {
        struct execution_message_mb* msg_mb;
        result = extract_typed_message1(mrb, msg_mb);
        if(result < 0)
        {
            pr_err("Message of type 'mrb' should be in buffer "
                "just after registering locked memory access which do not write.");
            return -EINVAL;
        }
        CHECK_MSG_FIELD(msg_mb, base.tid, tid1);
        CHECK_MSG_FIELD(msg_mb, pc, 0x543);
        kfree(msg_mb);
    }
    
    empty_buffer();
    /* External call in T2 returns */
    stub_on_call_post(tid2, 0x543, func2_1);
    {
        struct execution_message_fc* msg_fc;
        result = extract_typed_message1(fcpost, msg_fc);
        if(result < 0)
        {
            pr_err("After 'on_call_post' callback message of type 'fcpost' should be in buffer");
            return -EINVAL;
        }
        CHECK_MSG_FIELD(msg_fc, base.tid, tid2);
        CHECK_MSG_FIELD(msg_fc, pc, 0x543);
        CHECK_MSG_FIELD(msg_fc, func, func2_1);
        kfree(msg_fc);
    }

    empty_buffer();
    /* IO operation (with barrier) in T2 */
    stub_on_io_mem_op_pre(tid2, 0x3945, &data);
    stub_on_io_mem_op_post(tid2, 0x3945, 0x4532, 1000, KEDR_ET_MWRITE, data);
    {
        struct execution_message_mb* msg_mb;
        result = extract_typed_message1(mwb, msg_mb);
        if(result < 0)
        {
            pr_err("Message of type 'mwb' should be in buffer "
                "just before registering IO operation.");
            return -EINVAL;
        }
        CHECK_MSG_FIELD(msg_mb, base.tid, tid2);
        CHECK_MSG_FIELD(msg_mb, pc, 0x3945);
        kfree(msg_mb);
    }
    {
        struct execution_message_ma* msg_ma;
        result = extract_typed_message1(ma, msg_ma);
        if(result < 0)
        {
            pr_err("Message of type 'ma' should be in buffer "
                "when registering IO operation which access memory.");
            return -EINVAL;
        }
        CHECK_MSG_FIELD(msg_ma, base.tid, tid2);
        CHECK_MSG_FIELD(msg_ma, n_subevents, 1);
        CHECK_MSG_FIELD(msg_ma, subevents[0].pc, 0x3945);
        CHECK_MSG_FIELD(msg_ma, subevents[0].addr, 0x4532);
        CHECK_MSG_FIELD(msg_ma, subevents[0].size, 1000);
        CHECK_MSG_FIELD(msg_ma, subevents[0].access_type, KEDR_ET_MWRITE);
        kfree(msg_ma);
    }
    {
        struct execution_message_mb* msg_mb;
        result = extract_typed_message1(mrb, msg_mb);
        if(result < 0)
        {
            pr_err("Message of type 'mrb' should be in buffer "
                "just after registering IO operation.");
            return -EINVAL;
        }
        CHECK_MSG_FIELD(msg_mb, base.tid, tid2);
        CHECK_MSG_FIELD(msg_mb, pc, 0x3945);
        kfree(msg_mb);
    }

    //TODO: other operations
    
    /* Check non-fatal errors at the end */
    return get_error();
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