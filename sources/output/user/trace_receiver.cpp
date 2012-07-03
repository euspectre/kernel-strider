/* Receive trace from trace_server and store it in files. */

/* Interpret metadata packets, pads normal packets */
#include <kedr/ctf_reader/ctf_reader.h>

#include "udp_packet_definition.h"
#include "trace_receiver.h"

#include <string>

#include <sys/socket.h> /* socket operations */
#include <netinet/in.h> /* struct sockaddr_in */
#include <arpa/inet.h> /* inet_addr() */
#include <cstring> /* memcpy, strerror */

#include <sstream> /* streams from string */
#include <iostream> /* cerr */
#include <memory> /* auto_ptr */
#include <fstream> /* file streams */

#include <stdexcept> /* invalid_argument and other exceptions */
#include <cassert> /* assert() macro */

#include <signal.h> /* send signals to control program */

#include <algorithm> /*for_each*/

#include <cstdlib> /* system() */
#include <sys/wait.h> /* resolve exited status of program calling via system() */

#include <fcntl.h> /* Flags for open() */

#include <errno.h> /* errno */

#include <unistd.h> /* write(), getppid() */
#include <sys/types.h> /* pid_t */

class TraceReceiver;

/* Information about waiter of some state-transition. */
struct NotificationWaiter
{
    uint16_t port;
    
    NotificationWaiter(uint16_t port): port(port) {}
};

/* 
 * Trace session - session for receive one trace.
 * 
 * Created with first metadata packet received.
 * Destroyed with kedr_message_type_mark_trace_end mark or when
 * connection with trace sender die.
 * 
 * May contain some cached values.
 */
class TraceSession
{
public:
    /* Create session from first metadata packet */
    TraceSession(const std::string& traceDirectoryFormat,
        const char* data, size_t dataSize);
    ~TraceSession(void);
    
    const UUID* getUUID(void) const {return &uuid;};
    /* Add data containing metadata packet */
    void addMetaPacket(const char* data, size_t dataSize);
    /* Tell that metadata is fully received */
    void endMeta(void);
    /* Add normal CTF packet */
    void addPacket(const char* data, size_t dataSize);
    /* Markers of whole trace(currently do nothing) */
    void traceStart(void) {};
    void traceEnd(void) {};
private:
    UUID uuid;
    std::string traceDirectory;
    /* Created when whole metadata is received */
    CTFReader* reader;
    /* Return name of the file contained metadata */
    std::string getMetadataFilename(void) const;
    /* Return name of the file contained stream for given packet */
    std::string getStreamFilename(CTFReader::Packet& packet) const;
};

/* 
 * Send session - session with one trace sender.
 * 
 * Created when receive 'kedr_message_type_mark_session_start'
 * message, destroyed when receive 'kedr_message_type_mark_session_end'
 * message.
 */
class SendSession
{
public:
    SendSession(TraceReceiver& receiver,
        const std::string& traceDirectoryFormat,
        const std::vector<NotificationWaiter> traceStartWaiters
        = std::vector<NotificationWaiter>());
    ~SendSession(void);
    
    void addMetaPacket(const char* data, size_t dataSize);
    void addPacket(const char* data, size_t dataSize);
    
    void endMeta(void);
    void traceStart(void);
    void traceEnd(void);
    
    void addTraceStartWaiter(const NotificationWaiter& waiter);
    void addTraceStopWaiter(const NotificationWaiter& waiter);
    
private:
    /* Object which use this one. Need for send notification messages. */
    TraceReceiver& receiver;

    std::string traceDirectoryFormat;
    /* Currently trace sender may send only one trace at a moment. */
    TraceSession* traceSession;
    
    std::vector<NotificationWaiter> traceStartWaiters;
    std::vector<NotificationWaiter> traceStopWaiters;
};

/* Main class, described receiver */
class TraceReceiver
{
public:
    TraceReceiver(uint16_t port_native,
        const std::string& traceDirectoryFormat);
    ~TraceReceiver(void);
    
    void mainLoop(void);
    
    void addTraceStartWaiter(const NotificationWaiter& waiter);
    void addTraceStopWaiter(const NotificationWaiter& waiter);
    
    void addSessionStartWaiter(const NotificationWaiter& waiter);
    void addSessionStopWaiter(const NotificationWaiter& waiter);
    
    void addStopWaiter(pid_t controlPid);
    /* 
     * Make trace directory from its format variant using given
     * trace parameters.
     */
static std::string traceDirectory(const std::string& traceDirectoryFormat,
    const UUID& uuid);
    
    void sendNotification(enum kedr_message_info_type type,
        const std::vector<NotificationWaiter>& waiters);
    
    void sendNotification(enum kedr_message_info_type type,
        const NotificationWaiter& waiter);
private:
    int sock;
    std::string traceDirectoryFormat;
    
    /* Currently only one send session is supported. */
    SendSession* sendSession;
    struct sockaddr_in senderAddr;
    
    std::vector<NotificationWaiter> sessionStartWaiters;
    std::vector<NotificationWaiter> sessionStopWaiters;
    
    std::vector<pid_t> stopWaiters;
    
    /* Collect waiters for trace start when no session is active */
    std::vector<NotificationWaiter> traceStartWaiters;

    void processMessage(const struct sockaddr_in* from,
        enum kedr_message_type type, const char* data, int dataSize);
    
    void processControlMessage(const struct sockaddr_in* from,
        enum kedr_message_control_type type, const char* data, int dataSize);
    
    void sendCommand(enum kedr_message_command_type type,
        const struct sockaddr_in* to);

    bool terminated;
};


static int parsePort_native(const char* str, uint16_t* value_native)
{
    std::istringstream s(str);
    unsigned int val;
    s >> val;
    
    if(!s)
    {
        std::cerr << "Failed to parse port number as integer.\n";
        return -1;
    }
    if(val > 0xffff)
    {
        std::cerr << "Port number is too large is too large.\n";
        return -1;
    }
    /* Native byte order */
    *value_native = val;
    return 0;
}
/*
 * First parameter - own port, second - control port
 * (used only to send notification about initializing).
 */
int main(int argc, char** argv)
{
    if(argc < 3)
    {
        std::cerr << "Incorrect number of parameters: " << argc - 1 << " .\n";
        std::cerr << "Usage: kedr_trace_receiver <receiver_port> <trace_directory_format>\n";
        return -1;
    }
    /* Port numbers in native byte order */
    uint16_t receiverPort_native = 0;
    
    if(parsePort_native(argv[1], &receiverPort_native))
    {
        return -1;
    }
    
    //std::cerr << "Starting trace receiver...\n";
    
    TraceReceiver traceReceiver(receiverPort_native, argv[2]);
        
    //std::cerr << "Trace receiver has started.\n";

    traceReceiver.mainLoop();
    
    //std::cerr << "Trace Receiver is terminated.\n";
    
    return 0;
}

/*************************** Implementation ***************************/
/* Helpers for search typed variables */
static const CTFVarInt& findInt(const CTFReader& reader, const std::string& name)
{
    const CTFVar* var = reader.findVar(name);
    if(var == NULL)
    {
        std::cerr << "Failed to find integer variable '" << name << "'.\n";
        throw std::invalid_argument("Invalid variable name");
    }
    if(!var->isInt())
    {
        std::cerr << "Variable with name '" << name << "' is not integer.\n";
        throw std::invalid_argument("Invalid variable type");
    }
    return *static_cast<const CTFVarInt*>(var);
}

static const CTFVarEnum& findEnum(const CTFReader& reader, const std::string& name)
{
    const CTFVar* var = reader.findVar(name);
    if(var == NULL)
    {
        std::cerr << "Failed to find enumeration variable '" << name << "'.\n";
        throw std::invalid_argument("Invalid variable name");
    }
    if(!var->isEnum())
    {
        std::cerr << "Variable with name '" << name << "' is not enumeration.\n";
        throw std::invalid_argument("Invalid variable type");
    }
    return *static_cast<const CTFVarEnum*>(var);
}


/* For auto closing file descriptor */
class auto_fd
{
public:
    auto_fd(int fd): fd(fd) {}
    ~auto_fd(void) {if(fd != -1) close(fd);}
    
    int get(void) {return fd;}
    void reset(int fd = -1) {if(this->fd != -1) close(this->fd); this->fd = fd;}
private:
    int fd;
};

/* Trace Session */

/* 
 * Create directory and all its parents, if needed.
 * 
 * Return 0 on success.
 * Return -1 on fail.
 */
static int createDir(const std::string& path)
{
    std::string command("mkdir -p ");
    command += path;
    int status = system(command.c_str());
    if(WIFEXITED(status))
    {
        int result = WEXITSTATUS(status);
        if(result == 0) return 0;/* Success */
        else return -1;
    }
    else return -1;
}

TraceSession::TraceSession(const std::string& traceDirectoryFormat,
    const char* data, size_t dataSize) : reader(NULL)
{
    int result;
    
    std::stringstream firstPacketStream;
    firstPacketStream.write(data, dataSize);
    
    CTFReader::MetaPacket metaPacket(firstPacketStream);
    
    memcpy(uuid.bytes(), metaPacket.getUUID()->bytes(), 16);
    
    traceDirectory = TraceReceiver::traceDirectory(traceDirectoryFormat,
        uuid);

    if(*traceDirectory.rbegin() != '/') traceDirectory += '/';
    
    /* Create directory if it is not exists. */
    if(createDir(traceDirectory) != 0)
    {
        throw std::runtime_error("Failed to create directory for trace.");
    }

    std::string metadataFilename(getMetadataFilename());
    auto_fd metaFD(open(metadataFilename.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0755));
    if(metaFD.get() == -1)
    {
        std::cerr << "Failed to open/create metadata file '" 
            << metadataFilename << "': " << strerror(errno) << ".\n";
        throw std::runtime_error("Failed to create file with metadata");
    }
    result = write(metaFD.get(), metaPacket.getMetadata(), metaPacket.getMetadataSize());
    if(result != (int)metaPacket.getMetadataSize())
    {
        std::cerr << "Failed to write metadata portion to file '"
            << metadataFilename << "': " << strerror(errno) << ".\n";
        throw std::runtime_error("Failed to write metadata portion.");
    }
}

TraceSession::~TraceSession(void)
{
    delete reader;
}

void TraceSession::addMetaPacket(const char* data, size_t dataSize)
{
    assert(reader == NULL);
    
    int result;
    
    std::stringstream packetStream;
    packetStream.write(data, dataSize);
    
    CTFReader::MetaPacket metaPacket(packetStream);
    
    std::string metadataFilename = getMetadataFilename();
    
    auto_fd metaFD(open(metadataFilename.c_str(), O_WRONLY | O_APPEND));
    if(metaFD.get() == -1)
    {
        std::cerr << "Failed to open metadata file '" 
            << metadataFilename << "': " << strerror(errno) << ".\n";
        throw std::runtime_error("Failed to add metadata portion.");
    }
    
    result = write(metaFD.get(),
        metaPacket.getMetadata(), metaPacket.getMetadataSize());
    if(result != (int)metaPacket.getMetadataSize())
    {
        std::cerr << "Failed to add metadata portion to file '"
            << metadataFilename << "': " << strerror(errno) << ".\n";
        throw std::runtime_error("Failed to add metadata portion.");
    }
}

void TraceSession::endMeta(void)
{
    assert(reader == NULL);
    
    std::string metadataFilename = getMetadataFilename();
    std::ifstream metadataFile;
    metadataFile.open(metadataFilename.c_str());
    if(!metadataFile)
    {
        std::cerr << "Failed to open file '" << metadataFilename <<
            "' with metadata of the trace." << std::endl;
        throw std::runtime_error("Failed to open metadata file");
    }
    
    reader = new CTFReader(metadataFile);
}

void TraceSession::addPacket(const char* data, size_t dataSize)
{
    assert(reader != NULL);
    
    int result;
    
    std::stringstream packetStream;
    packetStream.write(data, dataSize);
    
    CTFReader::Packet packet(*reader, packetStream);
    
    int packetSize = packet.getPacketSize();
    int contentSize = packet.getContentSize();
    
    if(dataSize * 8 < (size_t)contentSize)
    {
        std::cerr << "Size of data in UDP packet is " << dataSize
            << ", but size of content in CTF packet is " << contentSize << ".\n";
        throw std::logic_error("Inconsistent size of UDP packet.");
    }

    if(dataSize * 8 > (size_t)packetSize)
    {
        std::cerr << "Size of data in UDP packet is " << dataSize
            << ", but size of CTF packet is " << packetSize << ".\n";
        throw std::logic_error("Inconsistent size of UDP packet.");
    }

    
    std::string streamFilename = getStreamFilename(packet);
    
    auto_fd streamFD(open(streamFilename.c_str(), O_WRONLY | O_CREAT | O_APPEND,
        0755));
    if(streamFD.get() == -1)
    {
        std::cerr << "Failed to open stream file '" 
            << streamFilename <<"': " << strerror(errno) << ".\n";
        throw std::runtime_error("Failed to add trace portion.");
    }
    
    result = write(streamFD.get(), data, dataSize);
    if(result != (int)dataSize)
    {
        std::cerr << "Failed to add trace portion to file '"
            << streamFilename << "': " << strerror(errno) << ".\n";
        throw std::runtime_error("Failed to add trace portion.");
    }
    
    int padSize = packetSize / 8 - dataSize;
    if(padSize != 0)
    {
        std::vector<char> padding(padSize, '\0');
        result = write(streamFD.get(), padding.data(), padding.size());
        if(result != (int)padding.size())
        {
            std::cerr << "Failed to add padding of trace packet to file '"
                << streamFilename << "': " << strerror(errno) << ".\n";
            throw std::runtime_error("Failed to pad trace packet.");
        }
    }
}

std::string TraceSession::getMetadataFilename(void) const
{
    return traceDirectory + "metadata";
}

std::string TraceSession::getStreamFilename(CTFReader::Packet& packet) const
{
    assert(reader);
    
    const CTFVarEnum& streamTypeVar = findEnum(*reader,
        "trace.packet.header.stream_type");
    const CTFVarInt& cpuVar = findInt(*reader,
        "trace.packet.header.cpu");
    
    std::ostringstream os;
    os << traceDirectory << streamTypeVar.getEnum(packet)
        << cpuVar.getInt32(packet);
    
    return os.str();
}

/* Send session*/
SendSession::SendSession(TraceReceiver& receiver,
    const std::string& traceDirectoryFormat,
    const std::vector<NotificationWaiter> traceStartWaiters)
    : receiver(receiver),
    traceDirectoryFormat(traceDirectoryFormat), traceSession(NULL),
    traceStartWaiters(traceStartWaiters)
{
}

SendSession::~SendSession(void)
{
    if(traceSession)
    {
        receiver.sendNotification(kedr_message_info_type_stop_connection,
            traceStopWaiters);
        
        delete traceSession;
    }
    else
    {
        /* Failed to wait trace start */
        receiver.sendNotification(kedr_message_info_type_stop_connection,
            traceStartWaiters);
    }
}

void SendSession::addMetaPacket(const char* data, size_t dataSize)
{
    if(traceSession == NULL)
    {
        traceSession = new TraceSession(traceDirectoryFormat, data, dataSize);
        receiver.sendNotification(kedr_message_info_type_start_connection,
            traceStartWaiters);
        traceStartWaiters.clear();
    }
    else
    {
        //TODO: check UUID
        traceSession->addMetaPacket(data, dataSize);
    }
}

void SendSession::endMeta(void)
{
    assert(traceSession);
    
    traceSession->endMeta();
}

void SendSession::addPacket(const char* data, size_t dataSize)
{
    assert(traceSession);
    
    //TODO: check UUID
    traceSession->addPacket(data, dataSize);
}

void SendSession::traceStart()
{
    assert(traceSession);
    
    //TODO: UUID currently cannot be checked
    traceSession->traceStart();
}

void SendSession::traceEnd()
{
    assert(traceSession);
    
    //TODO: UUID currently cannot be checked
    traceSession->traceEnd();
    
    receiver.sendNotification(kedr_message_info_type_stop_trace,
        traceStopWaiters);
    traceStopWaiters.clear();
    
    delete traceSession;
    traceSession = NULL;
}

void SendSession::addTraceStartWaiter(const NotificationWaiter& waiter)
{
    if(!traceSession)
    {
        traceStartWaiters.push_back(waiter);
    }
    else
    {
        /* There is already active trace, send notification immediately.*/
        receiver.sendNotification(kedr_message_info_type_start_trace, waiter);
    }
}

void SendSession::addTraceStopWaiter(const NotificationWaiter& waiter)
{
    if(traceSession)
    {
        traceStopWaiters.push_back(waiter);
    }
    else
    {
        /* Currently there is no trace, send notification immediately.*/
        receiver.sendNotification(kedr_message_info_type_stop_trace, waiter);
    }
}


/* Trace receiver */
TraceReceiver::TraceReceiver(uint16_t port_native,
    const std::string& traceDirectoryFormat)
    : traceDirectoryFormat(traceDirectoryFormat),
    sendSession(NULL), terminated(false)
{
    struct sockaddr_in receiverAddr;
    receiverAddr.sin_family = AF_INET;
    receiverAddr.sin_port = htons(port_native);
    receiverAddr.sin_addr.s_addr = htonl(INADDR_ANY);
    
    sock = socket(PF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (sock < 0)
    {
        throw std::runtime_error("Failed to create receiver socket");
    }

    int result = bind(sock, (struct sockaddr*)&receiverAddr,
        sizeof(receiverAddr));
    if(result < 0)
    {
        close(sock);
        throw std::runtime_error("Failed to bind receiver socket");
    }

    pid_t callerPid = getppid();
    
    if(kill(callerPid, SIGUSR1) != 0)
    {
        throw std::runtime_error("Failed to send signal to control program.");
    }
}

static void sendUSR1(pid_t pid) {kill(pid, SIGUSR1);}
static void sendUSR2(pid_t pid) {kill(pid, SIGUSR2);}
TraceReceiver::~TraceReceiver(void)
{
    sendNotification(kedr_message_info_type_stop,
        sessionStartWaiters);
    sendNotification(kedr_message_info_type_stop,
        sessionStopWaiters);
    sendNotification(kedr_message_info_type_stop,
        traceStartWaiters);
    
    std::for_each(stopWaiters.begin(), stopWaiters.end(), sendUSR1);

    close(sock);
    
    std::for_each(stopWaiters.begin(), stopWaiters.end(), sendUSR2);
}

void TraceReceiver::mainLoop(void)
{
    struct kedr_message_header kedrHeader;
    char data[TRACE_SERVER_MSG_LEN_MAX - kedr_message_header_size];
    
    struct iovec vecs[2];
    vecs[0].iov_base = &kedrHeader;
    vecs[0].iov_len = kedr_message_header_size;
    vecs[1].iov_base = data;
    vecs[1].iov_len = sizeof(data);
    
    struct sockaddr_in fromAddr;
    
    struct msghdr header;
    memset(&header, 0, sizeof(header));
    
    header.msg_name = &fromAddr;
    header.msg_namelen = sizeof(fromAddr);
    
    header.msg_iov = vecs;
    header.msg_iovlen = 2;
    
    while(!terminated)
    {
        int result = recvmsg(sock, &header, 0);
        if(result < 0)
        {
            std::cerr << "Failed to receive message\n";
            //TODO: process EAGAIN error.
            break;
        }
        
        else if(header.msg_namelen < sizeof(fromAddr))
        {
            std::cerr << "Ignore non-IP packets.\n";
        }
        else if(result < (int)kedr_message_header_size)
        {
            std::cerr << "Recieve packet which size is too small("
                << result << "). Ignore it.\n";
        }
        else if(kedrHeader.magic == htonl(KEDR_MESSAGE_HEADER_MAGIC))
        {
            processMessage(&fromAddr,
                (enum kedr_message_type)kedrHeader.type,
                data, result - kedr_message_header_size);
        }
        else if(kedrHeader.magic == htonl(KEDR_MESSAGE_HEADER_CONTROL_MAGIC))
        {
            processControlMessage(&fromAddr,
                (enum kedr_message_control_type)kedrHeader.type,
                data, result - kedr_message_header_size);
        }
        else
        {
            std::ios_base::fmtflags flagsOld = std::cerr.setf(
                std::ios_base::hex, std::ios_base::basefield);
            std::cerr << "Packet with unknown magic field "
                << ntohl(kedrHeader.magic) << " (packet size is "
                << result << "). Ignore it.\n";
            std::cerr.setf(flagsOld);
        }
    }
}

void TraceReceiver::processMessage(const struct sockaddr_in* from,
    enum kedr_message_type type, const char* data, int dataSize)
{
    if(sendSession)
    {
        if((from->sin_addr.s_addr != senderAddr.sin_addr.s_addr)
            || from->sin_port != senderAddr.sin_port)
        {
            std::cerr << "Ignore packets which are not from "
                << "current trace server.\n";
            return;
        }
    }
    else
    {
        if(type != kedr_message_type_mark_session_start)
        {
            std::cerr << "Ignore all packets before first "
                "session start mark.\n";
            return;
        }
        
        sendSession = new SendSession(*this, traceDirectoryFormat,
            traceStartWaiters);
        traceStartWaiters.clear();
        memcpy(&senderAddr, from, sizeof(senderAddr));
        
        sendNotification(kedr_message_info_type_start_connection,
            sessionStartWaiters);
        sessionStartWaiters.clear();
        
        return;
    }
    
    /* 
     * Here sender is active and message come from it with correct
     * header size.
     */
    
    //TODO: check sequential number
    
    switch((enum kedr_message_type)(type))
    {
    case kedr_message_type_ctf:
        sendSession->addPacket(data, dataSize);
    break;
    case kedr_message_type_meta_ctf:
        sendSession->addMetaPacket(data, dataSize);
    break;
    case kedr_message_type_mark_meta_ctf_end:
        sendSession->endMeta();
    break;
    case kedr_message_type_mark_session_start:
        std::cerr << "Ignore session start command while another "
            << "session is active.\n";
    break;
    case kedr_message_type_mark_session_end:
        delete sendSession;
        sendSession = NULL;
        
        sendNotification(kedr_message_info_type_stop_connection,
            sessionStopWaiters);
        sessionStopWaiters.clear();
    break;
    case kedr_message_type_mark_trace_start:
        sendSession->traceStart();
    break;
    case kedr_message_type_mark_trace_end:
        sendSession->traceEnd();
    break;
    default:
        std::cerr << "Unknown command type " << type
            << ".\n";
    }
}

void TraceReceiver::processControlMessage(const struct sockaddr_in* from,
    enum kedr_message_control_type type, const char* data, int dataSize)
{
    switch(type)
    {
    case kedr_message_control_type_keep_alive:
        /* Ignore this message.*/
    break;
    /* Actions */
    case kedr_message_control_type_terminate:
        terminated = true;
    break;
    case kedr_message_control_type_init_connection:
        if(dataSize < (int)sizeof(struct sockaddr_in))
        {
            std::cerr << "Too small size of data in control "
                "message of 'init_connection' type. Ignore it.\n";
        }
        else
        {
            sendCommand(kedr_message_command_type_start,
                (const struct sockaddr_in*)data);
        }
    break;
    case kedr_message_control_type_break_connection:
        if(dataSize < (int)sizeof(struct sockaddr_in))
        {
            std::cerr << "Too small size of data in control "
                "message of 'break_connection' type. Ignore it.\n";
            return;
        }
        else
        {
            sendCommand(kedr_message_command_type_stop,
                (const struct sockaddr_in*)data);
        }
    break;
    
    /* Waiters */
    case kedr_message_control_type_wait_terminate:
        if(dataSize < (int)sizeof(pid_t))
        {
            std::cerr << "Too small size of data in control "
                "message of 'wait_terminate' type. Ignore it.\n";
            return;
        }
        addStopWaiter(*((const pid_t*)data));
    break;
    case kedr_message_control_type_wait_init_connection:
        addSessionStartWaiter(from->sin_port);
    break;
    case kedr_message_control_type_wait_break_connection:
        addSessionStopWaiter(from->sin_port);
    break;
    case kedr_message_control_type_wait_trace_begin:
        addTraceStartWaiter(from->sin_port);
    break;
    case kedr_message_control_type_wait_trace_end:
        addTraceStopWaiter(from->sin_port);
    break;
    }
}

void TraceReceiver::sendCommand(enum kedr_message_command_type type,
    const struct sockaddr_in* to)
{
    struct kedr_message_header serverCommand;
    serverCommand.magic = htonl(KEDR_MESSAGE_HEADER_MAGIC);
    serverCommand.seq = 0;
    serverCommand.type = type;
    
    sendto(sock, &serverCommand, kedr_message_header_size, 0,
        (const struct sockaddr*)to, sizeof(*to));
}

std::string TraceReceiver::traceDirectory(
    const std::string& traceDirectoryFormat,
    const UUID& uuid)
{
    std::string result = traceDirectoryFormat;
    size_t pos = result.find("%u");
    while(pos != std::string::npos)
    {
        std::ostringstream os;
        os << uuid;
        result.replace(pos, 2, os.str());
        pos = result.find("%u", pos + os.str().size()); 
    }
    return result;
}

void TraceReceiver::sendNotification(enum kedr_message_info_type type,
    const NotificationWaiter& waiter)
{
    struct sockaddr_in destAddr;
    //memset(&destAddr, 0, sizeof(destAddr));
    destAddr.sin_family = AF_INET;
    destAddr.sin_port = waiter.port;
    destAddr.sin_addr.s_addr = inet_addr("127.0.0.1");
    
    struct kedr_message_header notification;
    notification.magic = htonl(KEDR_MESSAGE_HEADER_CONTROL_MAGIC);
    notification.seq = 0;
    notification.type = type;
    
    sendto(sock, &notification, kedr_message_header_size, 0,
        (struct sockaddr*)&destAddr, sizeof(destAddr));
}

void TraceReceiver::sendNotification(enum kedr_message_info_type type,
    const std::vector<NotificationWaiter>& waiters)
{
    std::vector<NotificationWaiter>::const_iterator iter = waiters.begin();
    std::vector<NotificationWaiter>::const_iterator iter_end = waiters.end();
    
    for(;iter != iter_end; ++iter)
        sendNotification(type, *iter);
}

void TraceReceiver::addTraceStartWaiter(const NotificationWaiter& waiter)
{
    if(sendSession)
    {
        sendSession->addTraceStartWaiter(waiter);
    }
    else
    {
        traceStartWaiters.push_back(waiter);
    }
}

void TraceReceiver::addTraceStopWaiter(const NotificationWaiter& waiter)
{
    if(sendSession)
    {
        sendSession->addTraceStopWaiter(waiter);
    }
    else
    {
        /* No sessions - no traces; send notification immediately. */
        sendNotification(kedr_message_info_type_stop_trace, waiter);
    }
}

void TraceReceiver::addSessionStartWaiter(const NotificationWaiter& waiter)
{
    if(!sendSession)
    {
        sessionStartWaiters.push_back(waiter);
    }
    else
    {
        /* Session already started. Send notification immediately. */
        sendNotification(kedr_message_info_type_start_connection, waiter);
    }
}

void TraceReceiver::addSessionStopWaiter(const NotificationWaiter& waiter)
{
    if(sendSession)
    {
        sessionStopWaiters.push_back(waiter);
    }
    else
    {
        /* No sessions. Send notification immediately. */
        sendNotification(kedr_message_info_type_stop_connection, waiter);
    }
}

void TraceReceiver::addStopWaiter(pid_t controlPid)
{
    stopWaiters.push_back(controlPid);
}