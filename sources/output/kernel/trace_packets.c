#include "trace_packets.h"

#include <linux/slab.h> /* kmalloc */

#include "uuid_generator.h" /* uuid for trace */

#include "trace_definition.h"

#include <asm/byteorder.h> /* endianess */

#include "config.h" /* ring_buffer_consume() signature*/

#define PACKETS_BETWEEN_RECHECK_BUFFERS 10

/*
 * 'lost_events' parameter to ring_buffer_peek() and
 * ring_buffer_consume(); depends from kernel version.
 */
#if defined(RING_BUFFER_CONSUME_HAS_4_ARGS)
#define LOST_EVENTS_ARG , NULL
#elif defined(RING_BUFFER_CONSUME_HAS_3_ARGS)
#define LOST_EVENTS_ARG
#else
#error RING_BUFFER_CONSUME_HAS_4_ARGS or RING_BUFFER_CONSUME_HAS_3_ARGS should be defined.
#endif

/******************Helpers for extract messages from buffer************/
/* 
 * Peek message from the buffer.
 */
static inline struct execution_message_base* message_peek(
    struct event_collector_buffer* buffer, int cpu, size_t* msg_len, uint64_t* ts)
{
    struct execution_message_base* message;
    
    uint64_t ts_rb;/* Timestamp used only for call ring buffer functions */
    struct ring_buffer_event* event = 
#if defined(RING_BUFFER_CONSUME_HAS_4_ARGS)    
        ring_buffer_peek(buffer->rbuffer, cpu, &ts_rb, NULL);
#else
        ring_buffer_peek(buffer->rbuffer, cpu, &ts_rb);
#endif
    if(event == NULL) return NULL;

    if(msg_len) *msg_len = ring_buffer_event_length(event);
    message = ring_buffer_event_data(event);
    if(ts) *ts = message->ts;
    
    return message;
}

/* 
 * Skip current message from buffer(e.g., after processing by peek).
 * 
 * Also set 'lost_events_total' parameter, if it is not NULL, to the total
 * number of events lost since buffer is start.
 * 
 * NB: Events lost - either failed to write or dropped(e.g. due to overwrite).
 * 
 * NOTE: Caller should check previously that event is not NULL.
 */
static inline void message_skip(struct event_collector_buffer* buffer,
    int cpu, unsigned long* lost_events_total)
{
    struct ring_buffer_event* event;
    struct execution_message_base* message;
    uint64_t ts_rb;/* Timestamp used only for call ring buffer functions */
#if defined(RING_BUFFER_CONSUME_HAS_4_ARGS)
    unsigned long lost_events;
    event = ring_buffer_consume(buffer->rbuffer, cpu, &ts_rb, &lost_events);
    buffer->dropped_events[cpu] += lost_events;
#else
    event = ring_buffer_consume(buffer->rbuffer, cpu, &ts_rb);
    buffer->dropped_events[cpu] = ring_buffer_overrun_cpu(buffer->rbuffer, cpu)
        + ring_buffer_commit_overrun_cpu(buffer->rbuffer, cpu);
#endif
    BUG_ON(event == NULL);
    
    message = ring_buffer_event_data(event);
    
    if(lost_events_total)
        *lost_events_total = buffer->dropped_events[cpu] + message->missed_events;
}

/******************KEDR trace initialization/finalization**************/

int kedr_trace_init(struct kedr_trace* trace,
    size_t buffer_normal_size, size_t buffer_critical_size)
{
    int result = execution_event_collector_init(&trace->event_collector,
        buffer_normal_size, buffer_critical_size);
    if(result < 0) return result;
    
    generate_uuid(trace->uuid);
    
    trace->current_packets_rest = 0;
    
    return 0;
}

void kedr_trace_destroy(struct kedr_trace* trace)
{
    execution_event_collector_destroy(&trace->event_collector);
}

/* Helper for search oldest message */
static void oldest_message_in_buffer(struct event_collector_buffer* buffer,
    struct event_collector_buffer** last_buffer_p,
    int* last_cpu_p, uint64_t* last_ts_p)
{
    int cpu;

    uint64_t ts;/* Timestamp used for sorting */

    for(cpu = 0; cpu < NR_CPUS; cpu++)
    {
        if(message_peek(buffer, cpu, NULL, &ts) == NULL) continue;
        
        if((*last_buffer_p == NULL) || (*last_ts_p > ts))
        {
            *last_buffer_p = buffer;
            *last_cpu_p = cpu;
            *last_ts_p = ts;
        }
    }
}



/* 
 * Update current buffer and cpu according to the messages in collector.
 * 
 * Return 0 on success, 1 if no messages in the collector.
 */
static int kedr_trace_update_subbuffer(struct kedr_trace* trace)
{
    /* 
     * Buffer and cpu, contained last message, and timestamp of that message.
     * 
     * Note, that timestamp calculated is not precise, so message found also
     * is not precisely the last one.
     * But it is sufficient for out purposes:
     * 
     * 1) Any message will be extracted in some time.
     * 2) Number of messages extracted before some message and having
     * timestamps less(in precise sence) than that message is
     * 
     * N = k*rate + o(rate),
     * 
     * where 'rate' is maximum rate of messages in sub buffer,
     * k = constant.
     * 
     * Assuming rate is limited with rate_max,
     * 
     * N <= k * rate_max + C1 = C2.
     */
    struct event_collector_buffer* last_buffer = NULL;/* NULL -  not found*/
    int last_cpu = 0;
    uint64_t last_ts = 0;
    /* Firstly check buffer with normal messages */
    oldest_message_in_buffer(&trace->event_collector.buffer_normal,
        &last_buffer, &last_cpu, &last_ts);
    /* 
     * Then check buffer with critical messages -
     * less chance of empty subbuffers.
     */
    oldest_message_in_buffer(&trace->event_collector.buffer_critical,
        &last_buffer, &last_cpu, &last_ts);

    
    if(last_buffer)
    {
        trace->current_buffer = last_buffer;
        trace->current_cpu = last_cpu;
        return 0;
    }
    else
    {
        return 1;/* buffer is empty */
    }
}


/* 
 * Add packet header and packet context corresponded to current buffer.
 * 
 * Set packet_context_p pointed to the added packet context for fill it
 * events-depended fields(timestamps, sizes).
 */
static ssize_t kedr_trace_begin_packet(struct kedr_trace* trace,
    struct msg_builder* builder,
    struct execution_event_packet_context** packet_context_p)
{
    ssize_t result;
    ssize_t size;

    struct execution_event_packet_header* packet_header;
    struct execution_event_packet_context* packet_context;
    
    result = msg_builder_append(builder, packet_header);
    if(result < 0) return result;
    
    size = result;
    
    result = msg_builder_append(builder, packet_context);
    if(result < 0)
    {
        msg_builder_trim_msg(builder, -size);
        return result;
    }
    
    size += result;
    
    packet_header->magic = CTF_MAGIC;
    memcpy(packet_header->uuid, trace->uuid, 16);
    packet_header->stream_type = (trace->current_buffer
        == &trace->event_collector.buffer_critical)
        ? execution_stream_type_critical
        : execution_stream_type_normal;
    packet_header->cpu = trace->current_cpu;
    
    *packet_context_p = packet_context;
    
    return size;
}

/*
 * Add event to the packet.
 * 
 * Return number of bytes written.
 * 
 * Return negative error code if failed to write event.
 * Return 0 if message doesn't produce event.
 */
static ssize_t kedr_trace_add_event(struct kedr_trace* trace,
    struct msg_builder* builder, struct execution_message_base* message,
    size_t size, uint64_t ts);

ssize_t kedr_trace_next_packet(struct kedr_trace* trace,
    struct msg_builder* builder)
{
    struct execution_message_base* message;
    size_t msg_len;
    uint64_t ts;
    
    ssize_t result;
    ssize_t size = 0;
    
    struct execution_event_packet_context* packet_context;
    
    /* Attempt to build one packet */
    do
    {
        int is_first_event = 1;
        unsigned long lost_events_total = 0;
        
        if(trace->current_packets_rest == 0)
        {
            if(kedr_trace_update_subbuffer(trace))
            {
                /* nothing to send */
                return -EAGAIN;
            }
            trace->current_packets_rest = PACKETS_BETWEEN_RECHECK_BUFFERS;
        }
        
        result = kedr_trace_begin_packet(trace, builder, &packet_context);
        if(result < 0) return result;
        
        size = result;

        /* Add events while it is possible. */
        while(1)
        {
            // TODO: process lost messages
            message = message_peek(trace->current_buffer,
                trace->current_cpu, &msg_len, &ts);

            if(message == NULL) break;
            
            result = kedr_trace_add_event(trace, builder,
                message, msg_len, ts);
            if(result < 0) break;

            message_skip(trace->current_buffer, trace->current_cpu,
                &lost_events_total);
            
            if(result == 0)
            {
                /* Event has been read but ignored*/
                continue;
            }
 
            size += result;
            
            packet_context->timestamp_end = ts;
            if(is_first_event)
            {
                packet_context->timestamp_begin = ts;
                is_first_event = 0;
            }
        }
        if(is_first_event)
        {
            msg_builder_trim_msg(builder, -size);
            if(message == NULL)
            {
                /* current buffer is empty */
                trace->current_packets_rest = 0;
                continue;
            }
            /* Failed to write first event */
            return result;
        }
        /* Set packet fields according to events collected */
        packet_context->content_size
            = msg_builder_get_len(builder) * 8;/*in bits */
        packet_context->packet_size
            = ALIGN_VAL(packet_context->content_size, 64);
        //pr_info("Size of packet formed is %d, content size is %d.",
        //    packet_context->packet_size, packet_context->content_size);
        
        packet_context->lost_events_total = lost_events_total;
        
        packet_context->stream_packet_count =
            trace->current_buffer->packet_counters[trace->current_cpu]++;
        
        trace->current_packets_rest--;
        
        return size;
    } while(1);
}

/********************* KEDR meta stream *******************************/
static int print_uuid(char* buf, int buf_size, void* user_data)
{
    struct kedr_trace* trace = user_data;

    char str[37];
    uuid_to_str(trace->uuid, str);
    str[36] = '\0';
    
    return snprintf(buf, buf_size, "%s", str);
}

static int print_nr_cpus(char* buf, int buf_size, void* user_data)
{
    (void)user_data;
    
    return snprintf(buf, buf_size, "%d", (int)NR_CPUS);
}

static int print_pointer_bits(char* buf, int buf_size, void* user_data)
{
    (void)user_data;
    return snprintf(buf, buf_size, "%d", (int)sizeof(void*) * 8);
}

static int print_byte_order(char* buf, int buf_size, void* user_data)
{
    (void)user_data;
    return snprintf(buf, buf_size,
#if defined(__BIG_ENDIAN)
    "be"
#elif defined(__LITTLE_ENDIAN)
    "le"
#endif
    );
}


#define PRINT_SPEC_INT(TYPE) \
static int print_spec_##TYPE(char* buf, int buf_size, void* user_data) \
{ \
    (void)user_data; \
    return snprintf(buf, buf_size, "size = %d; align = %d; signed = %s;", \
        (int)sizeof(TYPE) * 8, (int)ALIGN_OF(TYPE) * 8, ((TYPE)(-1)) <= 0 ? "true" : "false"); \
}

PRINT_SPEC_INT(uint8_t)
PRINT_SPEC_INT(int16_t)
PRINT_SPEC_INT(uint16_t)
PRINT_SPEC_INT(int32_t)
PRINT_SPEC_INT(uint32_t)
PRINT_SPEC_INT(uint64_t)
PRINT_SPEC_INT(size_t)


static struct param_spec meta_param_specs[] =
{
    { .name = "uuid",           .print = print_uuid},
    { .name = "pointer_bits",   .print = print_pointer_bits},
    { .name = "byte_order",     .print = print_byte_order},
    { .name = "nr_cpus",        .print = print_nr_cpus},
    { .name = "uint8_t_spec",   .print = print_spec_uint8_t},
    { .name = "int16_t_spec",   .print = print_spec_int16_t},
    { .name = "uint16_t_spec",  .print = print_spec_uint16_t},
    { .name = "int32_t_spec",   .print = print_spec_int32_t},
    { .name = "uint32_t_spec",  .print = print_spec_uint32_t},
    { .name = "uint64_t_spec",  .print = print_spec_uint64_t},
    { .name = "size_t_spec",    .print = print_spec_size_t},
};



extern char _binary_ctf_meta_template_start[];
extern char _binary_ctf_meta_template_end[];


int kedr_stream_meta_init(struct kedr_stream_meta* stream_meta,
    struct kedr_trace* trace)
{
    stream_meta->trace = trace;
    
    template_parser_init(&stream_meta->meta_template_parser,
        _binary_ctf_meta_template_start, _binary_ctf_meta_template_end,
        meta_param_specs, ARRAY_SIZE(meta_param_specs), trace);
    
    stream_meta->current_chunk = NULL;
    stream_meta->current_chunk_len = 0;

    return 0;
}

void kedr_stream_meta_destroy(struct kedr_stream_meta* stream_meta)
{
    template_parser_destroy(&stream_meta->meta_template_parser);
}

/*
 * Form next packet for metadata.
 *
 * Return number of bytes written.
 * Return 0 if whole metadata has been sended.
 */
ssize_t kedr_stream_meta_next_packet(struct kedr_stream_meta* stream_meta,
    struct msg_builder* builder)
{
    ssize_t result;
    ssize_t size = 0;
    size_t size_rest;
    
    int is_empty = 1;
    struct metadata_packet_header* meta_packet_header;
    
    result = msg_builder_append(builder, meta_packet_header);
    if(result < 0) return result;
    
    size += result;
    
    size_rest = msg_builder_get_max_len(builder)
        - msg_builder_get_len(builder);
    BUG_ON(size_rest == 0);
    
    while(size_rest > 0)
    {
        int read_size;
        void* buf;
        if(stream_meta->current_chunk_len == 0)
        {
            stream_meta->current_chunk = template_parser_next_chunk(
                &stream_meta->meta_template_parser,
                &stream_meta->current_chunk_len);
            if(stream_meta->current_chunk == NULL)
            {
                result = 0;/* EOF */
                break;
            }
        }
        
        read_size = stream_meta->current_chunk_len;
        if(read_size > size_rest) read_size = size_rest;
        
        result = msg_builder_append_struct(builder, read_size, 1, &buf);
        if(result < 0) break;
        
        is_empty = 0;
        
        memcpy(buf, stream_meta->current_chunk, read_size);
        
        size+= result;
        size_rest -= result;
        
        stream_meta->current_chunk_len-= read_size;
        stream_meta->current_chunk += read_size;
    }
    
    if(is_empty)
    {
        msg_builder_trim_msg(builder, -size);
        return result;
    }
    
    /* Fill metadata packet header */
    meta_packet_header->magic = CTF_META_MAGIC;
    /*
     * Currently use uuid of the trace as uuid of its metadata.
     * 
     * This shouldn't confuse readers, because magics of normal packet and
     * metadata packet differ.
     */
    memcpy(meta_packet_header->uuid, stream_meta->trace->uuid, 16);
    
    meta_packet_header->checksum = 0;
    meta_packet_header->content_size = size * 8;
    meta_packet_header->packet_size
        = ALIGN_VAL(meta_packet_header->content_size, 32);
    meta_packet_header->compression_scheme = 0;
    meta_packet_header->encryption_scheme = 0;
    meta_packet_header->checksum_scheme = 0;
    meta_packet_header->major = 1;
    meta_packet_header->minor = 8;

    return size;
}


/*
 * Helpers for the function which read message from collector.
 *
 * For corresponded message type add and fill
 * stream_event_context(if needed) and event_fields.
 */
static ssize_t
kedr_ctf_stream_process_event_ma(struct msg_builder* builder,
    const struct execution_message_ma* message_ma, size_t size)
{
    ssize_t result;
    ssize_t msg_size = 0;

    char n_subevents;
    char n_subevents_real;

    int i;
    struct execution_event_context_ma* context_ma;
    struct execution_event_fields_ma_elem* fields_ma_elem;

    n_subevents = message_ma->n_subevents;
    
    BUG_ON(size < sizeof(*message_ma) + n_subevents
        * sizeof(struct execution_message_ma_subevent));

    /* Count real subevents(exclude ones which doesn't occure)*/
    n_subevents_real = 0;
    for(i = 0; i < n_subevents; i++)
    {
        const struct execution_message_ma_subevent* message_ma_subevent
            = message_ma->subevents + i;
        
        if(message_ma_subevent->addr) ++n_subevents_real;
    }
    if(n_subevents_real == 0)
    {
        /* 
         * Ignore event with memory accesses when really no access
         * is registered.
         */
        return 0;
    }
    

    result = msg_builder_append(builder, context_ma);
    if(result < 0) return result;
    msg_size += result;

    context_ma->n_subevents = n_subevents_real;

    result = msg_builder_append_array(builder,
        n_subevents_real, fields_ma_elem);
    if(result < 0) goto err;
    msg_size += result;

    for(i = 0; i < n_subevents; i++)
    {
        const struct execution_message_ma_subevent* message_ma_subevent
            = message_ma->subevents + i;
        
        if(message_ma_subevent->addr == 0) continue;
        
        fields_ma_elem->pc = message_ma_subevent->pc;
        fields_ma_elem->addr = message_ma_subevent->addr;
        fields_ma_elem->size = message_ma_subevent->size;
        fields_ma_elem->access_type = message_ma_subevent->access_type;
        fields_ma_elem++;
    }
    return msg_size;

err:
    msg_builder_trim_msg(builder, -msg_size);
    return result;
}

static ssize_t
kedr_ctf_stream_process_event_lma(struct msg_builder* builder,
    const struct execution_message_lma* message_lma, size_t size)
{
    ssize_t result;

    struct execution_event_fields_lma* fields_lma;

    BUG_ON(size < sizeof(*message_lma));

    result = msg_builder_append(builder, fields_lma);
    if(result < 0) return result;

    fields_lma->pc = message_lma->pc;
    fields_lma->addr = message_lma->addr;
    fields_lma->size = message_lma->size;

    return result;
}

static ssize_t
kedr_ctf_stream_process_event_ioma(struct msg_builder* builder,
    const struct execution_message_ioma* message_ioma, size_t size)
{
    ssize_t result;

    struct execution_event_fields_ioma* fields_ioma;

    BUG_ON(size < sizeof(*message_ioma));

    result = msg_builder_append(builder, fields_ioma);
    if(result < 0) return result;

    fields_ioma->pc = message_ioma->pc;
    fields_ioma->addr = message_ioma->addr;
    fields_ioma->size = message_ioma->size;
    fields_ioma->access_type = message_ioma->access_type;

    return result;
}


static ssize_t
kedr_ctf_stream_process_event_mb(struct msg_builder* builder,
    const struct execution_message_mb* message_mb, size_t size)
{
    ssize_t result;

    struct execution_event_fields_mb* fields_mb;

    BUG_ON(size < sizeof(*message_mb));

    result = msg_builder_append(builder, fields_mb);
    if(result < 0) return result;

    fields_mb->pc = message_mb->pc;

    return result;
}

static ssize_t
kedr_ctf_stream_process_event_alloc(struct msg_builder* builder,
    const struct execution_message_alloc* message_alloc, size_t size)
{
    ssize_t result;

    struct execution_event_fields_alloc* fields_alloc;

    BUG_ON(size < sizeof(*message_alloc));

    result = msg_builder_append(builder, fields_alloc);
    if(result < 0) return result;

    fields_alloc->pc = message_alloc->pc;
    fields_alloc->size = message_alloc->size;
    fields_alloc->pointer = message_alloc->pointer;

    return result;
}

static ssize_t
kedr_ctf_stream_process_event_free(struct msg_builder* builder,
    const struct execution_message_free* message_free, size_t size)
{
    ssize_t result;

    struct execution_event_fields_free* fields_free;

    BUG_ON(size < sizeof(*message_free));

    result = msg_builder_append(builder, fields_free);
    if(result < 0) return result;

    fields_free->pc = message_free->pc;
    fields_free->pointer = message_free->pointer;

    return result;
}

static ssize_t
kedr_ctf_stream_process_event_lock(struct msg_builder* builder,
    const struct execution_message_lock* message_lock, size_t size)
{
    ssize_t result;

    struct execution_event_fields_lock* fields_lock;

    BUG_ON(size < sizeof(*message_lock));

    result = msg_builder_append(builder, fields_lock);
    if(result < 0)
    {
        return result;
    }

    fields_lock->pc = message_lock->pc;
    fields_lock->object = message_lock->obj;
    fields_lock->type = message_lock->type;

    return result;
}

static ssize_t
kedr_ctf_stream_process_event_sw(struct msg_builder* builder,
    const struct execution_message_sw* message_sw, size_t size)
{
    ssize_t result;

    struct execution_event_fields_sw* fields_sw;

    BUG_ON(size < sizeof(*message_sw));

    result = msg_builder_append(builder, fields_sw);
    if(result < 0) return result;

    fields_sw->pc = message_sw->pc;
    fields_sw->object = message_sw->obj;
    fields_sw->type = message_sw->type;

    return result;
}


static ssize_t
kedr_ctf_stream_process_event_tc_before(struct msg_builder* builder,
    const struct execution_message_tc_before* message_tc_before, size_t size)
{
    ssize_t result;

    struct execution_event_fields_tc_before* fields_tc_before;

    BUG_ON(size < sizeof(*message_tc_before));

    result = msg_builder_append(builder, fields_tc_before);
    if(result < 0) return result;

    fields_tc_before->pc = message_tc_before->pc;

    return result;
}

static ssize_t
kedr_ctf_stream_process_event_tc_after(struct msg_builder* builder,
    const struct execution_message_tc_after* message_tc_after, size_t size)
{
    ssize_t result;

    struct execution_event_fields_tc_after* fields_tc_after;

    BUG_ON(size < sizeof(*message_tc_after));

    result = msg_builder_append(builder, fields_tc_after);
    if(result < 0) return result;

    fields_tc_after->pc = message_tc_after->pc;
    fields_tc_after->child_tid = message_tc_after->child_tid;

    return result;
}

static ssize_t
kedr_ctf_stream_process_event_tjoin(struct msg_builder* builder,
    const struct execution_message_tjoin* message_tjoin, size_t size)
{
    ssize_t result;

    struct execution_event_fields_tjoin* fields_tjoin;

    BUG_ON(size < sizeof(*message_tjoin));

    result = msg_builder_append(builder, fields_tjoin);
    if(result < 0) return result;

    fields_tjoin->pc = message_tjoin->pc;
    fields_tjoin->child_tid = message_tjoin->child_tid;

    return result;
}


static ssize_t
kedr_ctf_stream_process_event_fee(struct msg_builder* builder,
    const struct execution_message_fee* message_fee, size_t size)
{
    ssize_t result;

    struct execution_event_fields_fee* fields_fee;

    BUG_ON(size < sizeof(*message_fee));

    result = msg_builder_append(builder, fields_fee);
    if(result < 0) return result;

    fields_fee->func = message_fee->func;

    return result;
}

static ssize_t
kedr_ctf_stream_process_event_fc(struct msg_builder* builder,
    const struct execution_message_fc* message_fc, size_t size)
{
    ssize_t result;

    struct execution_event_fields_fc* fields_fc;

    BUG_ON(size < sizeof(*message_fc));

    result = msg_builder_append(builder, fields_fc);
    if(result < 0) return result;

    fields_fc->pc = message_fc->pc;
    fields_fc->func = message_fc->func;

    return result;
}


ssize_t kedr_trace_add_event(struct kedr_trace* trace,
    struct msg_builder* builder, struct execution_message_base* message,
    size_t size, uint64_t ts)
{
    ssize_t result;
    ssize_t msg_size = 0;

    char type = message->type;

    struct execution_event_header* event_header;
    struct execution_stream_event_context* stream_event_context;

    result = msg_builder_append(builder, event_header);
    if(result < 0) goto err;
    BUG_ON(result == 0);
    msg_size += result;

    result = msg_builder_append(builder, stream_event_context);
    if(result < 0) goto err;
    msg_size += result;

    stream_event_context->timestamp = ts;
    stream_event_context->tid = message->tid;
    stream_event_context->counter = message->counter;

    switch(type)
    {
    case execution_message_type_ma:
        result = kedr_ctf_stream_process_event_ma(builder,
            (struct execution_message_ma*)message, size);
        event_header->type = execution_event_type_ma;
    break;
    case execution_message_type_lma_update:
        result = kedr_ctf_stream_process_event_lma(builder,
            (struct execution_message_lma*)message, size);
        event_header->type = execution_event_type_lma_update;
    break;
    case execution_message_type_lma_read:
        result = kedr_ctf_stream_process_event_lma(builder,
            (struct execution_message_lma*)message, size);
        event_header->type = execution_event_type_lma_read;
    break;
    case execution_message_type_lma_write:
        result = kedr_ctf_stream_process_event_lma(builder,
            (struct execution_message_lma*)message, size);
        event_header->type = execution_event_type_lma_write;
    break;
    case execution_message_type_ioma:
        result = kedr_ctf_stream_process_event_ioma(builder,
            (struct execution_message_ioma*)message, size);
        event_header->type = execution_event_type_ioma;
    break;
    case execution_message_type_mrb:
        result = kedr_ctf_stream_process_event_mb(builder,
            (struct execution_message_mb*)message, size);
        event_header->type = execution_event_type_mrb;
    break;
    case execution_message_type_mwb:
        result = kedr_ctf_stream_process_event_mb(builder,
            (struct execution_message_mb*)message, size);
        event_header->type = execution_event_type_mwb;
    break;
    case execution_message_type_mfb:
        result = kedr_ctf_stream_process_event_mb(builder,
            (struct execution_message_mb*)message, size);
        event_header->type = execution_event_type_mfb;
    break;
    case execution_message_type_alloc:
        result = kedr_ctf_stream_process_event_alloc(builder,
            (struct execution_message_alloc*)message, size);
        event_header->type = execution_event_type_alloc;
    break;
    case execution_message_type_free:
        result = kedr_ctf_stream_process_event_free(builder,
            (struct execution_message_free*)message, size);
        event_header->type = execution_event_type_free;
    break;
    case execution_message_type_lock:
        result = kedr_ctf_stream_process_event_lock(builder,
            (struct execution_message_lock*)message, size);
        event_header->type = execution_event_type_lock;
    break;
    case execution_message_type_unlock:
        result = kedr_ctf_stream_process_event_lock(builder,
            (struct execution_message_lock*)message, size);
        event_header->type = execution_event_type_unlock;
    break;
    case execution_message_type_rlock:
        result = kedr_ctf_stream_process_event_lock(builder,
            (struct execution_message_lock*)message, size);
        event_header->type = execution_event_type_rlock;
    break;
    case execution_message_type_runlock:
        result = kedr_ctf_stream_process_event_lock(builder,
            (struct execution_message_lock*)message, size);
        event_header->type = execution_event_type_runlock;
    break;
    case execution_message_type_signal:
        result = kedr_ctf_stream_process_event_sw(builder,
            (struct execution_message_sw*)message, size);
        event_header->type = execution_event_type_signal;
    break;
    case execution_message_type_wait:
        result = kedr_ctf_stream_process_event_sw(builder,
            (struct execution_message_sw*)message, size);
        event_header->type = execution_event_type_wait;
    break;
    case execution_message_type_tc_before:
        result = kedr_ctf_stream_process_event_tc_before(builder,
            (struct execution_message_tc_before*)message, size);
        event_header->type = execution_event_type_tc_before;
    break;
    case execution_message_type_tc_after:
        result = kedr_ctf_stream_process_event_tc_after(builder,
            (struct execution_message_tc_after*)message, size);
        event_header->type = execution_event_type_tc_after;
    break;
    case execution_message_type_tjoin:
        result = kedr_ctf_stream_process_event_tjoin(builder,
            (struct execution_message_tjoin*)message, size);
        event_header->type = execution_event_type_tjoin;
    break;
    case execution_message_type_fentry:
        result = kedr_ctf_stream_process_event_fee(builder,
            (struct execution_message_fee*)message, size);
        event_header->type = execution_event_type_fentry;
    break;
    case execution_message_type_fexit:
        result = kedr_ctf_stream_process_event_fee(builder,
            (struct execution_message_fee*)message, size);
        event_header->type = execution_event_type_fexit;
    break;
    case execution_message_type_fcpre:
        result = kedr_ctf_stream_process_event_fc(builder,
            (struct execution_message_fc*)message, size);
        event_header->type = execution_event_type_fcpre;
    break;
    case execution_message_type_fcpost:
        result = kedr_ctf_stream_process_event_fc(builder,
            (struct execution_message_fc*)message, size);
        event_header->type = execution_event_type_fcpost;
    break;

    default:
        pr_err("Unknown message type: %d. Ignore it.", (int)type);
        result = 0;
    }
    if(result < 0) goto err;
    else if(result == 0) goto ignore;
    
    msg_size += result;

    return msg_size;
err:
    if(msg_size > 0) msg_builder_trim_msg(builder, -msg_size);
    return result;
ignore:
    if(msg_size > 0) msg_builder_trim_msg(builder, -msg_size);
    return 0;
}
