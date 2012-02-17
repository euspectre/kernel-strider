/*
 * Simple handler, useful for check event collector.
 */

#ifndef HANDLER_STUB_API_H
#define HANDLER_STUB_API_H

#include "kedr/event_collector/event_handler.h"

/* Return non-zero if handler is used now(and there is trace buffer) */
int handler_stub_is_used(void);

/* 
 * Extract and consume last message from the trace buffer of event
 * collector.
 * 
 * Return size of this message and set 'msg' to allocated buffer with
 * content of this message. 'msg' should be freed when no longer needed.
 * 
 * If trace buffer is empty, return -EAGAIN.
 * 
 * NOTE: should be called only when handler_stub_is_used() return non-zero.
 */
int handler_stub_get_message(struct execution_message_base** msg);

/*
 * Return type of given message.
 * 
 * Also check correctness of size.
 * 
 * On error return execution_message_type_invalid.
 */
enum execution_message_type
handler_stub_get_msg_type(struct execution_message_base* msg, int size);

#endif /* HANDLER_STUB_API_H */