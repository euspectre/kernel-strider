/*
 * Contains C-definition of CTF data in the trace.
 *
 * File should always be in correspondence with ../ctf_meta_template,
 * which contains same definitions in CTF metadata format.
 *
 * Definitions of trace packets are made in a way similar to
 * Common Trace Format(http://www.efficios.com/ctf), but differ from it.
 * See description of ctf_reader utility.
 *
 * Every CTF structure has size(presize, using CTF_STRUCT_SIZE()) and
 * alignment.
 * There is no padding aside from one needed for process alignment.
 */

#ifndef TRACE_DEFINITIONS_H
#define TRACE_DEFINITIONS_H

#include <linux/types.h>

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
#include <kedr/output/event_collector.h> /* tid_t, addr_t*/

#include <kedr/utils/uuid.h> /* UUID */

/*********************** Trace Packet Header ***************************/
#define CTF_MAGIC 0xC1FC1FC1

enum execution_stream_type
{
    execution_stream_type_normal = 0,
    execution_stream_type_critical
};

struct execution_event_packet_header
{
    uint32_t magic;
    uuid_t uuid;
    unsigned char stream_type;
    unsigned char cpu;
    CTF_STRUCT_END
};

/*********************** Stream Packet Context ************************/

struct execution_event_packet_context
{
    /* Timestamp of the first event in the packet */
    uint64_t timestamp_begin;
    /* Timestamp of the last event in the packet */
    uint64_t timestamp_end;
    /* Packet count inside stream */
    uint32_t stream_packet_count;
    uint16_t content_size;/* Size of packet in bits*/
    uint16_t packet_size;/* Size of packet in bits, including padding */
    CTF_STRUCT_END
};

/********************* Steram Event Header ****************************/
struct execution_event_header
{
    /* Type of the event */
    uint8_t type;
    CTF_STRUCT_END
};

/* Possible event types */
enum execution_event_type
{
    /*
     * Event contains array of information about consequent
     * memory accesses.
     */
    execution_event_type_ma = 0,
    /* Event contains information about one locked memory access */
    execution_event_type_lma_update,
    execution_event_type_lma_read,
    execution_event_type_lma_write,
    /* Event contains information about one I/O with memory access */
    execution_event_type_ioma,
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
    /* Event contains information about function call pre- or post- operation */
    execution_event_type_fcpre,
    execution_event_type_fcpost,
};

/*************************** Stream Event Context ****************************/

struct execution_stream_event_context
{
    /* Timestamp of the event */
    uint64_t timestamp;
    /* Thread id of the event */
    tid_t tid;
    /* Event counter of that event. Used for ordering.*/
    int32_t counter;
    
    CTF_STRUCT_END
};

/************************** Event Context ************************************/

/*
 * Event context for packed events, contained
 * array of subevents.
 */
struct execution_event_context_ma
{
    /* Number of sub-events */
    uint8_t n_subevents;
    CTF_STRUCT_END
};

/**************** Event fields **********************************************/

/* One element of packed event */
struct execution_event_fields_ma_elem
{
    /* Program counter of the instruction*/
    addr_t pc;
    /* Access address */
    addr_t addr;
    /* Access size */
    size_t size;
    /* Type of access(types are defined in <kedr/objects.h>) */
    uint8_t access_type;
    CTF_STRUCT_END
};

struct execution_event_fields_ma
{
    struct execution_event_fields_ma_elem elems[0];
    CTF_STRUCT_END
};

struct execution_event_fields_lma
{
    /* Program counter of the instruction*/
    addr_t pc;
    /* Access address */
    addr_t addr;
    /* Access size */
    size_t size;
    CTF_STRUCT_END
};

struct execution_event_fields_ioma
{
    /* Program counter of the instruction*/
    addr_t pc;
    /* Access address */
    addr_t addr;
    /* Access size */
    size_t size;
    /* Type of access(types are defined in <kedr/objects.h>) */
    uint8_t access_type;
    CTF_STRUCT_END
};


struct execution_event_fields_mb
{
    /* Program counter of the instruction*/
    addr_t pc;
    CTF_STRUCT_END
};

struct execution_event_fields_alloc
{
    /* Program counter of the instruction(normally, call <*alloc>)*/
    addr_t pc;
    /* Access size */
    size_t size;
    /* Pointer returned from operation */
    addr_t pointer;
    CTF_STRUCT_END
};

struct execution_event_fields_free
{
    /* Program counter of the instruction(normally, call <*free>)*/
    addr_t pc;
    /* Pointer for free */
    addr_t pointer;
    CTF_STRUCT_END
};

/* Same structure for lock and unlock events */
struct execution_event_fields_lock
{
    /* Program counter of the instruction(normally, call <*(un)lock*>)*/
    addr_t pc;
    /* Address of lock object */
    addr_t object;
    /* Type of the lock */
    uint8_t type;
    CTF_STRUCT_END
};


/* Same structure for signal and wait events */
struct execution_event_fields_sw
{
    /* Program counter of the instruction(normally, call <*>) */
    addr_t pc;
    /* Address of wait object */
    addr_t object;
    /* Type of object */
    uint8_t type;
    CTF_STRUCT_END
};

/* Same structure for thread create and join events */
struct execution_event_fields_tcj
{
    /* Program counter of the instruction(normally, call <*>) */
    addr_t pc;
    /* Created or joined thread */
    tid_t child_tid;
    CTF_STRUCT_END
};

/* Same structure for function entry and exit */
struct execution_event_fields_fee
{
    /* Function address */
    addr_t func;
    CTF_STRUCT_END
};

/* Same structure for function call pre- and post- operation */
struct execution_event_fields_fc
{
    /* Program counter of the instruction(normally, call <*>) */
    addr_t pc;
    /* Function address */
    addr_t func;
    CTF_STRUCT_END
};


/************* CTF Meta-data in packet-base form **********************/

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

#endif /* TRACE_DEFINITIONS_H */

