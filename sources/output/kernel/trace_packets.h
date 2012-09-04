/*
 * Build packets for transmit events and other info for one trace.
 */

#ifndef TRACE_PACKETS_H
#define TRACE_PACKETS_H

#include <kedr/output/event_collector.h>
#include <linux/types.h> /* size_t, ssize_t */
#include "net_message.h" /* msg_builder */

#include <kedr/utils/template_parser.h> /* Metadata as template */

/* Contain all information needed to send events as CTF events */
struct kedr_trace
{
    unsigned char uuid[16];
    
    struct execution_event_collector event_collector;
    /* 
     * Buffer from which currently send events.
     */
    struct event_collector_buffer* current_buffer;
    /*
     * CPU number from whith currently send events.
     */
    int current_cpu;
    /* 
     * Number of packets for extract before recalculating current
     * buffer and cpu.
     * 
     * These values will be also recalculated in case when current
     * sub-buffer is empty.
     */
    int current_packets_rest;
};

/*
 * Initialize base structure of CTF trace object.
 *
 * UUID is generated automatically.
 */
int kedr_trace_init(struct kedr_trace* trace,
    size_t buffer_normal_size, size_t buffer_critical_size);

void kedr_trace_destroy(struct kedr_trace* trace);


/*
 * Form next packet with events from trace.
 *
 * Return number of bytes written.
 *
 * If no events in the trace, return -EAGAIN.
 */
ssize_t kedr_trace_next_packet(struct kedr_trace* trace,
    struct msg_builder* builder);


/********************* Stream with CTF metadata *******************************/

/* Stream for CTF metadata */
struct kedr_stream_meta
{
    struct kedr_trace* trace;

    struct template_parser meta_template_parser;
    
    const char* current_chunk;
    int current_chunk_len;
};

/*
 * Initialize stream with metadata of given trace.
 */
int kedr_stream_meta_init(struct kedr_stream_meta* stream_meta,
    struct kedr_trace* trace);

void kedr_stream_meta_destroy(struct kedr_stream_meta* stream_meta);

/*
 * Form next packet for metadata.
 *
 * Return number of bytes written.
 * Return 0 if whole metadata has been sended.
 */
ssize_t kedr_stream_meta_next_packet(struct kedr_stream_meta* stream_meta,
    struct msg_builder* builder);

#endif /* TRACE_PACKETS_H*/
