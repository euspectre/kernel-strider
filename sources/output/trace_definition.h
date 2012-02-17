/*
 * Contains definition of different eccences, used for 
 * transmitting execution trace via UDP and storing trace in a local file.
 * 
 * This file may be read from kernel and user spaces, and from different
 * machines.
 * 
 * So, toplevel definitions should be arhitecture-independed.
 * Other definitions may be interpret differently on different machines.
 * Trace producer should set identificator of machine type, and trace
 * consumer(e.g., reader), should read this type and interpret trace
 * correspondingly.
 * 
 * But: all members in structures has network byte order.
 * 
 * Definitions of trace packets are made in a way similar to
 * Common Trace Format(http://www.efficios.com/ctf).
 * 
 * Format of the message with trace events packet is follows:
 * 
 * - kedr_message_header (type = ctf)
 * - event_packet_header
 * - event_packet_context
 * - array of events:
 *   {
 *     - event_header
 *     - stream_event_context
 *        -- next fields depend on event type --
 *     - event_payload
 *   }
 * 
 * Every CTF structure has size(presize, using CTF_STRUCT_SIZE()) and
 * alignment.
 * There is no padding aside from one needed for process alignment.
 * But padding in total packet is permissible(for extract its presize
 * size, use 'event_packet_context.content_size').
 * 
 * Format of the message with CTF metadata:
 * - kedr_message_header (type = meta_ctf)
 * - metadata_packet_header
 * - CTF metadata(string representation)
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

#ifndef TRACE_DEFINITIONS_H
#define TRACE_DEFINITIONS_H

#include <linux/types.h>

#ifdef __KERNEL__
/*
 * Needed definitions in the kernel space.
 */

/* define htonl and others */
#include <linux/in.h>

#else /* __KERNEL__ */

/*
 * Needed definitions in the user space.
 */

/* define ntohs and others */
#include <arpa/inet.h>

#endif /* __KERNEL__ */


/* Server will be running on this port by default */
#ifndef TRACE_SERVER_PORT
#define TRACE_SERVER_PORT 5556
#endif
/* 
 * Maximum length of message sent from the server to the client
 */
#define TRACE_SERVER_MSG_LEN_MAX 1500

/*
 * Special processing of struct used by Common Trace Format:
 * size of this structs do not take into account alignment of whole
 * structure.
 * This is contradict to 'C' language structures, which sizes should be
 * equal to distance between elements in array.
 */

/* This macro should be used at the end of any CTF-structure */
#define CTF_STRUCT_END char ctf_struct_end[0];
/* 
 * This macro should be used for calculate size of CTF-structure,
 * instead of sizeof(TYPE).
 */
#define CTF_STRUCT_SIZE(TYPE) offsetof(TYPE, ctf_struct_end)
/* 
 * This macro should be used for calculate size of array of
 * CTF-structures, instead of sizeof(TYPE) * n_elems.
 */
#define CTF_ARRAY_SIZE(TYPE, n_elems) (sizeof(TYPE) * ((n_elems) - 1) + CTF_STRUCT_SIZE(TYPE))

#include <kedr/object_types.h> /* types of locks, memory accesses...*/

typedef unsigned char uuid_t[16];

/* Top-level message header defined as CTF-structure */
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
    kedr_message_type_mark_range_end
};

struct kedr_message_header
{
    __be32 seq;
    __u8 type;
    CTF_STRUCT_END
};

/********************CTF packet with trace events *********************/

#define CTF_MAGIC 0xC1FC1FC1

typedef uint16_t stream_id_t;
typedef __be16 __be_stream_id;

struct execution_event_packet_header
{
    __be32 magic;
    uuid_t uuid;
    __be_stream_id stream_id;
    CTF_STRUCT_END
};

/* 
 * Linux kernel before 2.6.36 doesnt't contain 64bit type which suitable
 * for use in network message(see notes above).
 * So we define our one for timestamps.
 */
typedef struct {__be32 high, low;} __attribute__((aligned(32))) timestamp_nt;

static inline void timestamp_nt_set(timestamp_nt *ts_nt, uint64_t ts)
{
    uint32_t ts_low = (uint32_t)(ts & 0xffffffff);
    uint32_t ts_high = (uint32_t)(ts >> 32);
    ts_nt->low = htonl(ts_low);
    ts_nt->high = htonl(ts_high);
}

static inline uint64_t timestamp_nt_get(timestamp_nt *ts_nt)
{
    return ntohl(ts_nt->low) + (((uint64_t)ntohl(ts_nt->high)) << 32);
}

struct execution_event_packet_context
{
    /* Timestamp of the first event in the packet */
    timestamp_nt timestamp_begin;
    /* Timestamp of the last event in the packet */
    timestamp_nt timestamp_end;
    /* Packet count inside stream */
    __be32 stream_packet_count;
    __be16 content_size;/* Size of packet in bits*/
    __be16 packet_size;/* Size of packet in bits, including padding */
    CTF_STRUCT_END
};

struct execution_event_header
{
    /* Event identificator inside trace stream */
    __be32 id;
    CTF_STRUCT_END
};


enum execution_event_type
{
    execution_event_type_invalid = 0,
    /* 
     * Event contains array of information about consequent
     * memory accesses.
     */
    execution_event_type_ma,
    /* Event contains information about one locked memory access */
    execution_event_type_lma,
    /* 
     * Event contains information about one memory barrier
     * (read, write, full).
     */
    execution_event_type_mrb,
    execution_event_type_mwb,
    execution_event_type_mfb,
    /* 
     * Event contains information about one memory management operation
     * (alloc/free).
     */
    execution_event_type_alloc,
    execution_event_type_free,
    /* 
     * Event contains information about one lock operation
     * (lock/unlock or its read variants).
     */
    execution_event_type_lock,
    execution_event_type_unlock,
    
    execution_event_type_rlock,
    execution_event_type_runlock,
    /* Event contains information about one signal/wait operation */
    execution_event_type_signal,
    execution_event_type_wait,
    /* Event contains information about thread create/join operation */
    execution_event_type_tcreate,
    execution_event_type_tjoin,
    /* Event contains information about function entry/exit */
    execution_event_type_fentry,
    execution_event_type_fexit,
};

/* 
 * NOTE: for event context and payload structures layout are
 * architecture-depended.
 */

typedef unsigned long __bitwise __be_addr;
typedef unsigned long __bitwise __be_size;
/* Type for thread identificator */
typedef unsigned long __bitwise __be_tid;

#ifdef CONFIG_X86_64
static __u64 hton_addr(__u64 val)
{
    union
    {
        __u64 result;
        __u32 words[2];
    };
    words[0] = htonl(val >> 32);
    words[1] = htonl(val & 0xffffffff);
    
    return result;
}
#else /* CONFIG_X86_32 */
#define hton_addr(val) htonl(val)
#endif /* CONFIG_X86_64 */

#define ntoh_addr(val) hton_addr(val)

#define hton_size(val) hton_addr(val)
#define ntoh_size(val) ntoh_addr(val)
#define hton_tid(val) hton_addr(val)
#define ntoh_tid(val) ntoh_addr(val)

struct execution_stream_event_context
{
    /* Timestamp of the event */
    timestamp_nt timestamp;
    /* Thread id of the event */
    __be_tid tid;
    /* Type of the event */
    __u8 type;
    CTF_STRUCT_END
};

/*
 * Additional stream event context for packed events, contained
 * array of subevents.
 */
struct execution_stream_event_context_ma_add
{
    /* Number of sub-events */
    __u8 n_subevents;
    CTF_STRUCT_END
};

/*
 * Additional stream event context for locked events.
 */
struct execution_stream_event_context_lock_add
{
    /* Type of lock */
    __u8 type;
    CTF_STRUCT_END
};


/**************** Payloads of events of different types****************/
/*
 * NOTE: All payloads has same alignment: 32 on x86 and 64 on x86_64.
 */

/* One element of packed event */
struct execution_event_ma_payload_elem
{
    /* Program counter of the instruction*/
    __be_addr pc;
    /* Access address */
    __be_addr addr;
    /* Access size */
    __be_size size;
    /* Type of access */
    __u8 access_type;
    CTF_STRUCT_END
};

struct execution_event_ma_payload
{
    struct execution_event_ma_payload_elem elems[0];
    CTF_STRUCT_END
};

struct execution_event_lma_payload
{
    /* Program counter of the instruction*/
    __be_addr pc;
    /* Access address */
    __be_addr addr;
    /* Access size */
    __be_size size;
    CTF_STRUCT_END
};

struct execution_event_mb_payload
{
    /* Program counter of the instruction*/
    __be_addr pc;
    CTF_STRUCT_END
};

struct execution_event_alloc_payload
{
    /* Program counter of the instruction(normally, call <*alloc>)*/
    __be_addr pc;
    /* Access size */
    __be_size size;
    /* Pointer returned from operation */
    __be_addr pointer;
    CTF_STRUCT_END
};

struct execution_event_free_payload
{
    /* Program counter of the instruction(normally, call <*free>)*/
    __be_addr pc;
    /* Pointer for free */
    __be_addr pointer;
    CTF_STRUCT_END
};

/* Same structure for lock and unlock events */
struct execution_event_lock_payload
{
    /* Program counter of the instruction(normally, call <*(un)lock*>)*/
    __be_addr pc;
    /* Address of lock object */
    __be_addr object;
    CTF_STRUCT_END
};


/* Same structure for signal and wait events */
struct execution_event_sw_payload
{
    /* Program counter of the instruction(normally, call <*>) */
    __be_addr pc;
    /* Address of wait object */
    __be_addr object;
    CTF_STRUCT_END
};

/* Same structure for thread create and join events */
struct execution_event_tcj_payload
{
    /* Program counter of the instruction(normally, call <*>) */
    __be_addr pc;
    /* Created or joined thread */
    __be_tid child_tid;
    CTF_STRUCT_END
};

/* Same structure for function entry and exit */
struct execution_event_fee_payload
{
    /* Function address */
    __be_addr func;
    CTF_STRUCT_END
};

/************* CTF Meta-data in packet-base form **********************/

/* Strings represented machine types */
#define MACHINE_TYPE_x86 "x86"
#define MACHINE_TYPE_x86_64 "x86_64"
// TODO: may be other machine types, which differ, e.g, in instuctions set

/* 
 * Define only big-endian meta-magic, because only it
 * is used in our trace.
 */
#define CTF_META_MAGIC 0x75D11D57

/* From CTF specification "as is" */
struct metadata_packet_header {
  uint32_t magic;               /* 0x75D11D57 */
  uint8_t  uuid[16];            /* Unique Universal Identifier */
  uint32_t checksum;            /* 0 if unused */
  uint32_t content_size;        /* in bits */
  uint32_t packet_size;         /* in bits */
  uint8_t  compression_scheme;  /* 0 if unused */
  uint8_t  encryption_scheme;   /* 0 if unused */
  uint8_t  checksum_scheme;     /* 0 if unused */
  uint8_t  major;               /* CTF spec version major number */
  uint8_t  minor;               /* CTF spec version minor number */
  CTF_STRUCT_END
};

/*******************Commands to the trace sender **********************/
enum kedr_trace_sender_command_type
{
    kedr_trace_sender_command_type_start = 1,
    kedr_trace_sender_command_type_stop,
};

/* Simple format of the command message - only type */
struct kedr_trace_sender_command
{
    char type;
};

#endif /* TRACE_DEFINITIONS_H */