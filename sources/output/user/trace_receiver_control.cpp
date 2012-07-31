/* Executable which may control trace receiver. */

#include "udp_packet_definition.h"

#include <kedr/utils/template_parser.h> /* Usage in template form */

#include "trace_receiver.h"

#include <string>

#include <sys/types.h> /* types sendmsg */
#include <sys/socket.h> /* socket operations */
#include <netinet/in.h> /* struct sockaddr_in */
#include <arpa/inet.h> /* inet_addr() */
#include <stddef.h> /* offsetof */

#include <sstream> /* streams from string */
#include <iostream> /* cerr */

#include <stdexcept> /* invalid_argument and other exceptions */
#include <cassert> /* assert() macro */

#include <getopt.h> /* options parsing */

#include <unistd.h> /* fork, exec, close */
#include <cstdlib> /* exit */
#include <sys/wait.h> /* waitpid */

#include "config.h" /* Path to the trace receiver program */

#include <vector>

#include <stdio.h> /* snprintf */
#include <cstdlib> /* malloc, free */
#include <algorithm> /* for_each */
#include <iostream> /* cerr */

#include <signal.h> /* signals processing */

#include <poll.h> /* poll() */
#include <errno.h>

/* Default port of the receiver */
#ifndef TRACE_RECEIVER_PORT
#define TRACE_RECEIVER_PORT 9999
#endif

/* Default port of the control program */
#ifndef CONTROL_PORT
#define CONTROL_PORT 8888
#endif

/* Period (in seconds) of keep-alive messages to the trace receiver. */
#define KEEP_ALIVE_PERIOD 3

class TraceReceiverControl
{
public:
    TraceReceiverControl(uint16_t controlPort_native,
        uint16_t receiverPort_native);
    ~TraceReceiverControl(void);

    uint16_t getRecieverPort_native(void) const
        { return ntohs(receiverAddr.sin_port);}

    /* 
     * Send message to the receiver.
     * 
     * Return 0 on success negative value on fail.
     */
    int sendControlMessage(enum kedr_message_control_type type,
        const void* data = NULL, int dataSize = 0);
    
    /*
     * Receive message from trace receiver.
     * 
     * Return 0 on success, negative error if fail to receive anything.
     * Return 1 if received message doesn't fit into needed format
     * (has incorrect header or doesn't enough data).
     */
    int recvControlMessage(enum kedr_message_info_type* type,
        void* data = NULL, int dataSize = 0);

    /* 
     * Prepare for waiting when trace receiver is initialized.
     * 
     * Return 0 on success, -1 on fail.
     */
    int waitForInitializedPrepare(void);
    /*
     * Cancel preparation.
     * 
     * Should be called in any case.
     */
    void waitForInitializedCancel(void);
    
    /* 
     * Wait while trace receiver is initialized.
     * 
     * Return 0 on success.
     * Return -1 on fail.
     * Return 1 if trace receiver failed to initialized.
     */
    int waitForInitialized(void);

    /* 
     * Prepare for waiting when trace receiver is finalized.
     * 
     * Return 0 on success, -1 on fail.
     */
    int waitForFinalizedPrepare(void);
    /*
     * Cancel preparation.
     * 
     * Should be called in any case.
     */
    void waitForFinalizedCancel(void);
    
    /* 
     * Wait while trace receiver is initialized.
     * 
     * Return 0 on success.
     * Return -1 on fail.
     * Return 1 if trace receiver failed to initialized.
     */
    int waitForFinalized(void);
    /* Similar to destructor. May be used after fork. */
    void finalize(void);
    
private:
    struct sockaddr_in controlAddr;
    struct sockaddr_in receiverAddr;
    
    int sock;
    /* Connect socket, if it is not connected */
    int sockConnect(void);
    /* Disconnect socket, if if is connected. */
    void sockDisconnect(void);
    
    bool isSockConnected;

    static bool isReceiverTerminated;
    static void onReceiverTerminated(int) {isReceiverTerminated = true;}
    static bool isReceiverInitialized;
    static void onReceiverInitialized(int) {isReceiverInitialized = true;}
    static void onReceiverUninitialized(int) {isReceiverInitialized = false;}
    
    /* 
     * Same as recvControlMessage, but do not follow keep-alive semantic.
     * 
     * Should be used when message definitely exists (e.g., after select).
     */
    int _recvControlMessage(enum kedr_message_info_type* type,
        void* data = NULL, int dataSize = 0);
};

/* One action of the control program */
class ControlAction
{
public:
    virtual ~ControlAction(void) {}
    
    virtual int doAction(TraceReceiverControl& control) = 0;
};

/* Parameters of the progam */
class ControlParams
{
public: /* Fields are public for simplification */
    uint16_t controlPort;
    uint16_t receiverPort;
    
    std::vector<ControlAction*> actions;
public:    
    /* Initialize with default parameters */
    ControlParams(void);
    ~ControlParams(void);
    
    int parseParameters(int argc, char** argv);
};



int main(int argc, char** argv)
{
    ControlParams params;
    
    int result = params.parseParameters(argc, argv);
    if(result != 0) return result;
    
    TraceReceiverControl traceReceiverControl(
        params.controlPort,    params.receiverPort);
    
    for(int i = 0; i < (int)params.actions.size(); i++)
    {
        //std::cerr << "Before " <<i + 1 << " action\n";
        result = params.actions[i]->doAction(traceReceiverControl);
        //std::cerr << i + 1 << "-th action has been performed\n";
        if(result) return result;
    }
    //std::cerr << "123\n";
    
    return 0;
}

/*************************** Implementation ***************************/
/* Parameters */
ControlParams::ControlParams(void):
    controlPort(CONTROL_PORT), receiverPort(TRACE_RECEIVER_PORT)
{
}

static inline void deleteAction(ControlAction* action) {delete action;}
ControlParams::~ControlParams(void)
{
    std::for_each(actions.begin(), actions.end(), deleteAction);
    actions.clear();
}

/* Different actions */
class ControlActionStart: public ControlAction
{
public:
    ControlActionStart(const char* receiverPath,
        const std::string& traceDirectoryFormat);
    
    int doAction(TraceReceiverControl& control);
private:
    const char* receiverPath;
    const std::string traceDirectoryFormat;
};

ControlActionStart::ControlActionStart(const char* receiverPath,
    const std::string& traceDirectoryFormat):
    receiverPath(receiverPath), traceDirectoryFormat(traceDirectoryFormat)
{
}

int ControlActionStart::doAction(TraceReceiverControl& control)
{
    //std::cerr << "Starting (store trace to '"
    //    << traceDirectoryFormat << "' )...\n";
    /* Prepare signals processing */
    if(control.waitForInitializedPrepare() != 0) return -1;
    
    /* fork + exec */
    pid_t pid = fork();
    if(pid == -1)
    {
        std::cerr << "Fork failed.\n";
        control.waitForInitializedCancel();
        return -1;
    }
    else if(pid == 0)
    {
        /* Child */
        control.waitForInitializedCancel();
        control.finalize();
        
        char receiverPortStr[10];
        snprintf(receiverPortStr, sizeof(receiverPortStr), "%d",
            (int)control.getRecieverPort_native());

        /* TODO: remove fixed size. */
        char traceDirectoryFormatStr[256];
        snprintf(traceDirectoryFormatStr, sizeof(traceDirectoryFormatStr),
            "%s", traceDirectoryFormat.c_str());
        
        /* TODO: remove fixed size. */
        char receiverPathStr[256];
        snprintf(receiverPathStr, sizeof(receiverPathStr),
            "%s", receiverPath);

        char* args[4];
        args[0] = receiverPathStr;
        args[1] = receiverPortStr;
        args[2] = traceDirectoryFormatStr;
        args[3] = NULL;
        
        execv(receiverPath, args);
        std::cerr << "Failed to run trace receiver at '"
         << receiverPath << "'.\n";
        exit(1);
    }
    /* Parent */
    int result = control.waitForInitialized();
    control.waitForInitializedCancel();
    if(result == 0)
    {
        //std::cerr << "Trace receiver has been started.\n";
        return 0;
    }
    else if(result == 1)
    {
        std::cerr << "Trace receiver failed to start.\n";
        return -1;
    }
    else
    {
        kill(pid, SIGABRT);
        return -1;
    }
}

class ControlActionStop: public ControlAction
{
public:
    int doAction(TraceReceiverControl& control);
};

int ControlActionStop::doAction(TraceReceiverControl& control)
{
    int result = control.waitForFinalizedPrepare();
    if(result != 0) return result;
    
    pid_t pid = getpid();
    result = control.sendControlMessage(
        kedr_message_control_type_wait_terminate, &pid, sizeof(pid));
    if(result != 0)
    {
        std::cerr << "Failed to send 'wait_terminate' control message.\n";
        goto err;
    }

    result = control.sendControlMessage(
        kedr_message_control_type_terminate);
    if(result != 0)
    {
        std::cerr << "Failed to send 'terminate' control message.\n";
        goto err;
    }

    result = control.waitForFinalized();

    control.waitForFinalizedCancel();
    
    return result;
    
err:
    control.waitForFinalizedCancel();
    return -1;
}

class ControlActionInitSession: public ControlAction
{
public:
    ControlActionInitSession(const struct sockaddr_in* serverAddr);
    
    int doAction(TraceReceiverControl& control);
private:
    struct sockaddr_in serverAddr;
};

ControlActionInitSession::ControlActionInitSession(
    const struct sockaddr_in* serverAddr)
{
    memcpy(&this->serverAddr, serverAddr, sizeof(*serverAddr));
}

int ControlActionInitSession::doAction(TraceReceiverControl& control)
{
    int result = control.sendControlMessage(
        kedr_message_control_type_wait_init_connection);

    if(result != 0)
    {
        std::cerr << "Failed to send 'wait_init_connection' "
            << "control message.\n";
        return result;
    }

    result = control.sendControlMessage(
        kedr_message_control_type_init_connection,
        &serverAddr, sizeof(serverAddr));
    
    if(result != 0)
    {
        std::cerr << "Failed to send 'init_connection' control message.\n";
        return result;
    }
   
    enum kedr_message_info_type recvType;
    result = control.recvControlMessage(&recvType);
    if(result != 0)
    {
        std::cerr << "Failed to receive answer "
            << "for 'wait_init_connection' control message.\n";
        return result;
    }
    switch(recvType)
    {
    case kedr_message_info_type_start_connection:
    break;
    default:
        std::cerr << "Failed to wait while connection is initialized.\n";
        return 1;
    }
    
    return 0;
}

class ControlActionBreakSession: public ControlAction
{
public:
    ControlActionBreakSession(const struct sockaddr_in* serverAddr);
    
    int doAction(TraceReceiverControl& control);
private:
    struct sockaddr_in serverAddr;
};

ControlActionBreakSession::ControlActionBreakSession(
    const struct sockaddr_in* serverAddr)
{
    memcpy(&this->serverAddr, serverAddr, sizeof(*serverAddr));
}

int ControlActionBreakSession::doAction(TraceReceiverControl& control)
{
    int result = control.sendControlMessage(
        kedr_message_control_type_wait_break_connection);

    if(result != 0)
    {
        std::cerr << "Failed to send 'wait_break_connection' "
            << "control message.\n";
        return result;
    }

    result = control.sendControlMessage(
        kedr_message_control_type_break_connection,
        &serverAddr, sizeof(serverAddr));
    
    if(result != 0)
    {
        std::cerr << "Failed to send 'break_connection' control message.\n";
        return result;
    }
    
    enum kedr_message_info_type recvType;
    result = control.recvControlMessage(&recvType);
    if(result != 0)
    {
        std::cerr << "Failed to receive answer "
            << "for 'wait_break_connection' control message.\n";
        return result;
    }
    switch(recvType)
    {
    case kedr_message_info_type_stop_connection:
    break;
    default:
        std::cerr << "Failed to wait while connection is broken.\n";
        return 1;
    }
    
    return 0;
}

class ControlActionStartTrace: public ControlAction
{
public:
    int doAction(TraceReceiverControl& control);
};

int ControlActionStartTrace::doAction(TraceReceiverControl& control)
{
    int result = control.sendControlMessage(
        kedr_message_control_type_wait_trace_begin);

    if(result != 0)
    {
        std::cerr << "Failed to send 'wait_trace_begin' "
            << "control message.\n";
        return result;
    }
    
    enum kedr_message_info_type recvType;
    result = control.recvControlMessage(&recvType);
    if(result != 0)
    {
        std::cerr << "Failed to receive answer "
            << "for 'wait_trace_begin' control message.\n";
        return result;
    }
    switch(recvType)
    {
    case kedr_message_info_type_start_trace:
    break;
    default:
        std::cerr << "Failed to wait while trace is started.\n";
        return 1;
    }
    
    return 0;
}

class ControlActionEndTrace: public ControlAction
{
public:
    int doAction(TraceReceiverControl& control);
};

int ControlActionEndTrace::doAction(TraceReceiverControl& control)
{
    int result = control.sendControlMessage(
        kedr_message_control_type_wait_trace_end);

    if(result != 0)
    {
        std::cerr << "Failed to send 'wait_trace_end' "
            << "control message.\n";
        return result;
    }
    
    enum kedr_message_info_type recvType;
    result = control.recvControlMessage(&recvType);
    if(result != 0)
    {
        std::cerr << "Failed to receive answer "
            << "for 'wait_trace_end' control message.\n";
        return result;
    }
    switch(recvType)
    {
    case kedr_message_info_type_stop_trace:
    break;
    default:
        std::cerr << "Failed to wait while trace is ended.\n";
        return 1;
    }
    
    return 0;
}



static int print_command(char* buf, int buf_size, void*)
{
    return snprintf(buf, buf_size, "%s", "kedr_save_trace");//TODO: configure command
}

static int print_server_port(char* buf, int buf_size, void*)
{
    return snprintf(buf, buf_size, "%d", TRACE_SERVER_PORT);
}

static int print_receiver_port(char* buf, int buf_size, void*)
{
    return snprintf(buf, buf_size, "%d", TRACE_RECEIVER_PORT);
}


extern char _binary_trace_receiver_control_usage_start[];
extern char _binary_trace_receiver_control_usage_end[];

static void print_usage(void)
{
    static struct param_spec param_specs[] =
    {
        {"command", print_command},
        {"SERVER_PORT", print_server_port},
        {"RECEIVER_PORT", print_receiver_port},
    };
    
    struct template_parser usage_template_parser;
    template_parser_init(&usage_template_parser,
        _binary_trace_receiver_control_usage_start,
        _binary_trace_receiver_control_usage_end,
        param_specs,
        sizeof(param_specs) / sizeof(param_specs[0]),
        NULL);
    
    const char* chunk;
    int chunk_size;
    
    while((chunk = template_parser_next_chunk(&usage_template_parser,
        &chunk_size)) != NULL)
    {
        std::cerr.write(chunk, chunk_size);
    }
    std::cerr << std::endl;
}

/* Parse port number */
static int parsePort(std::istream& s, uint16_t* port)
{
    unsigned int val;
    s >> val;
    
    if(!s)
    {
        std::cerr << "Failed to parse port number as integer.\n";
        return -1;
    }
    if(val > 0xffff)
    {
        std::cerr << "Port number is too large.\n";
        return -1;
    }
    /* Network byte order */
    *port = htons(val);
    return 0;
}

/* Parse internet address and possible port. */
static int parseInetAddr(std::istream& s, struct sockaddr_in* addr,
    uint16_t portDefault_native)
{
    memset(addr, 0, sizeof(*addr));
    addr->sin_family = AF_INET;
    for(int i = 0; i < 4; i++)
    {
        if(i != 0)
        {
            int ch = s.peek();
            if(ch == std::istream::traits_type::eof())
            {
                std::cerr << "End of inet address string while '.' expected.\n";
                return -1;
            }
            else if(ch != '.')
            {
                std::cerr << "Unexpected character '" << (char)ch
                    <<"' while '.' expected.\n";
                return -1;
            }
            s.get();
        }
        unsigned int component;
        s >> component;
        if(!s)
        {
            std::cerr << "Failed to parse " << i + 1
                << "-th component of inet address.\n";
            return -1;
        }
        if(component > 255)
        {
            std::cerr << i + 1 << "-th component of inet address "
                "is too large.\n";
            return -1;
        }
        
        unsigned char& componentPlace
            = ((uint8_t*)&addr->sin_addr.s_addr)[i];
        
        componentPlace = (uint8_t)component;
    }
    
    char c = s.peek();
    if(c == std::istream::traits_type::eof())
    {
        addr->sin_port = htons(portDefault_native);
        return 0;
    }
    else if(c != ':')
    {
        std::cerr << "Unexcepted symbol '" << c << "' after inet address.";
        return 1;
    }
    else
    {
        s.get();
        return parsePort(s, &addr->sin_port);
    }
}

int ControlParams::parseParameters(int argc, char** argv)
{
     /* needs only for start command */
     const char* receiverPath = KEDR_TRACE_RECEIVER_PATH;
    /* Options identificators */
    enum OptID
    {
        optError = '?',
        
        optHelp  = 'h',
        
        optStart = 255,
        optStop,
        optInitSession,
        optBreakSession,
        optStartTrace,
        optStopTrace,
        optReceiverPort,
        optReceiverPath
    };

    // Available program's options
    static const char short_options[] = "h";
    static struct option long_options[] = {
        {"start", 1, 0, optStart},
        {"stop", 0, 0, optStop},
        {"init-session", 1, 0, optInitSession},
        {"break-session", 1, 0, optBreakSession},
        {"start-trace", 0, 0, optStartTrace},
        {"stop-trace", 0, 0, optStopTrace},
        {"receiver-port", 1, 0, optReceiverPort},
        {"receiver-path", 1, 0, optReceiverPath},
        {"help", 0, 0, optHelp},
        {0, 0, 0, 0}
    };
    int opt;

    for(opt = getopt_long(argc, argv, short_options, long_options, NULL);
        opt != -1;
        opt = getopt_long(argc, argv, short_options, long_options, NULL))
    {
        switch((OptID)opt)
        {
        case optError:
            //error in options
            if(optind < argc)
            {
                std::cerr << "Incorrect option '" << argv[optind] << "'.\n";
            }
            return -1;
        break;
        case optStart:
            actions.push_back(new ControlActionStart(receiverPath, optarg));
        break;
        case optStop:
            actions.push_back(new ControlActionStop());
        break;
        case optInitSession:
            {
                std::istringstream s(optarg);
                struct sockaddr_in serverAddr;
                
                int result = parseInetAddr(s, &serverAddr, TRACE_SERVER_PORT);
                if(result != 0) return result;
                
                actions.push_back(new ControlActionInitSession(&serverAddr));
            }
        break;
        case optBreakSession:
            {
                std::istringstream s(optarg);
                struct sockaddr_in serverAddr;
                
                int result = parseInetAddr(s, &serverAddr, TRACE_SERVER_PORT);
                if(result != 0) return result;

                actions.push_back(new ControlActionBreakSession(&serverAddr));
            }
        break;
        case optStartTrace:
            actions.push_back(new ControlActionStartTrace());
        break;
        case optStopTrace:
            actions.push_back(new ControlActionEndTrace());
        break;
        
        case optReceiverPort:
            {
                std::istringstream s(optarg);

                int result = parsePort(s, &receiverPort);
                if(result != 0) return result;
            }
        break;
        case optReceiverPath:
            receiverPath = optarg;
        break;
        case optHelp:
            print_usage();
            return 1;
        default:
            return -1;
        }
    }
    return 0;

}

/* Trace reciever control */
bool TraceReceiverControl::isReceiverTerminated = false;
bool TraceReceiverControl::isReceiverInitialized = false;

TraceReceiverControl::TraceReceiverControl(uint16_t controlPort_native,
    uint16_t receiverPort_native): isSockConnected(false)
{
    controlAddr.sin_family = AF_INET;
    controlAddr.sin_port = htons(controlPort_native);
    controlAddr.sin_addr.s_addr = htonl(INADDR_ANY);
    
    receiverAddr.sin_family = AF_INET;
    receiverAddr.sin_port = htons(receiverPort_native);
    receiverAddr.sin_addr.s_addr = inet_addr("127.0.0.1");

    sock = socket(PF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (sock < 0)
    {
        throw std::runtime_error("Failed to create receiver socket");
    }

    int result = bind(sock, (struct sockaddr*)&controlAddr,
        sizeof(controlAddr));
    if(result < 0)
    {
        close(sock);
        throw std::runtime_error("Failed to bind control socket");
    }
}

void TraceReceiverControl::finalize(void)
{
    if(sock != -1) close(sock);
}

TraceReceiverControl::~TraceReceiverControl(void)
{
    finalize();
}

int TraceReceiverControl::sendControlMessage(
    enum kedr_message_control_type type,
    const void* data, int dataSize)
{
    if(sockConnect() != 0) return -1;

    struct kedr_message_header kedrControl;
    
    kedrControl.magic = htonl(KEDR_MESSAGE_HEADER_CONTROL_MAGIC);
    kedrControl.seq = 0;
    kedrControl.type = type;
    
    struct iovec vec[2];
    vec[0].iov_base = &kedrControl;
    vec[0].iov_len = kedr_message_header_size;
    vec[1].iov_base = (void*)data;/* remove constantness attribute */
    vec[1].iov_len = dataSize;
    
    struct msghdr message;
    memset(&message, 0, sizeof(message));
    
    message.msg_iov = vec;
    message.msg_iovlen = 2;

    
    int result = sendmsg(sock, &message, 0);
    if(result < (int)kedr_message_header_size + dataSize)
    {
        return -1;
    }
    
    return 0;
}

int TraceReceiverControl::recvControlMessage(
    enum kedr_message_info_type* type, void* data, int dataSize)
{
    if(sockConnect() != 0) return -1;
    
    while(1)
    {
        fd_set readSet;
        
        FD_ZERO(&readSet);
        FD_SET(sock, &readSet);
        
        struct timeval timeout = {KEEP_ALIVE_PERIOD, 0};
        
        int result = select(sock + 1, &readSet, NULL, NULL, &timeout);
        if(result == 1)
        {
            /* Message(or error) is available in the socket */
            return _recvControlMessage(type, data, dataSize);
        }
        else if(result == 0)
        {
            /* timeout */
            result = sendControlMessage(kedr_message_control_type_keep_alive);
            if(result != 0)
            {
                std::cerr << "Failed to send keep-alive packet.\n";
                return result;
            }
            continue;
        }
        else /*result == -1*/
        {
            std::cerr << "Failed to wait message in the socket. "
                << "Perhaps, trace receiver is dead.\n";
            return -1;
        }
    }
}

int TraceReceiverControl::_recvControlMessage(
    enum kedr_message_info_type* type, void* data, int dataSize)
{
    struct kedr_message_header kedrControl;
    
    struct iovec vec[2];
    vec[0].iov_base = &kedrControl;
    vec[0].iov_len = kedr_message_header_size;
    vec[1].iov_base = data;
    vec[1].iov_len = dataSize;
    
    struct msghdr message;
    memset(&message, 0, sizeof(message));
    
    

    message.msg_iov = vec;
    message.msg_iovlen = 2;

    int result = recvmsg(sock, &message, 0);
    if(result < (int)kedr_message_header_size + dataSize)
    {
        return -1;
    }
    
    if(kedrControl.magic != htonl(KEDR_MESSAGE_HEADER_CONTROL_MAGIC))
    {
        std::cerr << "Invalid magic field in the recieved information packet.\n";
        return -1;
    }
    
    *type = (enum kedr_message_info_type)kedrControl.type;
    
    return 0;
}

int TraceReceiverControl::sockConnect(void)
{
    if(!isSockConnected)
    {
        int result = connect(sock,
            (const struct sockaddr*)&receiverAddr,
            sizeof(receiverAddr));
        
        if(result == -1)
        {
            std::cerr << "Failed  to connect socket.";
            return -1;
        }
        isSockConnected = true;
    }
    return 0;
}

void TraceReceiverControl::sockDisconnect(void)
{
    if(isSockConnected)
    {
        struct sockaddr nullAddr;
        nullAddr.sa_family = AF_UNSPEC;
        connect(sock, &nullAddr, sizeof(&nullAddr));
        
        isSockConnected = false;
    }
}

int TraceReceiverControl::waitForInitializedPrepare(void)
{
    sigset_t signalMask;
    sigemptyset(&signalMask);
    sigaddset(&signalMask, SIGCHLD);
    sigaddset(&signalMask, SIGUSR1);
    
    sigprocmask(SIG_BLOCK, &signalMask, NULL);
    
    isReceiverInitialized = false;
    isReceiverTerminated = false;
    
    return 0;
}

void TraceReceiverControl::waitForInitializedCancel(void)
{
    sigset_t signalMask;
    sigemptyset(&signalMask);
    sigaddset(&signalMask, SIGCHLD);
    sigaddset(&signalMask, SIGUSR1);
    
    sigprocmask(SIG_BLOCK, &signalMask, NULL);
}

int TraceReceiverControl::waitForInitialized(void)
{
    struct sigaction saChild, saChildOld;
    saChild.sa_flags = 0;
    saChild.sa_handler = onReceiverTerminated;
    sigemptyset(&saChild.sa_mask);
    if (sigaction(SIGCHLD, &saChild, &saChildOld) == -1)
    {
        std::cerr << "Failed to set handler for SIGCHLD signal.\n";
        return -1;
    }

    struct sigaction saUsr1, saUsr1Old;
    saUsr1.sa_flags = 0;
    saUsr1.sa_handler = onReceiverInitialized;
    sigemptyset(&saUsr1.sa_mask);
    if (sigaction(SIGUSR1, &saUsr1, &saUsr1Old) == -1)
    {
        std::cerr << "Failed to set handler for SIGUSR1 signal.\n";
        sigaction(SIGCHLD, &saChildOld, NULL);
        return -1;
    }

    sigset_t signalMask;
    sigemptyset(&signalMask);

    do{
        sigsuspend(&signalMask);
        assert(errno == EINTR);
    }while(!isReceiverInitialized && !isReceiverTerminated);

    sigaction(SIGCHLD, &saUsr1Old, NULL);
    sigaction(SIGCHLD, &saChildOld, NULL);

    return isReceiverTerminated? 1 : 0;
}

int TraceReceiverControl::waitForFinalizedPrepare(void)
{
    sigset_t signalMask;
    sigemptyset(&signalMask);
    sigaddset(&signalMask, SIGUSR1);
    sigaddset(&signalMask, SIGUSR2);
    
    sigprocmask(SIG_BLOCK, &signalMask, NULL);
    
    isReceiverInitialized = true;
    isReceiverTerminated = false;
    
    return 0;
}

void TraceReceiverControl::waitForFinalizedCancel(void)
{
    sigset_t signalMask;
    sigemptyset(&signalMask);
    sigaddset(&signalMask, SIGUSR1);
    sigaddset(&signalMask, SIGUSR2);
    
    sigprocmask(SIG_BLOCK, &signalMask, NULL);
}

int TraceReceiverControl::waitForFinalized(void)
{
    if(sockConnect() != 0)
    {
        std::cerr << "Failed to connect socket for wait finalization.\n";
        return 1;
    }
    
    struct sigaction saUsr1, saUsr1Old;
    saUsr1.sa_flags = 0;
    saUsr1.sa_handler = onReceiverUninitialized;
    sigemptyset(&saUsr1.sa_mask);
    if (sigaction(SIGUSR1, &saUsr1, &saUsr1Old) == -1)
    {
        std::cerr << "Failed to set handler for SIGUSR1 signal.\n";
        return -1;
    }

    struct sigaction saUsr2, saUsr2Old;
    saUsr2.sa_flags = 0;
    saUsr2.sa_handler = onReceiverTerminated;
    sigemptyset(&saUsr2.sa_mask);
    if (sigaction(SIGUSR2, &saUsr2, &saUsr2Old) == -1)
    {
        std::cerr << "Failed to set handler for SIGUSR2 signal.\n";
        sigaction(SIGUSR1, &saUsr1Old, NULL);
        return -1;
    }

    sigset_t signalMask;
    sigemptyset(&signalMask);

    do{
        fd_set readSet;
        FD_ZERO(&readSet);
        FD_SET(sock, &readSet);
        
        struct timespec timeout = {KEEP_ALIVE_PERIOD, 0};
        
        int result = pselect(sock + 1, &readSet, NULL, NULL, &timeout,
            &signalMask);
        if(result == 1)
        {
            /* Message or error in the socket is available. */
            kedr_message_info_type type;
            result = recvControlMessage(&type);
            if(result == 0)
            {
                std::cerr << "Unexpected message from trace receiver.\n";
            }
            else if(result == 1)
            {
                /* Ignore incorrect message*/
            }
            else /* result == -1*/
            {
                /* Connection is died */
                std::cerr << "Connection with trace receiver is dead. "
                    << "Peharps it is not running.\n";
                goto terminated;
            }
        }
        else if(result == 0)
        {
            /* Timeout */
            result = sendControlMessage(kedr_message_control_type_keep_alive);
            if(result != 0)
            {
                /* Connection is died */
                std::cerr << "Connection with trace receiver is dead. "
                    << "Peharps it is not running.\n";
                goto terminated;
            }
        }
        else /* result == -1 */
        {
            if(errno != EINTR)
            {
                std::cerr << "Failed to wait on socket using pselect.\n";
                goto err;
            }
            /* Interrupted with signal */
        }
        
    }while(isReceiverInitialized && !isReceiverTerminated);
    
    while(!isReceiverTerminated)
    {
        errno = sigsuspend(&signalMask);
        assert(errno == -EINTR);
    }

    sigaction(SIGCHLD, &saUsr1Old, NULL);
    sigaction(SIGCHLD, &saUsr2Old, NULL);

    return 0;

terminated:
    sigaction(SIGCHLD, &saUsr1Old, NULL);
    sigaction(SIGCHLD, &saUsr2Old, NULL);
    
    return 1;
    
err:
    sigaction(SIGCHLD, &saUsr1Old, NULL);
    sigaction(SIGCHLD, &saUsr2Old, NULL);
    
    return -1;

}