/* Module for control sending of the trace */

/* implements 'kedr_event_handler' */
#include "kedr/kedr_mem/core_api.h" 

#include "udp_packet_definition.h"

#include <linux/module.h>
#include <linux/init.h>

#include <net/sock.h>
#include <linux/skbuff.h>
#include <linux/delay.h>
#include <linux/inet.h>

#include <linux/wait.h>
#include <linux/sched.h>

#include <linux/in.h>
#include <linux/ip.h>
#include <linux/udp.h>

#include <linux/moduleparam.h>

#include "trace_sender.h"

#include "config.h"
/*
 * Transmition size limit, in bytes.
 *
 * Restriction in size of one packet.
 *
 * Usefull for satisfy network requirements and for receive packets
 * in user space.
 */
#define TRANSMITION_SIZE_LIMIT 1300

/*
 * Transmittion speed limit, in Kbytes/sec.
 *
 * Restriction in total size of the packets, sent by the server per
 * time unit.
 *
 * Usefull for not overload network or system.
 */
#define TRANSMITION_SPEED_LIMIT 200

/*
 * Interval between initiating sending of trace packets, in ms.
 *
 * NOTE: messages with trace marks may ignore this interval.
 */
#define SENDER_WORK_INTERVAL 100

/*
 * Sensitivity of the trace sender for new trace events, in ms.
 *
 * Interval between new event arrival and sending it
 * if no other limits.
 */
#define SENDER_SENSITIVITY 1000

//TODO: module parameters
#define BUFFER_NORMAL_SIZE 1000000
#define BUFFER_CRITICAL_SIZE 10000000

/* Port of the server */
unsigned short server_port = TRACE_SERVER_PORT;
module_param(server_port, ushort, S_IRUGO);

/* Parameters affected on trace transmition rate. */
int transmition_size_limit = TRANSMITION_SIZE_LIMIT;
module_param(transmition_size_limit, int, S_IRUGO);

int transmition_speed_limit = TRANSMITION_SPEED_LIMIT;
module_param(transmition_speed_limit, int, S_IRUGO);

int sender_work_interval = SENDER_WORK_INTERVAL;
module_param(sender_work_interval, int, S_IRUGO);

int sender_sensetivity = SENDER_SENSITIVITY;
module_param(sender_sensetivity, int, S_IRUGO);

/* Parameters affected on kernel-space capacity for collect trace(before sending) */
unsigned int buffer_normal_size = BUFFER_NORMAL_SIZE;
module_param(buffer_normal_size, uint, S_IRUGO);

unsigned int buffer_critical_size = BUFFER_CRITICAL_SIZE;
module_param(buffer_critical_size, uint, S_IRUGO);
/********************Inet address as module parameter********************/
/*
 * Address which may be used as ending point in IP connection(e.g., UDP).
 *
 * It is written as "127.0.0.1: 5000".
 */

/*
 * String describing absence of address.
 */
const char net_addr_none_str[] = "none";

/* Callbacks for do real work */
struct net_addr_control
{
    int (*set_addr)(struct net_addr_control* control,
        __u32 addr, __u16 port);
    /* Called when 'none' is written to the param */
    int (*clear_addr)(struct net_addr_control* control);
    /* Should return 1 if address is not set */
    int (*get_addr)(struct net_addr_control* control,
        __u32 *addr, __u16 *port);
};



static int
kernel_param_net_ops_set(const char *val,
#if defined(MODULE_PARAM_CREATE_USE_OPS_STRUCT)
    const struct kernel_param *kp
#elif defined(MODULE_PARAM_CREATE_USE_OPS)
    struct kernel_param *kp
#else
#error Unknown way to create module parameter with callbacks
#endif
)
{
    struct net_addr_control* control = kp->arg;
    unsigned int addr1, addr2, addr3, addr4;
    unsigned int port;
    int n;
    __u32 addr;

    if(strncmp(val, net_addr_none_str, sizeof(net_addr_none_str) - 1) == 0)
    {
        return control->clear_addr ? control->clear_addr(control) : -EINVAL;
    }

    n = sscanf(val, "%u.%u.%u.%u: %u",
        &addr1, &addr2, &addr3, &addr4, &port);
    if(n != 5) return -EINVAL;


    if((addr1 > 255) || (addr2 > 255) || (addr3 > 255) || (addr4 > 255)
        || (port > 0xffff))
    {
        return -EINVAL;
    }

    addr = (addr1 << 24) | (addr2 << 16) | (addr3 << 8) | addr4;
    return control->set_addr
        ? control->set_addr(control, (__u32)addr, (__u16)port)
        : -EINVAL;
}

static int
kernel_param_net_ops_get(char *buffer,
#if defined(MODULE_PARAM_CREATE_USE_OPS_STRUCT)
    const struct kernel_param *kp
#elif defined(MODULE_PARAM_CREATE_USE_OPS)
    struct kernel_param *kp
#else
#error Unknown way to create module parameter with callbacks
#endif
)
{
    struct net_addr_control* control = kp->arg;
    __u32 addr;
    __u16 port;
    int result = control->get_addr
        ? control->get_addr(control, &addr, &port)
        : -EINVAL;

    if(result < 0) return result;

    if(result == 1)
        return sprintf(buffer, "%s", net_addr_none_str);
    else
        return sprintf(buffer, "%u.%u.%u.%u: %u",
            (unsigned)(addr >> 24), (unsigned)(addr >> 16) & 0xff,
            (unsigned)(addr >> 8) & 0xff, (unsigned)addr & 0xff,
            (unsigned)port);
}

#if defined(MODULE_PARAM_CREATE_USE_OPS_STRUCT)
struct kernel_param_ops kernel_param_net_ops =
{
    .set = kernel_param_net_ops_set,
    .get = kernel_param_net_ops_get
};

#define module_param_net_named(name, control, perm) \
module_param_cb(name, &kernel_param_net_ops, control, perm)

#elif defined(MODULE_PARAM_CREATE_USE_OPS)
#define module_param_net_named(name, control, perm) \
module_param_call(name, \
    kernel_param_net_ops_set, kernel_param_net_ops_get, control, perm)
#else
#error Unknown way to create module parameter with callbacks
#endif

//***********************Protocol implementation**********************//
struct port_listener
{
    struct socket *udpsocket;
    /* Object which implements commands recieved by listener */
    struct trace_sender* sender;
};

static void port_listener_cb_data(struct sock* sk, int bytes)
{
    struct port_listener* listener = (struct port_listener*)sk->sk_user_data;

    __be32 sender_addr;
    __be16 sender_port;
    struct kedr_message_header* msg;
    int msg_len;

    struct sk_buff *skb = NULL;

    skb = skb_dequeue(&sk->sk_receive_queue);
    if(skb == NULL)
    {
        pr_info("Failed to extract recieved skb.");
        return;
    }

    if(ip_hdr(skb)->protocol != IPPROTO_UDP)
    {
        pr_info("Ignore non-UDP packets.");
        goto out;
    }
    sender_addr = ip_hdr(skb)->saddr;
    sender_port = udp_hdr(skb)->source;

    msg = (typeof(msg))(skb->data + sizeof(struct udphdr));
    msg_len = skb->len - sizeof(struct udphdr);

    if(msg_len < kedr_message_header_size)
    {
        pr_info("Ignore request with incorrect length(%d).", msg_len);
        goto out;
    }

    if(msg->magic != htonl(KEDR_MESSAGE_HEADER_MAGIC))
    {
        pr_info("Ignore udp packets with incorrect magic field.");
        goto out;
    }
    
    switch((enum kedr_message_command_type)msg->type)
    {
    case kedr_message_command_type_start:
        trace_sender_start(listener->sender,
            ntohl(sender_addr), ntohs(sender_port));
    break;
    case kedr_message_command_type_stop:
        trace_sender_stop(listener->sender);
    break;
    default:
        pr_info("Ignore incorrect request of type %d.", (int)msg->type);
        goto out;
    }
out:
    kfree_skb(skb);
}

static int port_listener_init(struct port_listener* listener,
    int16_t port, struct trace_sender* sender)
{
    struct sockaddr_in server;
    int result;

    result = sock_create(PF_INET, SOCK_DGRAM, IPPROTO_UDP,
        &listener->udpsocket);

    if (result < 0) {
        pr_err("server: Error creating udpsocket.");
        return result;
    }

    server.sin_family = AF_INET;
    server.sin_addr.s_addr = INADDR_ANY;
    server.sin_port = htons(port);

    result = kernel_bind(listener->udpsocket,
        (struct sockaddr*)&server, sizeof(server));
    if (result)
    {
        pr_err("Failed to bind server socket.");
        sock_release(listener->udpsocket);
        return result;
    }

    listener->sender = sender;
    listener->udpsocket->sk->sk_user_data = listener;
    /* Barrier before publish callback for recieving messages */
    smp_wmb();
    listener->udpsocket->sk->sk_data_ready = port_listener_cb_data;

    return 0;

}

static void port_listener_destroy(struct port_listener* listener)
{
    sock_release(listener->udpsocket);
}

/* Concrete objects for module */
static struct trace_sender* sender;
static struct port_listener listener;

struct execution_event_collector* current_collector = NULL;
EXPORT_SYMBOL(current_collector);

/* Callbacks for KEDR CORE module */
static void sender_on_target_loaded(struct kedr_event_handlers *eh, 
    struct module *target_module)
{
    current_collector = trace_sender_collect_messages(
        sender, target_module,
        buffer_normal_size, buffer_critical_size);
}

static void sender_on_target_about_to_unload(
    struct kedr_event_handlers *eh, struct module *target_module)
{
    if(current_collector)
    {
        trace_sender_stop_collect_messages(sender, target_module);
        current_collector = NULL;
    }
}


static void sender_on_function_entry(struct kedr_event_handlers *eh, 
    unsigned long tid, unsigned long func)
{
    record_function_entry(tid, func);
}

static void sender_on_function_exit(struct kedr_event_handlers *eh, 
    unsigned long tid, unsigned long func)
{
    record_function_exit(tid, func);
}

static void sender_on_call_pre(struct kedr_event_handlers *eh, 
    unsigned long tid, unsigned long pc, unsigned long func)
{
    record_function_call_pre(tid, pc, func);
}

static void sender_on_call_post(struct kedr_event_handlers *eh, 
    unsigned long tid, unsigned long pc, unsigned long func)
{
    record_function_call_post(tid, pc, func);
}

static void sender_begin_memory_events(
    struct kedr_event_handlers *eh, 
    unsigned long tid, unsigned long num_events, 
    void **pdata /* out param*/)
{
    record_memory_accesses_begin(tid, (int)num_events, pdata);
}

static void sender_end_memory_events(
    struct kedr_event_handlers *eh, 
    unsigned long tid, void *data)
{
    record_memory_accesses_end(data);
}

static void sender_on_memory_event(
    struct kedr_event_handlers *eh, 
    unsigned long tid, 
    unsigned long pc, unsigned long addr, unsigned long size, 
    enum kedr_memory_event_type memory_event_type,
    void *data)
{
    record_memory_access_next(data, pc, addr, size,
        (unsigned char)memory_event_type);
}

static void sender_on_locked_op_post(struct kedr_event_handlers *eh, 
    unsigned long tid, unsigned long pc, 
    unsigned long addr, unsigned long size, 
    enum kedr_memory_event_type type, void *data)
{
    record_locked_memory_access(tid, pc, addr, size, type);
}


static void sender_on_io_mem_op_post(struct kedr_event_handlers *eh, 
    unsigned long tid, unsigned long pc, 
    unsigned long addr, unsigned long size, 
    enum kedr_memory_event_type type, void *data)
{
    record_io_memory_access(tid, pc, addr, size, type);
}

/*
 * Record information about barrier after operation,
 * which don't access memory.
 */
static void sender_on_memory_barrier_post(struct kedr_event_handlers *eh, 
    unsigned long tid, unsigned long pc, 
    enum kedr_barrier_type type)
{
    record_memory_barrier(tid, pc, type);
}

static void sender_on_alloc_post(struct kedr_event_handlers *eh, 
        unsigned long tid, unsigned long pc, 
        unsigned long size, unsigned long addr)
{
    record_alloc(tid, pc, size, addr);
}

static void sender_on_free_pre(struct kedr_event_handlers *eh, 
    unsigned long tid, unsigned long pc, 
    unsigned long addr)
{
    record_free(tid, pc, addr);
}

static void sender_on_lock_post(struct kedr_event_handlers *eh, 
    unsigned long tid, unsigned long pc, 
    unsigned long lock_id, enum kedr_lock_type type)
{
    record_lock(tid, pc, lock_id, type);
}

static void sender_on_unlock_pre(struct kedr_event_handlers *eh, 
    unsigned long tid, unsigned long pc, 
    unsigned long lock_id, enum kedr_lock_type type)
{
    record_unlock(tid, pc, lock_id, type);
}

static void sender_on_signal_pre(struct kedr_event_handlers *eh, 
        unsigned long tid, unsigned long pc, 
        unsigned long obj_id, enum kedr_sw_object_type type)
{
    record_signal(tid, pc, obj_id, type);
}

static void sender_on_wait_post(struct kedr_event_handlers *eh, 
    unsigned long tid, unsigned long pc, 
    unsigned long obj_id, enum kedr_sw_object_type type)
{
    record_wait(tid, pc, obj_id, type);
}

static void sender_on_thread_create_pre(struct kedr_event_handlers *eh, 
        unsigned long tid, unsigned long pc)
{
    record_thread_create_before(tid, pc);
}

static void sender_on_thread_create_post(struct kedr_event_handlers *eh, 
        unsigned long tid, unsigned long pc, 
        tid_t child_tid)
{
    record_thread_create_after(tid, pc, child_tid);
}


static void sender_on_thread_join_post(struct kedr_event_handlers *eh,
        unsigned long tid, unsigned long pc, 
        unsigned long child_tid)
{
    record_thread_join(tid, pc, child_tid);
}

static struct kedr_event_handlers sender_event_handlers =
{
    .owner = THIS_MODULE,
    .on_target_loaded =             sender_on_target_loaded,
    .on_target_about_to_unload =    sender_on_target_about_to_unload,
    
    .on_function_entry =            sender_on_function_entry,
    .on_function_exit =             sender_on_function_exit,
    
    .on_call_pre =                  sender_on_call_pre,
    .on_call_post =                 sender_on_call_post,
    
    .begin_memory_events =          sender_begin_memory_events,
    .end_memory_events =            sender_end_memory_events,
    .on_memory_event =              sender_on_memory_event,
    
    .on_locked_op_post =            sender_on_locked_op_post,
    
    .on_io_mem_op_post =            sender_on_io_mem_op_post,
    
    .on_memory_barrier_pre =        sender_on_memory_barrier_post,
    
    .on_alloc_post =                sender_on_alloc_post,
    .on_free_pre =                  sender_on_free_pre,
    
    .on_lock_post =                 sender_on_lock_post,
    .on_unlock_pre =                sender_on_unlock_pre,
    
    .on_signal_pre =                sender_on_signal_pre,
    .on_wait_post =                 sender_on_wait_post,
    
    .on_thread_create_pre =         sender_on_thread_create_pre,
    .on_thread_create_post =        sender_on_thread_create_post,
    .on_thread_join_post =          sender_on_thread_join_post,
};

/* Client address as module parameter */

/* This values are used only while set parameters while module is being
 * initialized.
 * After trace sender is created, setting and reading values are
 * performed via sender's functions.
 */
int client_is_set = 0;
__u32 client_addr = 0;
__u16 client_port = 0;

/* volatile flag */
int sender_initialized_flag = 0;
static void set_sender_initialized(void)
{
    smp_wmb();
    sender_initialized_flag = 1;
}
static int is_sender_initialized(void)
{
    int result = sender_initialized_flag;
    smp_rmb();
    return result;
}

/* Module parameters callbacks */
static int client_ops_set_addr(struct net_addr_control* control,
    __u32 addr, __u16 port)
{
    if(is_sender_initialized())
    {
        return trace_sender_start(sender, addr, port);
    }
    else
    {
        if(client_is_set) return -EBUSY;
        client_addr = addr;
        client_port = port;
        client_is_set = 1;
        return 0;
    }
}

static int client_ops_clear_addr(struct net_addr_control* control)
{
    if(is_sender_initialized())
    {
        trace_sender_stop(sender);
        return 0;
    }
    else
    {
        if(client_is_set) client_is_set = 0;
        return 0;
    }
}

static int client_ops_get_addr(struct net_addr_control* control,
    __u32 *addr, __u16 *port)
{
    if(is_sender_initialized())
    {
        int result = trace_sender_get_session_info(sender, addr, port);
        if(result == -ENODEV) return 1;
        return result;
    }
    else
    {
        if(!client_is_set) return 1;
        *addr = client_addr;
        *port = client_port;
        return 0;
    }
}

struct net_addr_control client_ops =
{
    .set_addr = client_ops_set_addr,
    .get_addr = client_ops_get_addr,
    .clear_addr = client_ops_clear_addr
};

module_param_net_named(client_addr, &client_ops, S_IRUGO | S_IWUSR);

static int __init server_init( void )
{
    int result;

    if((buffer_normal_size <= 0) || (buffer_critical_size <= 0))
    {
        pr_err("Sizes of buffers for trace should be positive.");
        return -EINVAL;
    }

    sender = trace_sender_create(
        sender_work_interval,
        sender_sensetivity,
        transmition_size_limit,
        transmition_speed_limit);
    if(sender == NULL)
    {
        result = -EINVAL;
        goto sender_err;
    }

    /* Data race, but nothing bad*/
    if(client_is_set)
    {
        result = trace_sender_start(sender, client_addr, client_port);
        if(result < 0) goto sender_start_err;
        //pr_info("Trace sender is initialized with client set.");
    }
    set_sender_initialized();

    result = kedr_register_event_handlers(&sender_event_handlers);
    if(result) goto event_handler_err;

    result = port_listener_init(&listener, server_port, sender);
    if(result) goto listener_err;

    return 0;

listener_err:
    kedr_unregister_event_handlers(&sender_event_handlers);
event_handler_err:
    trace_sender_stop(sender);
    trace_sender_wait_stop(sender);
sender_start_err:
    trace_sender_destroy(sender);
sender_err:
    return result;
}

static void __exit server_exit( void )
{
    port_listener_destroy(&listener);

    kedr_unregister_event_handlers(&sender_event_handlers);

    trace_sender_stop(sender);
    if(trace_sender_wait_stop(sender) == 0);
        trace_sender_destroy(sender);
}

module_init(server_init);
module_exit(server_exit);
MODULE_LICENSE("GPL");
