#include "trace_definition.h" /* form control packets, set 'seq'*/

//#include <linux/module.h> /* struct */
//#include <linux/init.h>

#include <net/sock.h>
#include <linux/skbuff.h>
#include <linux/inet.h>

#include <linux/wait.h>
#include <linux/sched.h>

//#include <linux/in.h>
//#include <linux/ip.h>
#include <linux/udp.h> /* use UDP protocol for send messages */

#include "trace_packets.h" /* Connected with 'struct trace_info' object */

#include "kedr/event_collector/event_handler.h" /* core functions for event handlers */

#include "trace_sender.h"

/******************** Session for send one trace **********************/
enum kedr_trace_session_state
{
    kedr_trace_session_state_invalid = 0,
    /* Just initialized */
    kedr_trace_session_state_ready,
    /* Currently send metadata */
    kedr_trace_session_state_send_meta,
    /* Currently send 'meta_end' mark */
    kedr_trace_session_state_meta_end_mark,
    /*
     * Currently send 'start' mark.
     * (When no events from stream has read.)
     */
    kedr_trace_session_state_start_mark,
    /* Currently send stream events */
    kedr_trace_session_state_send,
    /* 
     * Currently send 'stop' mark.
     * (When no more events are expected in the stream.)
     */
    kedr_trace_session_state_end_mark,
    /*
     * Trace is empty and session is terminated.
     * 
     * next_packet() return 0 in that state as mark of trace EOF.
     */
    kedr_trace_session_state_eof
};

struct kedr_trace_session
{
    struct kedr_ctf_trace trace;
    /* Currently only one stream */
    struct kedr_ctf_stream stream;
    /* whether no events was read from stream before*/
    int is_first_event;

    /* Pointer to the stream with metainformation if used */
    struct ctf_stream_meta* stream_meta;
    /*
     * Current state.
     * 
     * NOTE: Use volatile semantic.
     */
    enum kedr_trace_session_state state;
    /* 
     * Whether 'terminate' has issued - modificator for the state.
     * 
     * NOTE: Use volatile semantic.
     */
    int is_terminated;
    /* Queue for wait stopping */
    wait_queue_head_t stop_waiter;
};

static int kedr_trace_session_init(
    struct kedr_trace_session* trace_session,
    struct execution_event_collector* collector)
{
    int result;
    
    result = kedr_ctf_trace_init(&trace_session->trace, collector);
    if(result < 0) goto trace_init_err;
    
    result = kedr_ctf_stream_init(&trace_session->stream,
        &trace_session->trace);
    if(result < 0) goto stream_init_err;

    trace_session->is_first_event = 0;
    
    trace_session->stream_meta = NULL;

    trace_session->state = kedr_trace_session_state_ready;
    trace_session->is_terminated = 0;
    
    init_waitqueue_head(&trace_session->stop_waiter);

    return 0;

stream_init_err:
    kedr_ctf_trace_destroy(&trace_session->trace);
trace_init_err:
    return result;
}

static void kedr_trace_session_destroy(
    struct kedr_trace_session* trace_session)
{
    BUG_ON(trace_session->stream_meta != NULL);
    
    kedr_ctf_stream_destroy(&trace_session->stream);
    kedr_ctf_trace_destroy(&trace_session->trace);
}

/* Helpers for volatile fields */
static void kedr_trace_session_set_state(
    struct kedr_trace_session* trace_session,
    enum kedr_trace_session_state new_state)
{
    smp_wmb();
    trace_session->state = new_state;
    if(new_state == kedr_trace_session_state_ready)
        wake_up_all(&trace_session->stop_waiter);
}

static enum kedr_trace_session_state kedr_trace_session_get_state(
    struct kedr_trace_session* trace_session)
{
    enum kedr_trace_session_state result = trace_session->state;
    smp_rmb();
    return result;
}

static int kedr_trace_session_is_terminated(
    struct kedr_trace_session* trace_session)
{
    if(trace_session->is_terminated)
    {
        smp_rmb();
        return 1;
    }
    return 0;
}

static int kedr_trace_session_start(
    struct kedr_trace_session* trace_session)
{
    int result;
    
    BUG_ON(trace_session->state != kedr_trace_session_state_ready);
    if(kedr_trace_session_is_terminated(trace_session))
        return -ENODEV;
    
    trace_session->stream_meta =
        kmalloc(sizeof(*trace_session->stream_meta), GFP_KERNEL);
    if(trace_session->stream_meta == NULL)
    {
        pr_err("Failed to allocate stream with metadata.");
        return -ENOMEM;
    }
    
    result = ctf_stream_meta_init(trace_session->stream_meta,
        &trace_session->trace.base);
    if(result < 0)
    {
        kfree(trace_session->stream_meta);
        trace_session->stream_meta = NULL;
        return result;
    }
    
    kedr_trace_session_set_state(trace_session,
        kedr_trace_session_state_send_meta);
    
    return 0;
}

static int kedr_trace_session_is_started(
    struct kedr_trace_session* trace_session)
{
    return kedr_trace_session_get_state(trace_session)
        != kedr_trace_session_state_ready;
}

static void kedr_trace_session_stop(
    struct kedr_trace_session* trace_session)
{
    if(trace_session->state == kedr_trace_session_state_ready)
        return;/* Already stopped */
    
    switch(kedr_trace_session_get_state(trace_session))
    {
    case kedr_trace_session_state_send_meta:
        ctf_stream_meta_destroy(trace_session->stream_meta);
        kfree(trace_session->stream_meta);
        trace_session->stream_meta = NULL;
    break;
    default:
        /* no additional steps are required */
    break;
    }
    
    kedr_trace_session_set_state(trace_session,
        kedr_trace_session_state_ready);
}

/* Helpers for the next function */
static ssize_t kedr_trace_session_next_packet_meta(
    struct kedr_trace_session* trace_session,
    struct msg_builder* builder)
{
    ssize_t result;
    ssize_t size = 0;
    
    struct kedr_message_header* message_header;
    
    BUG_ON(msg_builder_has_msg(builder));
    
    result = msg_builder_append(builder, message_header);
    if(result < 0) return result;
    size += result;
    
    message_header->type = kedr_message_type_meta_ctf;
    
    result = ctf_stream_meta_next_packet(trace_session->stream_meta,
        builder);
    
    /* Empty metadata shouldn't be sent */
    if(result <= 0)
    {
        msg_builder_clean_msg(builder);
        return result;
    }
    
    size += result;
    
    return size;
}

static ssize_t kedr_trace_session_next_packet_normal(
    struct kedr_trace_session* trace_session,
    struct msg_builder* builder)
{
    ssize_t result;
    ssize_t size = 0;
    
    struct kedr_message_header* message_header;
    
    BUG_ON(msg_builder_has_msg(builder));
    
    result = msg_builder_append(builder, message_header);
    if(result < 0) return result;
    size += result;
    
    message_header->type = kedr_message_type_ctf;
    
    result = ctf_stream_next_packet(&trace_session->stream.base,
        builder);
    
    if(result < 0)
    {
        msg_builder_clean_msg(builder);
        return result;
    }
    
    size += result;
    
    return size;
}

static ssize_t kedr_trace_session_next_packet_mark(
    struct kedr_trace_session* trace_session,
    struct msg_builder* builder,
    enum kedr_message_type mark)
{
    ssize_t result;
    
    struct kedr_message_header* message_header;
    
    BUG_ON(msg_builder_has_msg(builder));
    BUG_ON((mark < kedr_message_type_mark_range_start)
        || (mark > kedr_message_type_mark_range_end));
    
    result = msg_builder_append(builder, message_header);
    if(result < 0) return result;
    
    message_header->type = mark;
    
    return result;
}

/* 
 * Extract next packet in the trace.
 * 
 * Should be executed only after 'start'.
 */
static ssize_t kedr_trace_session_next_packet(
    struct kedr_trace_session* trace_session,
    struct msg_builder* builder)
{
    ssize_t result;
    switch(kedr_trace_session_get_state(trace_session))
    {
    case kedr_trace_session_state_send_meta:
        result = kedr_trace_session_next_packet_meta(trace_session, builder);
        if(result != 0) return result;/* error or success */
        /* State transition in case EOF */
        ctf_stream_meta_destroy(trace_session->stream_meta);
        kfree(trace_session->stream_meta);
        trace_session->stream_meta = NULL;
        
        kedr_trace_session_set_state(trace_session,
            kedr_trace_session_state_meta_end_mark);
        /* Fall through */
    case kedr_trace_session_state_meta_end_mark:
        result = kedr_trace_session_next_packet_mark(trace_session,
            builder, kedr_message_type_mark_meta_ctf_end);
        if(result < 0) return result;
        
        kedr_trace_session_set_state(trace_session,
            trace_session->is_first_event
                ? kedr_trace_session_state_start_mark
                : kedr_trace_session_state_send);
    break;
    case kedr_trace_session_state_start_mark:
        result = kedr_trace_session_next_packet_mark(trace_session,
            builder, kedr_message_type_mark_meta_ctf_end);
        if(result < 0) return result;
        
        kedr_trace_session_set_state(trace_session,
            kedr_trace_session_state_send);
    break;
    case kedr_trace_session_state_send:
        result = kedr_trace_session_next_packet_normal(trace_session, builder);
        if(result != -EAGAIN) return result;/* error or success */
        /* Check 'terminated' flag in case EOF */
        if(!kedr_trace_session_is_terminated(trace_session))
            return result;

        /* State transition in case of empty terminated stream */
        kedr_trace_session_set_state(trace_session,
            kedr_trace_session_state_end_mark);
        /* Fall through */
    /* Really, this case is reachable only via previouse one or error */
    case kedr_trace_session_state_end_mark:
        result = kedr_trace_session_next_packet_mark(trace_session,
            builder, kedr_message_type_mark_trace_end);
        if(result < 0) return result;
        
        kedr_trace_session_set_state(trace_session,
            kedr_trace_session_state_eof);
    break;
    case kedr_trace_session_state_eof:
        return 0;
    break;
    default:
        BUG();
    }
    return result;
}

/* 
 * Mark trace session as terminated - no new events will be generated in
 * the event collector.
 * 
 * When all events from such trace will be read, 0 will be returned
 * instead of -EAGAIN for indicate real EOF.
 * 
 * NOTE: May be called asynchronous with other trace session functions.
 */
static void kedr_trace_session_terminate(
    struct kedr_trace_session* trace_session)
{
    BUG_ON(trace_session->is_terminated);
    smp_wmb();
    trace_session->is_terminated = 1;
}

/* 
 * Wait until trace session is stopped.
 * 
 * Return 0 on success, negative error code on fail(e.g, interrupt).
 * 
 * NOTE: May be called asynchronous with other trace session functions.
 */
static int kedr_trace_session_wait_stop(
    struct kedr_trace_session* trace_session)
{
    int result = wait_event_interruptible(trace_session->stop_waiter,
        trace_session->state == kedr_trace_session_state_ready);
    /* Barrier after reading published state */
    if(result == 0) smp_wmb();
    
    return result;
}

/************************** Trace sender ******************************/

enum trace_sender_state_type
{
	/* Uninitialized */
	trace_sender_state_invalid = 0,
	/* Initialized and wait commands */
	trace_sender_state_ready,
	/* Start session... */
	trace_sender_state_starting,
	/* Session is set */
	trace_sender_state_session,
	/* Stop session... */
	trace_sender_state_stopping,
};

/*
 * State of the sender may be changed in the recieve message callback,
 * so it cannot be protected by mutex, only spinlock.
 * But some actions, which change state, cannot be performed under
 * spinlock. E.g., message sending.
 * 
 * For such actions we allow to freeze state until action is done.
 * 
 * When someone else want to change state while it is pinned, it set
 * corresponded 'deferred' variables. While state will being unfreezed,
 * deferred variables is merged into normal ones.
 */

enum trace_sender_deferred_command
{
    trace_sender_deferred_none = 0,
    trace_sender_deferred_start,
    trace_sender_deferred_stop
};

struct trace_sender
{
	/* Main state of the sender */
    enum trace_sender_state_type state;
	/* Next fields are used only when sender send messages */
	__be32 client_addr;
	__be16 client_port;

    /* Whether state is freezed by long work */
    int is_freezed;
    /* 
     * Queue of deferred commands.
     * 
     * 'start' and 'stop' may be inserted to it at most in one instance.
     */
    enum trace_sender_deferred_command deferred_commands[2];
    /* Parameters for 'deferred_start' */
    __be32 deferred_client_addr;
    __be16 deferred_client_port;

	/* Protect state changes */
	spinlock_t lock;

    /* 
     * Trace session, to which sender is connected.
     * Currently, maximum one trace is possible.
     * 
     * When added in the 'session' state, trace session should be 
     * started.
     */
    struct kedr_trace_session* trace_session;
    /* 
     * Protect 'trace_info' field from concurrent accesses.
     */
    struct mutex trace_session_mutex;


	/* 
	 * Sequential number of the last packet.
	 * 
	 * It is accessed (and changed) only in callback for work, which
	 * is serialized in respect to itself. So, accesses to this field
	 * do not require any sync.
	 */
	int32_t seq;

	/* Is used for send messages */
	struct socket* clientsocket;
	/* Work for send packets to the client */
	struct delayed_work work;
	/* Workqueue for pending 'work' */
	struct workqueue_struct* wq;

    /* Different transmition parameters */
    
    /* Maximum size packet, in bytes */
    int transmition_size_limit;
    /* Interval between works which send packets, in jiffies*/
    int transmition_interval_jiff;
    /* Interval for re-read empty trace, in jiffies */
    int transmition_interval_empty_jiff;
    /* Maximum total size of packets sent per work */
    int transmition_total_size_limit_per_interval;
    
    /* Queue for wait stopping */
    wait_queue_head_t stop_waiter;
};

/* 
 * Helper for send message.
 * 
 * Before send, set 'seq' field in the message.
 * 
 * NOTE: first element in vector should be at least of size
 * CTF_STRUCT_SIZE(struct kedr_message_header).
 */
static int trace_sender_send_msg(struct trace_sender* sender,
	struct kvec* vec, size_t vec_num, size_t size)
{
	int result;
	struct msghdr msg;

	struct sockaddr_in to;

	BUG_ON(sender->state == trace_sender_state_ready);
	
	/* Form destination address */
	memset(&to, 0, sizeof(to));
	to.sin_family = AF_INET;
	to.sin_addr.s_addr = sender->client_addr;
	to.sin_port = sender->client_port;
	
	/* Form message itself */
	memset(&msg, 0, sizeof(msg));
	msg.msg_name = &to;
	msg.msg_namelen = sizeof(to);

	msg.msg_control = NULL;
	msg.msg_controllen = 0;

	BUG_ON((vec_num == 0) || (vec[0].iov_len
        < CTF_STRUCT_SIZE(struct kedr_message_header)));

    /* Set sequential number */
    ((struct kedr_message_header*)(vec[0].iov_base))->seq =
        htonl(sender->seq);
    
    result = kernel_sendmsg(sender->clientsocket, &msg, vec, vec_num, size);
	if(result < 0)
	{
		pr_err("Error occure while sending message.");
		return result;
	}
    sender->seq++;

	return 0;
}

/* 
 * Send given trace mark.
 */
static int trace_sender_send_trace_mark(struct trace_sender* sender,
	enum kedr_message_type mark)
{
	struct kedr_message_header msg_mark = {.type = mark};
	struct kvec vec =
	{
		.iov_base = &msg_mark,
		.iov_len = CTF_STRUCT_SIZE(struct kedr_message_header)
	};
	
    BUG_ON((mark < kedr_message_type_mark_range_start)
        || (mark > kedr_message_type_mark_range_end));
	
	return trace_sender_send_msg(sender, &vec, 1, vec.iov_len);
}


/*
 * Send trace events encoded in ctf packets.
 * 
 * 'total_size_limit' is limit (in bytes) for total size of packets sent.
 * 
 * Return number of transmitted bytes.
 * Returned 0 means that no trace is processed by the sender.
 * 
 * Return negative error code on fail.
 * 
 * Return -EAGAIN if no message was transmitted because trace is empty.
 * 
 * NOTE: If found that stopped trace has no more messages, call
 * ctf_trace_info_break_session() for it and remove from processed traces.
 */
static int trace_sender_send_trace(struct trace_sender* sender,
    int size_limit)
{
    int result = 0;
    int size = 0;
    int msg_size;
    /* Upper limit to stop the cycle*/
    int size_out = size_limit - sender->transmition_size_limit;
    
    struct msg_builder builder;
    msg_builder_init(&builder, sender->transmition_size_limit);

    result = mutex_lock_interruptible(&sender->trace_session_mutex);
    if(result < 0)
    {
        msg_builder_destroy(&builder);
        return result;
    }
    
    for(; size <= size_out; size += msg_size)
    {
        struct kedr_trace_session* trace_session = sender->trace_session;
        if((trace_session == NULL)
            || !kedr_trace_session_is_started(trace_session))
        {
            /* no traces */
            msg_size = 0;
            break;
        }

        msg_size = (int)kedr_trace_session_next_packet(trace_session,
            &builder);
        
        if(msg_size > 0) /* Success */
        {
            result = trace_sender_send_msg(sender,
                msg_builder_get_vec(&builder),
                msg_builder_get_vec_len(&builder),
                msg_builder_get_len(&builder));

            if(result < 0)
            {
                /* As if the message was lost in network. */
                pr_err("Failed to send msg. Ignore it.");
                sender->seq++;
            }
        }
        
        else if(msg_size == 0) /* EOF for the trace */
        {
            kedr_trace_session_stop(trace_session);
            sender->trace_session = NULL;
        }
        else
        {
            break;
        }
        msg_builder_clean_msg(&builder);
    }
    if(size == 0)
    {
        /* 
         * Cycle has been interrupted due to error(or EOF) before
         * any packet sent.
         * 
         * 'msg_size' contain error code.
         */

        size = msg_size;
    }

    mutex_unlock(&sender->trace_session_mutex);
    
    return size;
}

/*
 * Helper for state transition.
 * 
 * Wake up stop waiters if set 'ready' state.
 * 
 * Should be executed under lock.
 */
static void trace_sender_set_state(struct trace_sender* sender,
    enum trace_sender_state_type state)
{
    smp_wmb();
    sender->state = state;
    if(state == trace_sender_state_ready)
    {
        wake_up_all(&sender->stop_waiter);
    }
}


/* 
 * Helpers for implementing commands to the sender.
 * 
 * Should be executed with lock.
 * Correctly work with freezing state and after unfreeze.
 */

static int trace_sender_start_internal(struct trace_sender* sender,
	__be32 client_addr,
	__be16 client_port)
{
	if(sender->is_freezed)
    {
        switch(sender->deferred_commands[0])
        {
        case trace_sender_deferred_none:
            sender->deferred_commands[0] = trace_sender_deferred_start;
        break;
        case trace_sender_deferred_start:
            return -EBUSY;/* 'start' already issued */
        case trace_sender_deferred_stop:
            if(sender->deferred_commands[1] == trace_sender_deferred_start)
                return -EBUSY;/* 'start' already issued */
            sender->deferred_commands[1] = trace_sender_deferred_start;
        break;
        }
        sender->deferred_client_addr = client_addr;
        sender->deferred_client_port = client_port;
        return 1;/* Deferred execution */
    }
    else
    {
        if(sender->state == trace_sender_state_ready)
        {
            sender->client_addr = client_addr;
            sender->client_port = client_port;
                
            trace_sender_set_state(sender, trace_sender_state_starting);
            queue_work(sender->wq, &sender->work.work);
            
            return 0;
        }
    }
    return -EBUSY;
}

static void trace_sender_stop_internal(struct trace_sender* sender)
{
    if(sender->is_freezed)
    {
        switch(sender->deferred_commands[0])
        {
        case trace_sender_deferred_none:
            sender->deferred_commands[0] = trace_sender_deferred_stop;
        break;
        case trace_sender_deferred_stop:
            return;/* 'stop' already issued */
        case trace_sender_deferred_start:
            if(sender->deferred_commands[1] == trace_sender_deferred_stop)
                return;/* 'stop' already issued */
            sender->deferred_commands[1] = trace_sender_deferred_stop;
        break;
        }
        return; /* deferred execution */
    }
    else
    {
        switch(sender->state)
        {
        case trace_sender_state_starting:
            trace_sender_set_state(sender, trace_sender_state_ready);
            /* Do not cancel queued work, so it can see 'ready' state */
        break;
        case trace_sender_state_session:
            trace_sender_set_state(sender, trace_sender_state_stopping);
            queue_work(sender->wq, &sender->work.work);
        break;
        default:
        break;
        }
    }
}

/*
 * Helpers for implementing freezing and unfreezing states.
 */

/* Should be executed under lock */
static void trace_sender_freeze_state_internal(
    struct trace_sender* sender)
{
	sender->is_freezed = 1;
    /* Clear deferred commands */
    sender->deferred_commands[0] = trace_sender_deferred_none;
    sender->deferred_commands[1] = trace_sender_deferred_none;
}

static void trace_sender_unfreeze_state_internal(
    struct trace_sender* sender,
    enum trace_sender_state_type new_state)
{
    trace_sender_set_state(sender, new_state);
    sender->is_freezed = 0;

    switch(sender->deferred_commands[0])
    {
    case trace_sender_deferred_start:
        trace_sender_start_internal(sender,
            sender->deferred_client_addr, sender->deferred_client_port);
    break;
    case trace_sender_deferred_stop:
        trace_sender_stop_internal(sender);
    break;
    case trace_sender_deferred_none:
        goto out;
    }
    
    switch(sender->deferred_commands[1])
    {
    case trace_sender_deferred_start:
        trace_sender_start_internal(sender,
            sender->deferred_client_addr, sender->deferred_client_port);
    break;
    case trace_sender_deferred_stop:
        trace_sender_stop_internal(sender);
    break;
    case trace_sender_deferred_none:
        goto out;
    }
out:
    return;
}

/*
 * Work task for trace sender.
 * 
 * Implements the most part of the server-client protocol.
 */
static void trace_sender_work(struct work_struct *data)
{
	int result;
	
	unsigned long flags;

	struct trace_sender* sender = container_of(to_delayed_work(data),
		struct trace_sender, work);
	
	spin_lock_irqsave(&sender->lock, flags);

	/* Switch state under lock */
    switch(sender->state)
	{
	case trace_sender_state_starting:
        /* Freeze state*/
        trace_sender_freeze_state_internal(sender);
        spin_unlock_irqrestore(&sender->lock, flags);
        
        trace_sender_send_trace_mark(sender,
			kedr_message_type_mark_session_start);

        result = mutex_lock_interruptible(&sender->trace_session_mutex);
        if(result < 0)
        {
            pr_err("Failed to acquire mutex for starting trace sessions.");
            
            spin_lock_irqsave(&sender->lock, flags);
            trace_sender_unfreeze_state_internal(sender,
                trace_sender_state_session);
            spin_unlock_irqrestore(&sender->lock, flags);
            return;
        }

        if(sender->trace_session != NULL)
        {
            result = kedr_trace_session_start(sender->trace_session);
            if(result == 0)
            {
                queue_delayed_work(sender->wq, &sender->work,
                    sender->transmition_interval_jiff);
            }
            else
            {
                pr_err("Failed to start trace session.");
            }    
        }
        
        
   		/* 
         * Unfreeze state, perform state transition and execute
         * deferred commands.
         * 
         * It should be executed under trace lock for do not allow
         * addition of non-started trace sessions after we start others.
         */
        spin_lock_irqsave(&sender->lock, flags);
        trace_sender_unfreeze_state_internal(sender,
            trace_sender_state_session);
        spin_unlock_irqrestore(&sender->lock, flags);
        
        mutex_unlock(&sender->trace_session_mutex);
	break;
	case trace_sender_state_session:
        trace_sender_freeze_state_internal(sender);
        spin_unlock_irqrestore(&sender->lock, flags);
        
        result = trace_sender_send_trace(sender,
            sender->transmition_total_size_limit_per_interval);
        if(result > 0)
        {
            queue_delayed_work(sender->wq, &sender->work,
                sender->transmition_interval_jiff);
        }
        else switch(result)
        {
        case 0: /* no trace */
        break;
        case -EAGAIN: /* trace is empty */
            queue_delayed_work(sender->wq, &sender->work,
                sender->transmition_interval_empty_jiff);
        break;
        default:
            pr_err("Unexpected error while sending trace: %d.", result);
            /* Queue work again - may be error will be recovered */
            queue_delayed_work(sender->wq, &sender->work,
                sender->transmition_interval_jiff);
        }
   		/* Unfreeze state, perform state transition and execute deferred commands */
        spin_lock_irqsave(&sender->lock, flags);
        trace_sender_unfreeze_state_internal(sender,
            trace_sender_state_session);
        spin_unlock_irqrestore(&sender->lock, flags);
	break;
	case trace_sender_state_stopping:
        trace_sender_freeze_state_internal(sender);
        spin_unlock_irqrestore(&sender->lock, flags);
        /* Break sessions for existing traces */
        result = mutex_lock_interruptible(&sender->trace_session_mutex);
        if(result < 0)
        {
            /* Who can interrupt workqueue process? But nevertheless. */
            queue_work(sender->wq, &sender->work.work);
            /* Do not change state */
            spin_lock_irqsave(&sender->lock, flags);
            trace_sender_unfreeze_state_internal(sender,
                trace_sender_state_stopping);
            spin_unlock_irqrestore(&sender->lock, flags);
            return;
        }
        if(sender->trace_session)
        {
            kedr_trace_session_stop(sender->trace_session);
        }
        mutex_unlock(&sender->trace_session_mutex);
        
        trace_sender_send_trace_mark(sender,
			kedr_message_type_mark_session_end);

   		/* Unfreeze state, perform state transition and execute deferred commands */
        spin_lock_irqsave(&sender->lock, flags);
        trace_sender_unfreeze_state_internal(sender,
            trace_sender_state_ready);
        trace_sender_set_state(sender, trace_sender_state_ready);
        spin_unlock_irqrestore(&sender->lock, flags);
	break;

	case trace_sender_state_ready:
    	/* Execution in READY state is possible, but has no effect. */
        spin_unlock_irqrestore(&sender->lock, flags);
	break;
	default:
        pr_info("Invalid trace sender state %d.", (int)sender->state);
		spin_unlock_irqrestore(&sender->lock, flags);
	}
}


struct trace_sender* trace_sender_create(
    int transmition_interval,
    int transmition_interval_empty,
    int transmition_size_limit,
    int transmition_rate_limit)
{
	int result;
	struct trace_sender* sender;
    /* Check transmition parameters */
    
    if((transmition_interval < 0)
        || (transmition_interval_empty < 0)
        || (transmition_interval > 1000)
        || (transmition_interval_empty > 1000))
    {
        pr_err("Incorrect value of transmition intervals. Should be in [0,1000].");
        goto err;
    }
    
    if(transmition_interval_empty < transmition_interval)
    {
        pr_err("Transmition interval for empty trace shouldn't be less "
            "than one for non-emtpy trace.");
        goto err;
    }
    
    if(transmition_size_limit < 0)
    {
        pr_err("Negative value of transmition size.");
        goto err;
    }
    
    if(transmition_rate_limit < 0)
    {
        pr_err("Negative value of transmition speed.");
        goto err;
    }
    
    if(transmition_size_limit
        > transmition_interval * transmition_rate_limit)
    {
        pr_err("At least one message of size 'transmition_size_limit' "
            "should be allowed to send at every transmition_interval");
        goto err;
    }
    
    sender = kmalloc(sizeof(*sender), GFP_KERNEL);
    if(sender == NULL)
    {
        pr_err("Failed to allocate sender structure.");
        result = -ENOMEM;
        goto alloc_err;
    }

    result = sock_create(PF_INET, SOCK_DGRAM, IPPROTO_UDP,
		&sender->clientsocket);
	if(result)
	{
		pr_err("Failed to create client socket.");
        goto sock_err;
	}

	sender->wq = create_singlethread_workqueue("sendtrace"); 
	if (!sender->wq){
		pr_err("Failed to create workqueue for sending trace.");
		result = -ENOMEM;
        goto workqueue_err;
	}

    sender->state = trace_sender_state_ready;
    sender->is_freezed = 0;
    spin_lock_init(&sender->lock);
    
    sender->trace_session = NULL;
    mutex_init(&sender->trace_session_mutex);
	
	sender->seq = 0;

	INIT_DELAYED_WORK(&sender->work, &trace_sender_work);
	
	init_waitqueue_head(&sender->stop_waiter);
	
    sender->transmition_size_limit = transmition_size_limit;
    
    sender->transmition_interval_jiff =
        transmition_interval * HZ / 1000;
    sender->transmition_interval_empty_jiff =
        transmition_interval_empty * HZ / 1000;
    
    /* Kbytes/sec * ms = bytes */
    sender->transmition_total_size_limit_per_interval = 
        transmition_rate_limit * transmition_interval;
    
	return sender;

workqueue_err:
    sock_release(sender->clientsocket);
sock_err:
    kfree(sender);
alloc_err:
err:
    return NULL;
}

void trace_sender_destroy(struct trace_sender* sender)
{
	BUG_ON(sender->state != trace_sender_state_ready);
	
	/* Just in case */
    cancel_delayed_work(&sender->work);
    cancel_work_sync(&sender->work.work);
	
	flush_workqueue(sender->wq);
    destroy_workqueue(sender->wq);
	
	sock_release(sender->clientsocket);
	
	sender->state = trace_sender_state_invalid;
    kfree(sender);
}

int trace_sender_start(struct trace_sender* sender,
	__u32 client_addr,
	__u16 client_port)
{
	int result;
	unsigned long flags;
    
	spin_lock_irqsave(&sender->lock, flags);
	result = trace_sender_start_internal(sender,
        htonl(client_addr), htons(client_port));
	spin_unlock_irqrestore(&sender->lock, flags);
	
	return result;
}

void trace_sender_stop(struct trace_sender* sender)
{
	unsigned long flags;
	
	spin_lock_irqsave(&sender->lock, flags);
    trace_sender_stop_internal(sender);
	spin_unlock_irqrestore(&sender->lock, flags);
    
	return;
}

int trace_sender_wait_stop(struct trace_sender* sender)
{
	int result = wait_event_killable(sender->stop_waiter,
        sender->state == trace_sender_state_ready);
    /* Barrier after reading shared state */
    if(result == 0)
    {
        smp_rmb();
        flush_work(&sender->work.work);
    }
    
    return result;
}

int trace_sender_add_event_collector(struct trace_sender* sender,
    struct execution_event_collector* event_collector)
{
    int result = 0;
    
    unsigned long flags;
    int is_session;
    
    struct kedr_trace_session* trace_session =
        kmalloc(sizeof(*trace_session), GFP_KERNEL);
    if(trace_session == NULL)
    {
        pr_err("Failed to allocate trace session structure.");
        return -ENOMEM;
    }
    
    result = kedr_trace_session_init(trace_session, event_collector);
    if(result < 0)
    {
        kfree(trace_session);
        return result;
    }
    
    result = mutex_lock_interruptible(&sender->trace_session_mutex);
    if(result < 0)
    {
        kedr_trace_session_destroy(trace_session);
        kfree(trace_session);
        return result;
    }
    
    if(sender->trace_session)
    {
        pr_err("Only one event collector may be processed at a time.");
        result = -EBUSY;
        goto out;
    }

    spin_lock_irqsave(&sender->lock, flags);
    
    is_session = sender->state == trace_sender_state_session;
    
    spin_unlock_irqrestore(&sender->lock, flags);

    if(is_session)
    {
        queue_work(sender->wq, &sender->work.work);
        kedr_trace_session_start(trace_session);
    }

    sender->trace_session = trace_session;

    event_collector->private_data = sender;
out:
    
    mutex_unlock(&sender->trace_session_mutex);
    
    if(result < 0)
    {
        kedr_trace_session_destroy(trace_session);
        kfree(trace_session);
    }
    return result;
}

/* NOTE: sender object is derived from private data of event collector */
int trace_sender_remove_event_collector(
    struct execution_event_collector* event_collector)
{
    int result;
    
    struct trace_sender* sender = 
        (struct trace_sender*)event_collector->private_data;

    struct kedr_trace_session* trace_session = sender->trace_session;
    
    kedr_trace_session_terminate(trace_session);
    result = kedr_trace_session_wait_stop(trace_session);
    if(result < 0)
    {
        pr_err("Failed to wait until trace session stop. Do not remove it.");
        return result;
    }

    result = mutex_lock_killable(&sender->trace_session_mutex);
    if(result < 0) return result;
    
    /* Clear field with trace session if needed */
    if(sender->trace_session == trace_session)
        sender->trace_session = NULL;
   
    mutex_unlock(&sender->trace_session_mutex);

    kedr_trace_session_destroy(trace_session);
    kfree(trace_session);
    
    return 0;
}

int trace_sender_get_session_info(struct trace_sender* sender,
    __u32* client_addr, __u16* client_port)
{
    int result;
    unsigned long flags;
    
    spin_lock_irqsave(&sender->lock, flags);
    switch(sender->state)
    {
    case trace_sender_state_starting:
    case trace_sender_state_session:
    case trace_sender_state_stopping:
        *client_addr = ntohl(sender->client_addr);
        *client_port = ntohs(sender->client_port);
        result = 0;
    break;
    case trace_sender_state_ready:
        result = -ENODEV;
    break;
    default: /* invalid state*/
        result = -EINVAL;
    }
    spin_unlock_irqrestore(&sender->lock, flags);
    
    return result;
}