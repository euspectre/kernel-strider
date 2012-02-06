#include "kedr/event_collector/event_handler.h"

#include "handler_stub_api.h"

#include <linux/slab.h> /* kmalloc */

#include <linux/module.h>

MODULE_AUTHOR("Andrey Tsyvarev");
MODULE_LICENSE("GPL");

static struct execution_event_collector* local_current_collector = NULL;

int handler_stub_is_used(void)
{
    return local_current_collector != NULL;
}
EXPORT_SYMBOL(handler_stub_is_used);


/* Callback for trace_buffer_read_message */
static int handler_stub_process_data(const void* msg, size_t size,
    int cpu, u64 ts, bool *consume, void* user_data)
{
    void* allocated_msg = kmalloc(size, GFP_KERNEL);
    if(allocated_msg == NULL)
    {
        pr_err("Failed to allocate buffer for message.");
        return -ENOMEM;
    }
    memcpy(allocated_msg, msg, size);
    *((void**)user_data) = allocated_msg;
    *consume = 1;
    
    return (int)size;
}


int handler_stub_get_message(struct execution_message_base** msg)
{
    int result;
    BUG_ON(local_current_collector == NULL);
     
    result = execution_event_collector_read_message(
        local_current_collector, &handler_stub_process_data, (void*)msg);
    
    /* Currently execution_event_collector_read_message() return 0 if buffer is empty */
    if(result == 0) return -EAGAIN;
    
    return result;
}
EXPORT_SYMBOL(handler_stub_get_message);

enum execution_message_type
handler_stub_get_msg_type(struct execution_message_base* msg, int size)
{
    int expected_size;
    if(size < sizeof(*msg))
    {
        pr_err("Size of the message is less than size of base structure");
        return execution_message_type_invalid;
    }
    switch(msg->type)
    {
    case execution_message_type_ma:
        if(size < sizeof(struct execution_message_ma))
        {
            pr_err("Size of the message is less then size of base message "
                "with memory access.");
            return execution_message_type_invalid;
        }
        expected_size = sizeof(struct execution_message_ma)
            + sizeof(struct execution_message_ma_subevent)
            * ((struct execution_message_ma*)msg)->n_subevents;
        if(size != expected_size)
        {
            pr_err("Incorrect size of message of type 'execution_message_type_ma': "
                "should be %d, but it is %d.", expected_size, size);
        }
    break;
/* Checker for simple message type(when type imply size) */
#define CASE_SIMPLE_TYPE(msuff, ssuff)                                  \
    case execution_message_type_##msuff:                                \
        if(size < sizeof(struct execution_message_##ssuff))             \
        {                                                               \
            pr_err("Incorrect size of message of type '%s':"            \
                "should be %d, but it is %d.",                          \
                "execution_message_type_"#msuff,                        \
                (int)sizeof(struct execution_message_##ssuff), size);   \
            return execution_message_type_invalid;                      \
        }                                                               \
    break
    
    CASE_SIMPLE_TYPE(lma, lma);
    
    CASE_SIMPLE_TYPE(mrb, mb);
    CASE_SIMPLE_TYPE(mwb, mb);
    CASE_SIMPLE_TYPE(mfb, mb);
    
    CASE_SIMPLE_TYPE(alloc, alloc);
    CASE_SIMPLE_TYPE(free, free);
    
    CASE_SIMPLE_TYPE(lock, lock);
    CASE_SIMPLE_TYPE(unlock, lock);
    CASE_SIMPLE_TYPE(rlock, lock);
    CASE_SIMPLE_TYPE(runlock, lock);
    
    CASE_SIMPLE_TYPE(signal, sw);
    CASE_SIMPLE_TYPE(wait, sw);
    
    CASE_SIMPLE_TYPE(tcreate, tcj);
    CASE_SIMPLE_TYPE(tjoin, tcj);
    
    CASE_SIMPLE_TYPE(fentry, fee);
    CASE_SIMPLE_TYPE(fexit, fee);
    
    CASE_SIMPLE_TYPE(fcpre, fc);
    CASE_SIMPLE_TYPE(fcpost, fc);

#undef CASE_SIMPLE_TYPE
    default:
        pr_err("Unknown message type: %d.", msg->type);
        return execution_message_type_invalid;
    }
    return msg->type;
}
EXPORT_SYMBOL(handler_stub_get_msg_type);

static int handler_stub_op_start(
    struct execution_event_collector* collector)
{
    if(local_current_collector != NULL)
    {
        pr_err("Attempt to use handler for more than one event collectors.");
        return -EBUSY;
    }
    local_current_collector = collector;
    //pr_info("Handler has been started.");
    return 0;
}

static int handler_stub_op_stop(
    struct execution_event_collector* collector)
{
    if(local_current_collector != collector)
    {
        pr_err("Attempt to stop handler for collector for which it is not used.");
        return -EINVAL;
    }
    
    local_current_collector = NULL;
    //pr_info("Handler has stopped.");
    return 0;
}

static struct execution_event_handler handler_stub =
{
    .owner = THIS_MODULE,
    .start = handler_stub_op_start,
    .stop = handler_stub_op_stop
};

int __init handler_stub_init(void)
{
    int result = execution_event_set_handler(&handler_stub);
    if(result) return result;
    
    return 0;
}

void __exit handler_stub_exit(void)
{
    execution_event_unset_handler(&handler_stub);
}

module_init(handler_stub_init);
module_exit(handler_stub_exit);