#ifndef TRACE_RECEIVER_H
#define TRACE_RECEIVER_H

#include "udp_packet_definition.h"

/* 
 * For control packets use same header format as for normal ones but with
 * different magic field.
 */

#define KEDR_MESSAGE_HEADER_CONTROL_MAGIC 0xBBB5B4B3

/* Types of control messages to the receiver */
enum kedr_message_control_type
{
/* Keep-connection-alive packet */
	kedr_message_control_type_keep_alive = 0,
/* Actions */
	/* Exit from the program */
	kedr_message_control_type_terminate,
	/* Send 'start' to the trace sender with given address. */
	kedr_message_control_type_init_connection,
	/* Send 'stop' to the trace sender with given address. */
	kedr_message_control_type_break_connection,
/* Waiters */
	/* Reply when any connection is set */
	kedr_message_control_type_wait_init_connection,
	/* Reply when current connection is broken */
	kedr_message_control_type_wait_break_connection,
	/* Reply just before terminating exit*/
	kedr_message_control_type_wait_terminate,
	/* Reply when trace begins */
	kedr_message_control_type_wait_trace_begin,
	/* Reply when trace ends */
	kedr_message_control_type_wait_trace_end,
};

/* Types of messages contain information about trace receiver. */
enum kedr_message_info_type
{
	kedr_message_info_type_start_connection = 0,
	kedr_message_info_type_stop_connection,
	kedr_message_info_type_start_trace,
	kedr_message_info_type_stop_trace,
	kedr_message_info_type_start,
	kedr_message_info_type_stop
};

#endif /* TRACE_RECEIVER_H */