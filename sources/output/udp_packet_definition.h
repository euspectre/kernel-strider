/*
 * Contains definition of different eccences, used for
 * transmitting execution trace via UDP.
 *
 * This file may be read from kernel and user spaces, and from different
 * machines.
 *
 * So, its definitions should be arhitecture-independed.
 *
 * Format of the message with trace events packet is follows:
 *
 * - kedr_message_header (type = ctf)
 * - packet
 *
 * Format of the message with CTF metadata:
 * - kedr_message_header (type = meta_ctf)
 * - metadata_packet(contains CTF metadata string representation)
 *
 * Format of the message with mark:
 * - kedr_message_header (mark_range_start <= type <= mark_range_end)
 *
 * Typical sequence of messages:
 *
 * 1. mark_session_start
 * 2. mark_trace_start(if no trace events has been read before)
 * 3. meta_ctf (1 or more)
 * 4. mark_meta_ctf_end
 * 5. ctf (1 or more)
 * 6. mark_trace_end (if last message from trace has been transmitted)
 * 7. mark_session_end
 */

#ifndef UDP_PACKET_DEFINITIONS_H
#define UDP_PACKET_DEFINITIONS_H

#ifdef __KERNEL__
#include <linux/types.h>
#else
#include <stdint.h>
#endif

/* Server will be running on this port by default */
#ifndef TRACE_SERVER_PORT
#define TRACE_SERVER_PORT 5556
#endif
/*
 * Maximum length of message sent from the server to the client
 */
#define TRACE_SERVER_MSG_LEN_MAX 1500

/* UDP packet type */
enum kedr_message_type
{
    /* Shouldn't be used */
    kedr_message_type_invalid = 0,
    /* Message contains CTF packet with trace events */
    kedr_message_type_ctf,
    /* Message contains packet with meta information about CTF trace */
    kedr_message_type_meta_ctf,

    /* Start of the range with marks */
    kedr_message_type_mark_range_start,
    /* Start of the session with reciever */
    kedr_message_type_mark_session_start = kedr_message_type_mark_range_start,
    /* End of the session with reciever */
    kedr_message_type_mark_session_end,
    /* Stop transmit CTF meta data */
    kedr_message_type_mark_meta_ctf_end,
    /* No message from the trace has been transmitted at that moment */
    kedr_message_type_mark_trace_start,
    /*
     * Last message from the trace has been transmitted,
     * futher messages are not expected.
     */
    kedr_message_type_mark_trace_end,
    /* End of the range with marks */
    kedr_message_type_mark_range_end = kedr_message_type_mark_trace_end,
};

#define KEDR_MESSAGE_HEADER_MAGIC 0xBBB5B4C2

/* All integers in header should be in network byte order. */
struct kedr_message_header
{
    uint32_t magic;
    uint32_t seq;
    uint8_t type;
    /* Data follows without padding */
    unsigned char data[0];
};

#define kedr_message_header_size (offsetof(struct kedr_message_header, data))

/*******************Commands to the trace sender **********************/
enum kedr_message_command_type
{
    kedr_message_command_type_start = 1,
    kedr_message_command_type_stop,
};

/* struct kedr_message_header is used for server commands */

#endif /* UDP_PACKET_DEFINITIONS_H */
