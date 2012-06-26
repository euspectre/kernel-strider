/*
 * Object for sending trace via network.
 */

#ifndef TRACE_SENDER_H
#define TRACE_SENDER_H

#include <kedr/output/event_collector.h>

struct trace_sender;

/*
 * Create trace sender object.
 * 
 * 'transmition_interval' is a time interval (in ms) between call function,
 * which really send messages.
 * 
 * 'transmition_interval_limit' is a time interval (in ms) between
 * call function, which really send messages, in case when trace is empty.
 * 
 * 'transmition_size_limit' is a maximum size (in bytes) of message
 * to send.
 * 
 * 'transmition_rate_limit' is a maximum rate (in kbytes/sec) of sending
 * messages.
 */
struct trace_sender* trace_sender_create(
    int transmition_interval,
    int transmition_interval_empty,
    int transmition_size_limit,
    int transmition_rate_limit);

/*
 * Destroy trace sender object.
 * 
 * It is an error to destroy trace sender while it send messages.
 */
void trace_sender_destroy(struct trace_sender* sender);

/* 
 * Say to sender to start sending session with given client.
 * 
 * May be executed in atomic context.
 * 
 * Note, that address and port of client are given in network byte order.
 * 
 * Return 0 on success and negative error code on fail.
 * 
 * Return -EBUSY if sender already send trace.
 */
int trace_sender_start(struct trace_sender* sender,
	__u32 client_addr,
	__u16 client_port);

/*
 * Say to sender to stop sending session with current client.
 * 
 * May be executed in atomic context.
 * 
 * NOTE: After this command several messages may be sent to the client.
 * 
 * If sender doesn't send trace or already perform stopping actions, 
 * do nothing.
 */
void trace_sender_stop(struct trace_sender* sender);


/*
 * Wait until sender stops to send any message.
 * 
 * If no 'start' command was issued from this function call,
 * trace sender may be safely destroyed.
 */
int trace_sender_wait_stop(struct trace_sender* sender);

/* 
 * Start collect messages from given module.
 * 
 * Return collector for that module.
 */
struct execution_event_collector*
    trace_sender_collect_messages(struct trace_sender* sender,
    struct module* m,
    size_t buffer_normal_size, size_t buffer_critical_size);

/*
 * Stop collect messages from given module.
 * 
 * NOTE: this function waits until all messages from this module
 * will be sent (only in case when sender has session set).
 */
int trace_sender_stop_collect_messages(struct trace_sender* sender,
    struct module* m);

/* 
 * Return information about current session.
 * 
 * If session is set, return 0 and set 'client_addr' and 'client_port'
 * to the address and port of current session client.
 * 
 * If session is not set, return -ENODEV.
 */
int trace_sender_get_session_info(struct trace_sender* sender,
    __u32* client_addr, __u16* client_port);

#endif /* TRACE_SENDER_H */
