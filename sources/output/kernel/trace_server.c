/* Module for control sending of the trace */

#include "trace_definition.h"

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
#define TRANSMITION_SPEED_LIMIT 300

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

/* Port of the server */
unsigned short server_port = TRACE_SERVER_PORT;
module_param(server_port, ushort, S_IRUGO);

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
	struct kedr_trace_sender_command* msg;
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
	
	if(msg_len < sizeof(*msg))
	{
		pr_info("Ignore incorrect request.");
		goto out;
	}
	
	switch(msg->type)
	{
	case kedr_trace_sender_command_type_start:
		trace_sender_start(listener->sender,
            ntohl(sender_addr), ntohs(sender_port));
	break;
	case kedr_trace_sender_command_type_stop:
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

/* Callbacks for create/delete events collector */
static int collector_start(struct execution_event_collector* collector)
{
    return trace_sender_add_event_collector(sender, collector);
}
static int collector_stop(struct execution_event_collector* collector)
{
    return trace_sender_remove_event_collector(collector);
}

static struct execution_event_handler event_handler =
{
    .owner = THIS_MODULE,
    .start = collector_start,
    .stop = collector_stop
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
	
	sender = trace_sender_create(
        SENDER_WORK_INTERVAL,
        SENDER_SENSITIVITY,
        TRANSMITION_SIZE_LIMIT,
        TRANSMITION_SPEED_LIMIT);
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
    
    result = execution_event_set_handler(&event_handler);
    if(result) goto event_handler_err;
	
	result = port_listener_init(&listener, server_port, sender);
	if(result) goto listener_err;
	
	return 0;

listener_err:
    execution_event_unset_handler(&event_handler);
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
    
    execution_event_unset_handler(&event_handler);

	trace_sender_stop(sender);
	if(trace_sender_wait_stop(sender) == 0);
        trace_sender_destroy(sender);
}

module_init(server_init);
module_exit(server_exit);
MODULE_LICENSE("GPL");
