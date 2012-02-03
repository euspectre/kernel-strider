/*
 * Types of event collector and messages it collects.
 * Also API for define handler for events collected.
 */

#ifndef EVENT_HANDLER_H
#define EVENT_HANDLER_H

#include "kedr/event_collector/event_collector.h" /* only for types */
#include "kedr/object_types.h"

struct trace_buffer;

struct execution_event_collector
{
    /* Which module events are belong to */
    struct module* m;
    /* This field may be used by collector handler */
    void* private_data;
    
    /* Other fields are private to implementation */

    /* 
     * Trace buffer which contain all collected events.
     * Format is described in "event_collector/execution_messages.h".
     * 
     * TODO: lock events may be sended into another trace buffer.
     */
    struct trace_buffer* common_buffer;

    int is_handled;
};

/* Handler of execution events */
struct execution_event_handler
{
    /* Module contained code of callbacks */
    struct module* owner;
    /* 
     * Callback which will be called after event collector is created.
     * 
     * If it return negative value, event collector is processed
     * as non-handled.
     */
    int (*start)(struct execution_event_collector* collector);
    /* 
     * Callback which will be called before event collector is destroyed.
     * 
     * Negative value may be returned in case of serious error.
     */
    int (*stop)(struct execution_event_collector* collector);
};

/*
 * Set handler for execution events.
 * 
 * Every time new execution event collector is created, 'start'
 * callback of the handler is called with this collector as argument.
 * 
 * If this callback return 0, then 'stop' callback will be called when
 * collector is destroyed.
 * 
 * NOTE: In the current implementation of instrumentor, at most one
 * event collector may exist at any moment of time.
 */
int execution_event_set_handler(
    struct execution_event_handler* handler);
int execution_event_unset_handler(
    struct execution_event_handler* handler);


/* 
 * Read the oldest message from the event collector.
 * 
 * For message read call 'process_data':
 * 'msg' is set to the pointer to the message data.
 * 'size' is set to the size of the message,
 * 'ts' is set to the timestamp of the message,
 * 'user_data' is set to the 'user_data' parameter of the function.
 * 
 * Return value, which is returned by 'process_message'.
 *
 * If 'process_message' set 'consume' parameter to not 0,
 * message is treated consumed, and next reading return next message from buffer.
 * Otherwise, next reading return the same message.
 * 
 * If buffer is empty return 0.
 * 
 * If error occurs, return negative error code.
 * 
 * Shouldn't be called in atomic context.
 */
int execution_event_collector_read_message(
    struct execution_event_collector* collector,
    int (*process_message)(const void* msg, size_t size, int cpu,
        u64 ts, bool *consume, void* user_data),
    void* user_data);
/******************* Message collected ********************************/

/* Types of messages */
enum execution_message_type
{
    execution_message_type_invalid = 0,
    /* 
     * Message contains array of information about consequent
     * memory accesses.
     */
    execution_message_type_ma,
    /* Message contains information about one locked memory access */
    execution_message_type_lma,
    /* 
     * Message contains information about one memory barrier
     * (read, write, full).
     */
    execution_message_type_mrb,
    execution_message_type_mwb,
    execution_message_type_mfb,
    /* 
     * Message contains information about one memory management operation
     * (alloc/free).
     */
    execution_message_type_alloc,
    execution_message_type_free,
    /* 
     * Message contains information about one lock operation
     * (lock/unlock or its read variants).
     */
    execution_message_type_lock,
    execution_message_type_unlock,
    
    execution_message_type_rlock,
    execution_message_type_runlock,
    /* Message contains information about one signal/wait operation */
    execution_message_type_signal,
    execution_message_type_wait,
    /* Message contains information about thread create/join operation */
    execution_message_type_tcreate,
    execution_message_type_tjoin,
    /* Message contains information about function entry/exit */
    execution_message_type_fentry,
    execution_message_type_fexit,
    /* Message contain information about function call(pre and post)*/
    execution_message_type_fcpre,
    execution_message_type_fcpost,
};

/* Beginning of every message */
struct execution_message_base
{
    tid_t tid;
    char type;
};

/* Messages of concrete types */
struct execution_message_ma_subevent
{
    addr_t pc;
    addr_t addr;
    size_t size;
    unsigned char access_type;
};

struct execution_message_ma
{
    struct execution_message_base base; /* .type = ma */
    unsigned char n_subevents;
    struct execution_message_ma_subevent subevents[0];
};

struct execution_message_lma
{
    struct execution_message_base base; /* .type = lma */
    addr_t pc;
    addr_t addr;
    size_t size;
};


struct execution_message_mb
{
    struct execution_message_base base; /* .type = m*b */
    addr_t pc;
};

struct execution_message_alloc
{
    struct execution_message_base base; /* .type = alloc */
    addr_t pc;
    size_t size;
    addr_t pointer;
};

struct execution_message_free
{
    struct execution_message_base base; /* .type = free */
    addr_t pc;
    addr_t pointer;
};

struct execution_message_lock
{
    struct execution_message_base base; /* .type = (un)lock */
    unsigned char type;
    addr_t pc;
    addr_t obj;
};

struct execution_message_sw
{
    struct execution_message_base base; /* .type = signal/wait */
    addr_t pc;
    addr_t obj;
    unsigned char type;
};

struct execution_message_tcj
{
    struct execution_message_base base; /* .type = tcreate/tjoin */
    addr_t pc;
    tid_t child_tid;
};

struct execution_message_fee
{
    struct execution_message_base base; /* .type = fentry/fexit */
    addr_t func;
};

struct execution_message_fc
{
    struct execution_message_base base; /* .type = fcpre/fcpost */
    addr_t pc;
    addr_t func;
};

#endif /* EVENT_HANDLER_H */