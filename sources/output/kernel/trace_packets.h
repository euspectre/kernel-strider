/*
 * Build packets for transmit events and other info for one trace.
 */

#ifndef TRACE_PACKETS_H
#define TRACE_PACKETS_H

#include "kedr/event_collector/event_collector.h"
#include "linux/types.h" /* size_t, ssize_t */
#include "net_message.h" /* msg_builder */

struct ctf_trace_operations;

/* Base struct for the CTF trace */
struct ctf_trace
{
    uuid_t uuid;
    const struct ctf_trace_operations* t_ops;
};

struct ctf_trace_operations
{
    /* Append packet header into builder and fill it */
    ssize_t (*append_packet_header) (struct ctf_trace* trace,
        struct msg_builder* builder, stream_id_t stream_id);
    /* Return allocated (multy-)string with trace metadata */
    char* (*get_metadata)(struct ctf_trace* trace, size_t* length_p);
};

struct ctf_stream_operations;

/* Base struct for the CTF stream*/
struct ctf_stream
{
    stream_id_t stream_id;
    struct ctf_trace* trace;
    /* Should be manually incremented after each packet has been sent */
    __u32 packet_count;
    const struct ctf_stream_operations *s_ops;
};

struct ctf_stream_operations
{
    /* 
     * Append stream packet header and stream packet context info builder
     * and fill all known fields.
     * 
     * Also may set data for others operations.
     */
    ssize_t (*append_stream_header)(struct ctf_stream* stream,
        struct msg_builder* builder, void** data_p);
    
    /*
     * Append event to the stream packet.
     * 
     * Return number of bytes added to the message. Also set 'ts_p'
     * to the timestamp of event added.
     * 
     * If stream is currently empty, return -EAGAIN.
     */
    
    ssize_t (*append_event)(struct ctf_stream* stream,
        struct msg_builder* builder, void* data, __u64* ts_p);

    /*
     * Set all fields in the message that wasn't set before, thus
     * preparing message to be send.
     */
    void (*prepare_packet)(struct ctf_stream* stream,
        struct msg_builder* builder, void* data);
    
    /* If not NULL, free data returned in append_stream_header() */
    void (*free_data)(void* data);
};

/********************* Stream with CTF metadata ***********************/

/* Stream for CTF metadata */
struct ctf_stream_meta
{
    struct ctf_trace* trace;

    char* metadata;
    size_t metadata_length;
    /* Index of first unread symbol */
    size_t current_pos;
};

/* 
 * Initialize stream with metadata of given trace.
 */
int ctf_stream_meta_init(struct ctf_stream_meta* stream_meta,
    struct ctf_trace* trace);

void ctf_stream_meta_destroy(struct ctf_stream_meta* stream_meta);

/* 
 * Form next packet for metadata.
 * 
 * Return number of bytes written.
 * Return 0 if whole metadata has been sended.
 */
ssize_t ctf_stream_meta_next_packet(struct ctf_stream_meta* stream_meta,
    struct msg_builder* builder);


/**************************** CTF trace *******************************/

/* 
 * Initialize base struct of CTF trace.
 * 
 * UUID is generated automatically.
 */
int ctf_trace_init(struct ctf_trace* trace,
    const struct ctf_trace_operations* ops);

void ctf_trace_destroy(struct ctf_trace* trace);

/* Append packet header into builder and fill it */
ssize_t ctf_trace_append_packet_header(struct ctf_trace* trace,
    struct msg_builder* builder, stream_id_t stream_id);


/**************************** CTF stream *******************************/

/* Initialize base struct of CTF stream. */
int ctf_stream_init(struct ctf_stream* stream,
    struct ctf_trace* trace, stream_id_t stream_id,
    const struct ctf_stream_operations* ops);

void ctf_stream_destroy(struct ctf_stream* stream);

/* 
 * Form next packet with events for the stream.
 * 
 * Return number of bytes written.
 * 
 * If no events in the stream, return -EAGAIN.
 * 
 * NOTE: currently there is no function for append events to the
 * existing packet.
 */
ssize_t ctf_stream_next_packet(struct ctf_stream* stream,
    struct msg_builder* builder);

/********************Conrete CTF trace *****************************/
struct kedr_ctf_trace
{
    struct ctf_trace base;
    struct execution_event_collector* collector;
    //TODO: other fields?
};

/* Initialize trace for given event collector */
int kedr_ctf_trace_init(struct kedr_ctf_trace* kedr_trace,
    struct execution_event_collector* collector);

void kedr_ctf_trace_destroy(struct kedr_ctf_trace* kedr_trace);

/********************Conrete CTF stream *****************************/

struct kedr_ctf_stream
{
    struct ctf_stream base;
    // TODO: other fields
};

int kedr_ctf_stream_init(struct kedr_ctf_stream* kedr_stream,
    struct kedr_ctf_trace* kedr_trace);

void kedr_ctf_stream_destroy(struct kedr_ctf_stream* kedr_stream);

/**********************************************************************/

#endif /* TRACE_PACKETS_H*/