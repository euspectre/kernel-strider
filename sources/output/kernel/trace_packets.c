#include "trace_packets.h"

#include <linux/slab.h> /* kmalloc */

#include "uuid_generator.h" /* uuid for trace */

#include "trace_definition.h"
#include <kedr/event_collector/event_handler.h>

/********************* CTF meta stream *******************************/

int ctf_stream_meta_init(struct ctf_stream_meta* stream_meta,
    struct ctf_trace* trace)
{
    stream_meta->metadata = trace->t_ops->get_metadata(trace,
        &stream_meta->metadata_length);
    if(stream_meta->metadata == NULL) return -ENOMEM;
    
    stream_meta->trace = trace;
    stream_meta->current_pos = 0;

    return 0;
}

void ctf_stream_meta_destroy(struct ctf_stream_meta* stream_meta)
{
    kfree(stream_meta->metadata);
}

ssize_t ctf_stream_meta_next_packet(struct ctf_stream_meta* stream_meta,
    struct msg_builder* builder)
{
    ssize_t size = 0;
    ssize_t result;
    /* Size of metadata for write (without header)*/
    size_t meta_size;
    /* Maximum size of metadata for write */
    size_t meta_size_max;
    /* Rest size of metadata for send */
    size_t meta_size_rest;
    /* Position for rollback */
    size_t begin_pos;

    struct metadata_packet_header* packet_header;
    char* metadata_buf;
    
    if(stream_meta->current_pos >= stream_meta->metadata_length)
        return 0;
    
    begin_pos = msg_builder_get_len(builder);
    result = msg_builder_append(builder, packet_header);
    if(result < 0) return result;
    
    size += result;
    
    meta_size_max = msg_builder_get_max_len(builder)
        - msg_builder_get_len(builder);
    meta_size_rest =  stream_meta->metadata_length
        - stream_meta->current_pos;
    
    meta_size = meta_size_max;;
    if(meta_size > meta_size_rest) meta_size = meta_size_rest;
    
    result = msg_builder_append_struct(builder, meta_size,
        1, (void**)&metadata_buf);
    if(result < 0)
    {
        msg_builder_trim_msg(builder, begin_pos);
        return result;
    }
    size += result;
    
    memcpy(metadata_buf, stream_meta->metadata
        + stream_meta->current_pos, meta_size);
    stream_meta->current_pos += meta_size;
    
    return size;
}

/************************* CTF trace **********************************/
int ctf_trace_init(struct ctf_trace* trace,
    const struct ctf_trace_operations* ops)
{
    trace->t_ops = ops;
    generate_uuid(trace->uuid);
    
    return 0;
}

void ctf_trace_destroy(struct ctf_trace* trace)
{
}

ssize_t ctf_trace_append_packet_header(struct ctf_trace* trace,
    struct msg_builder* builder, stream_id_t stream_id)
{
    return trace->t_ops->append_packet_header(trace, builder, stream_id);
}

/******************** KEDR CTF trace **********************************/
/* Callbacks for trace */
// TODO: correct names should be here
extern char _binary_meta_format_start[];
extern char _binary_meta_format_end[];

static int kedr_ctf_trace_snprintf_meta(char* buffer, size_t size,
    char* ctf_meta_format, struct ctf_trace* trace)
{

#define combine_bytes_2(char_buf) (((int)(char_buf)[0]) << 8) | (int)((char_buf)[1])
#define combine_bytes_4(char_buf) (combine_bytes_2(char_buf) << 16)\
| combine_bytes_2((char_buf) + 2)
    
    int time_low = combine_bytes_4(trace->uuid);
    int time_middle = combine_bytes_2(trace->uuid + 4);
    int time_high = combine_bytes_2(trace->uuid + 6);
    int clock_seq = combine_bytes_2(trace->uuid + 8);
    int node_high = combine_bytes_2(trace->uuid + 10);
    int node_low = combine_bytes_4(trace->uuid + 12);

#undef combine_bytes_4
#undef combine_bytes_2
    
    return snprintf(buffer, size, ctf_meta_format,
        time_low, time_middle, time_high, clock_seq, node_high, node_low);
}

static char* kedr_ctf_trace_ops_get_metadata(struct ctf_trace* trace,
    size_t* length_p)
{
    char* metadata;
    size_t length;
    /* Need null-terminating format */
    char* format;
    size_t format_length = _binary_meta_format_end - _binary_meta_format_start;
    format = kmalloc(format_length + 1, GFP_KERNEL);
    if(format == NULL)
    {
        pr_err("Failed to allocate format for metadata.");
        return NULL;
    }
    
    memcpy(format, _binary_meta_format_start, format_length);
    format[format_length] = '\0';
    
    length = kedr_ctf_trace_snprintf_meta(NULL, 0, format, trace);
    
    metadata = kmalloc(length + 1, GFP_KERNEL);
    if(metadata == NULL)
    {
        pr_err("Failed to allocate metadata.");
        kfree(format);
        return NULL;
    }
    
    kedr_ctf_trace_snprintf_meta(metadata, length + 1, format, trace);
    
    kfree(format);
    
    *length_p = length;
    return metadata;
}

static ssize_t kedr_ctf_trace_ops_append_packet_header(
    struct ctf_trace* trace, struct msg_builder* builder,
    stream_id_t stream_id)
{
    ssize_t result;
    struct execution_event_packet_header* packet_header;
    
    result = msg_builder_append(builder, packet_header);
    if(result < 0) return result;
    
    packet_header->magic = htonl(CTF_MAGIC);
    memcpy(packet_header->uuid, trace->uuid, sizeof(packet_header->uuid));
    packet_header->stream_id = stream_id;
    
    return result;
}

static struct ctf_trace_operations kedr_ctf_trace_ops =
{
    .get_metadata = kedr_ctf_trace_ops_get_metadata,
    .append_packet_header = kedr_ctf_trace_ops_append_packet_header
};

int kedr_ctf_trace_init(struct kedr_ctf_trace* kedr_trace,
    struct execution_event_collector* collector)
{
    int result;
    
    result = ctf_trace_init(&kedr_trace->base, &kedr_ctf_trace_ops);
    if(result) return result;
    
    kedr_trace->collector = collector;
    
    return 0;
}

void kedr_ctf_trace_destroy(struct kedr_ctf_trace* kedr_trace)
{
    ctf_trace_destroy(&kedr_trace->base);
}

/************************* CTF stream *********************************/
int ctf_stream_init(struct ctf_stream* stream,
    struct ctf_trace* trace, stream_id_t stream_id,
    const struct ctf_stream_operations* ops)
{
    stream->trace = trace;
    stream->stream_id = stream_id;
    stream->s_ops = ops;
    
    return 0;
}

void ctf_stream_destroy(struct ctf_stream* stream)
{
    /* Do nothing */
}

ssize_t ctf_stream_next_packet(struct ctf_stream* stream,
    struct msg_builder* builder)
{
    size_t size = 0;
    ssize_t result;
    /* For rollback in case of error */
    size_t begin_pos;
    /* 'data' for stream operations */
    void* data = NULL;
    /* timestamp of event read, currently unused */
    __u64 ts;
    
    begin_pos = msg_builder_get_len(builder);
    
    result = stream->s_ops->append_stream_header(stream, builder, &data);
    if(result < 0) return result;
    size += result;
    result = stream->s_ops->append_event(stream, builder, data, &ts);
    if(result <= 0)
    {
        msg_builder_trim_msg(builder, begin_pos);
        if(stream->s_ops->free_data)
            stream->s_ops->free_data(data);
        return result;
    }
    for(;
        result > 0;
        result = stream->s_ops->append_event(stream, builder, data, &ts))
    {
        size += result;
    }
    
    stream->s_ops->prepare_packet(stream, builder, data);

    if(stream->s_ops->free_data)
        stream->s_ops->free_data(data);
    /* Prepare packet_count for the next packet */
    stream->packet_count++;

    return size;
}

/******************** KEDR CTF stream *********************************/
/* Callback operations */
struct kedr_ctf_stream_data
{
    /* Packet context fields will be filled when prepare packet */
    struct execution_event_packet_context* packet_context;
    
    __u64 timestamp_begin;
    __u64 timestamp_end;
    /* only first event affects on timestamp_begin */
    int has_event;
    /* Need for calculate size of packet collected */
    size_t begin_pos;
};

ssize_t kedr_ctf_stream_ops_append_stream_header(
    struct ctf_stream* stream, struct msg_builder* builder,
    void** data_p)
{
    ssize_t result;
    
    struct kedr_ctf_stream_data* data;
    
    data = kmalloc(sizeof(*data), GFP_KERNEL);
    if(data == NULL)
    {
        pr_err("Failed to allocate data for stream packet.");
        return -ENOMEM;
    }
    
    data->has_event = 0;
    data->begin_pos = msg_builder_get_len(builder);
    
    result = msg_builder_append(builder, data->packet_context);
    if(result < 0)
    {
        kfree(data);
        return result;
    }
    
    *data_p = data;
    /* All fields will be filled when prepare whole packet */
    return result;
}

static void kedr_ctf_stream_ops_free_data(void* data)
{
    kfree(data);
}


/* 
 * Helpers for the callback function which read message from collector.
 * 
 * For corresponded message type add to message and fill
 * stream_event_context(if needed) and event_context.
 * 
 * NOTE: tid is set externally.
 */
static ssize_t
kedr_ctf_stream_process_event_ma(struct msg_builder* builder,
    struct execution_message_ma* message_ma, size_t size)
{
    ssize_t result;
    ssize_t msg_size = 0;
    
    char n_subevents;
    
    struct execution_message_ma_subevent* message_ma_subevent;
    int i;
    struct execution_stream_event_context_ma_add* context_ma_add;
    struct execution_event_ma_payload* payload_ma;
    struct execution_event_ma_payload_elem* payload_ma_elem;

    BUG_ON(message_ma->base.type != execution_event_type_ma);
    
    n_subevents = message_ma->n_subevents;
    BUG_ON(size < sizeof(*message_ma) + n_subevents * sizeof(*message_ma_subevent));
    message_ma_subevent = message_ma->subevents;
    
    result = msg_builder_append(builder, context_ma_add);
    if(result < 0) return result;
    msg_size += result;
    
    context_ma_add->n_subevents = n_subevents;
    
    result = msg_builder_append(builder, payload_ma);
    if(result < 0) goto err;
    msg_size += result;

    /* Nothing to fill - structure is empty and terminates with 0-sized array */
    result = msg_builder_append_array(builder,
        n_subevents, payload_ma_elem);
    if(result < 0) goto err;
    msg_size += result;
    
    for(i = 0; i < n_subevents;
        i++, message_ma_subevent++, payload_ma_elem++)
    {
        payload_ma_elem->pc = hton_addr(message_ma_subevent->pc);
        payload_ma_elem->addr = hton_addr(message_ma_subevent->addr);
        payload_ma_elem->size = hton_addr(message_ma_subevent->size);
        payload_ma_elem->access_type = message_ma_subevent->access_type;
    }
    return msg_size;

err:
    msg_builder_trim_msg(builder, -msg_size);
    return result;
}

static ssize_t
kedr_ctf_stream_process_event_lma(struct msg_builder* builder,
    struct execution_message_lma* message_lma, size_t size)
{
    ssize_t result;
    
    struct execution_event_lma_payload* payload_lma;

    BUG_ON(size < sizeof(*message_lma));
    
    result = msg_builder_append(builder, payload_lma);
    if(result < 0) return result;

    payload_lma->pc = hton_addr(message_lma->pc);
    payload_lma->addr = hton_addr(message_lma->addr);
    payload_lma->size = hton_addr(message_lma->size);

    return result;
}

static ssize_t
kedr_ctf_stream_process_event_mb(struct msg_builder* builder,
    struct execution_message_mb* message_mb, size_t size)
{
    ssize_t result;
    
    struct execution_event_mb_payload* payload_mb;

    BUG_ON(size < sizeof(*message_mb));
    
    result = msg_builder_append(builder, payload_mb);
    if(result < 0) return result;

    payload_mb->pc = hton_addr(message_mb->pc);

    return result;
}

static ssize_t
kedr_ctf_stream_process_event_alloc(struct msg_builder* builder,
    struct execution_message_alloc* message_alloc, size_t size)
{
    ssize_t result;
    
    struct execution_event_alloc_payload* payload_alloc;

    BUG_ON(size < sizeof(*message_alloc));
    
    result = msg_builder_append(builder, payload_alloc);
    if(result < 0) return result;

    payload_alloc->pc = hton_addr(message_alloc->pc);
    payload_alloc->size = hton_size(message_alloc->size);
    payload_alloc->pointer = hton_addr(message_alloc->pointer);

    return result;
}

static ssize_t
kedr_ctf_stream_process_event_free(struct msg_builder* builder,
    struct execution_message_free* message_free, size_t size)
{
    ssize_t result;
    
    struct execution_event_free_payload* payload_free;

    BUG_ON(size < sizeof(*message_free));
    
    result = msg_builder_append(builder, payload_free);
    if(result < 0) return result;

    payload_free->pc = hton_addr(message_free->pc);
    payload_free->pointer = hton_addr(message_free->pointer);

    return result;
}

static ssize_t
kedr_ctf_stream_process_event_lock(struct msg_builder* builder,
    struct execution_message_lock* message_lock, size_t size)
{
    ssize_t result;
    ssize_t msg_size = 0;
    
    struct execution_stream_event_context_lock_add* context_lock_add;
    struct execution_event_lock_payload* payload_lock;

    BUG_ON(size < sizeof(*message_lock));
    
    result = msg_builder_append(builder, context_lock_add);
    if(result < 0) return result;
    msg_size += result;
    
    context_lock_add->type = message_lock->type;

    result = msg_builder_append(builder, payload_lock);
    if(result < 0)
    {
        msg_builder_trim_msg(builder, -msg_size);
        return result;
    }
    msg_size += result;

    payload_lock->pc = hton_addr(message_lock->pc);
    payload_lock->object = hton_addr(message_lock->obj);

    return msg_size;
}

static ssize_t
kedr_ctf_stream_process_event_sw(struct msg_builder* builder,
    struct execution_message_sw* message_sw, size_t size)
{
    ssize_t result;
    
    struct execution_event_sw_payload* payload_sw;

    BUG_ON(size < sizeof(*message_sw));
    
    result = msg_builder_append(builder, payload_sw);
    if(result < 0) return result;

    payload_sw->pc = hton_addr(message_sw->pc);
    payload_sw->object = hton_addr(message_sw->obj);

    return result;
}


static ssize_t
kedr_ctf_stream_process_event_tcj(struct msg_builder* builder,
    struct execution_message_tcj* message_tcj, size_t size)
{
    ssize_t result;
    
    struct execution_event_tcj_payload* payload_tcj;

    BUG_ON(size < sizeof(*message_tcj));
    
    result = msg_builder_append(builder, payload_tcj);
    if(result < 0) return result;

    payload_tcj->pc = hton_addr(message_tcj->pc);
    payload_tcj->child_tid = hton_tid(message_tcj->child_tid);

    return result;
}

static ssize_t
kedr_ctf_stream_process_event_fee(struct msg_builder* builder,
    struct execution_message_fee* message_fee, size_t size)
{
    ssize_t result;
    
    struct execution_event_fee_payload* payload_fee;

    BUG_ON(size < sizeof(*message_fee));
    
    result = msg_builder_append(builder, payload_fee);
    if(result < 0) return result;

    payload_fee->func = hton_addr(message_fee->func);

    return result;
}


/* 
 * Data passed to process_event callback for read message from collector.
 */
struct kedr_ctf_stream_process_event_data
{
    /* Where to write data */
    struct msg_builder* builder;
    /* Stored data between events */
    struct kedr_ctf_stream_data* stream_data;
    /* timestamp of the event to set */
    __u64 *ts_p;
};

/* 
 * Callback function for read events from event collector.
 * 
 * Return number of data loaded into 'stream_info'.
 * 
 * Return -EFBIG if data wasn't be loaded because of message size limit.
 */
static int kedr_ctf_stream_process_message(const void* msg, size_t size,
    int cpu, u64 ts, bool *consume, void* user_data)
{
    ssize_t result;
    ssize_t msg_size = 0;
    
    struct kedr_ctf_stream_process_event_data* process_event_data =
        (struct kedr_ctf_stream_process_event_data*)user_data;
    struct msg_builder* builder = process_event_data->builder;
    struct kedr_ctf_stream_data* stream_data = process_event_data->stream_data;
    
    struct execution_message_base* message =
        (struct execution_message_base*)msg;
        
    char type = message->type;
    
    struct execution_event_header* event_header;
    struct execution_stream_event_context* stream_event_context;

    result = msg_builder_append(builder, event_header);
    if(result < 0) goto err;
    msg_size += result;
    
    result = msg_builder_append(builder, stream_event_context);
    if(result < 0) goto err;
    msg_size += result;
    
    timestamp_nt_set(&stream_event_context->timestamp, ts); 
    stream_event_context->tid = hton_tid(message->tid);
    
    switch(type)
    {
    case execution_message_type_ma:
        result = kedr_ctf_stream_process_event_ma(builder,
            (struct execution_message_ma*)message, size);
        stream_event_context->type = execution_event_type_ma;
    break;
    case execution_message_type_lma:
        result = kedr_ctf_stream_process_event_lma(builder,
            (struct execution_message_lma*)message, size);
        stream_event_context->type = execution_event_type_lma;
    break;
    case execution_message_type_mrb:
        result = kedr_ctf_stream_process_event_mb(builder,
            (struct execution_message_mb*)message, size);
        stream_event_context->type = execution_event_type_mrb;
    break;
    case execution_message_type_mwb:
        result = kedr_ctf_stream_process_event_mb(builder,
            (struct execution_message_mb*)message, size);
        stream_event_context->type = execution_event_type_mwb;
    break;
    case execution_message_type_mfb:
        result = kedr_ctf_stream_process_event_mb(builder,
            (struct execution_message_mb*)message, size);
        stream_event_context->type = execution_event_type_mfb;
    break;
    case execution_message_type_alloc:
        result = kedr_ctf_stream_process_event_alloc(builder,
            (struct execution_message_alloc*)message, size);
        stream_event_context->type = execution_event_type_alloc;
    break;
    case execution_message_type_free:
        result = kedr_ctf_stream_process_event_free(builder,
            (struct execution_message_free*)message, size);
        stream_event_context->type = execution_event_type_free;
    break;
    case execution_message_type_lock:
        result = kedr_ctf_stream_process_event_lock(builder,
            (struct execution_message_lock*)message, size);
        stream_event_context->type = execution_event_type_lock;
    break;
    case execution_message_type_unlock:
        result = kedr_ctf_stream_process_event_lock(builder,
            (struct execution_message_lock*)message, size);
        stream_event_context->type = execution_event_type_unlock;
    break;
    case execution_message_type_signal:
        result = kedr_ctf_stream_process_event_sw(builder,
            (struct execution_message_sw*)message, size);
        stream_event_context->type = execution_event_type_signal;
    break;
    case execution_message_type_wait:
        result = kedr_ctf_stream_process_event_sw(builder,
            (struct execution_message_sw*)message, size);
        stream_event_context->type = execution_event_type_wait;
    break;
    case execution_message_type_tcreate:
        result = kedr_ctf_stream_process_event_tcj(builder,
            (struct execution_message_tcj*)message, size);
        stream_event_context->type = execution_event_type_tcreate;
    break;
    case execution_message_type_tjoin:
        result = kedr_ctf_stream_process_event_tcj(builder,
            (struct execution_message_tcj*)message, size);
        stream_event_context->type = execution_event_type_tjoin;
    break;
    case execution_message_type_fentry:
        result = kedr_ctf_stream_process_event_fee(builder,
            (struct execution_message_fee*)message, size);
        stream_event_context->type = execution_event_type_fentry;
    break;
    case execution_message_type_fexit:
        result = kedr_ctf_stream_process_event_fee(builder,
            (struct execution_message_fee*)message, size);
        stream_event_context->type = execution_event_type_fexit;
    break;

    default:
        pr_err("Unknown message type: %d. Ignore it.", (int)type);
        goto ignore;
    }
    if(result < 0) goto err;
    
    msg_size += result;
    

    *consume = 1;
    
    if(!stream_data->has_event)
    {
        stream_data->timestamp_begin = ts;
        stream_data->has_event = 1;
    }
    
    stream_data->timestamp_end = ts;
    
    *process_event_data->ts_p = ts;

    return (int)msg_size;
err:
    if(msg_size > 0) msg_builder_trim_msg(builder, -msg_size);
    return (int)result;
ignore:
    *consume = 1;
    if(msg_size > 0) msg_builder_trim_msg(builder, -msg_size);
    return 0;
}


static ssize_t kedr_ctf_stream_ops_append_event(
    struct ctf_stream* stream, struct msg_builder* builder,
    void* data, __u64* ts_p)
{
    int result;
    struct kedr_ctf_stream_data* stream_data =
        (struct kedr_ctf_stream_data*)data;
    
    struct kedr_ctf_stream_process_event_data process_event_data =
    {
        .builder = builder,
        .stream_data = stream_data,
        .ts_p = ts_p
    };
    
    struct kedr_ctf_trace* kedr_trace = container_of(stream->trace,
        typeof(*kedr_trace), base);
    
    do
    {
        size_t first_pos = msg_builder_get_len(builder);
        result = execution_event_collector_read_message(
            kedr_trace->collector,
            &kedr_ctf_stream_process_message,
            &process_event_data);
        if(result >= 0)
        {
            size_t last_pos = msg_builder_get_len(builder);
            if(result + first_pos != last_pos)
            {
                pr_err("collector_read_message() return %zu, but position changed on %zu",
                    (size_t)result, (last_pos - first_pos));
            }
        }
    }while(result == 0);/* result==0 means ignore message */
    return result;
}

static void kedr_ctf_stream_ops_prepare_packet(struct ctf_stream* stream,
    struct msg_builder* builder, void* data)
{
    struct kedr_ctf_stream_data* stream_data =
        (struct kedr_ctf_stream_data*)data;
    struct execution_event_packet_context* packet_context = 
        stream_data->packet_context;
    
    packet_context->stream_packet_count = htons(stream->stream_id);
    packet_context->content_size = htonl(8 *
        (__u32)(msg_builder_get_len(builder) - stream_data->begin_pos));
    packet_context->packet_size = packet_context->content_size;
    
    BUG_ON(!stream_data->has_event);
    timestamp_nt_set(&packet_context->timestamp_begin,
        stream_data->timestamp_begin);
    timestamp_nt_set(&packet_context->timestamp_end,
        stream_data->timestamp_end);
}

static struct ctf_stream_operations kedr_ctf_stream_ops =
{
    .append_stream_header   = kedr_ctf_stream_ops_append_stream_header,
    .append_event           = kedr_ctf_stream_ops_append_event,
    .free_data              = kedr_ctf_stream_ops_free_data,
    .prepare_packet         = kedr_ctf_stream_ops_prepare_packet
};

int kedr_ctf_stream_init(struct kedr_ctf_stream* kedr_stream,
    struct kedr_ctf_trace* kedr_trace)
{
    int result = ctf_stream_init(&kedr_stream->base,
        &kedr_trace->base, 0, &kedr_ctf_stream_ops);
    
    if(result < 0) return result;
    
    // TODO: other initialization may be here
    return 0;
}

void kedr_ctf_stream_destroy(struct kedr_ctf_stream* kedr_stream)
{
    ctf_stream_destroy(&kedr_stream->base);
}
