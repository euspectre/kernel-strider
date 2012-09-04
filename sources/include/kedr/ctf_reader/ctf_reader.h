/* API for read CTF data from files */

#ifndef CTF_READER_H_INCLUDED
#define CTF_READER_H_INCLUDED

#include "ctf_meta.h"
#include "ctf_var.h"
#include "ctf_context.h"

#include "ctf_type.h"
#include "ctf_var_place.h"

#include <string>
#include <cstring> /* memcmp */

#include <sys/types.h> /* off_t */

#include <iostream>

#include <cassert>

/* Representation of the UUID for the trace */
struct UUID
{
    /* Create UUID with its own value. */
    UUID(void);
    /* Create const UUID with external value*/
    UUID(const unsigned char* val);
    /* Create non-const UUID with external value*/
    UUID(unsigned char* val);

    unsigned char& operator[](int index) {assert(!is_const); return val[index];}
    const unsigned char& operator[](int index) const {return val[index];}

    bool operator==(const UUID& uuid){return memcmp(val, uuid.val, 16) == 0;}
    bool operator!=(const UUID& uuid){return memcmp(val, uuid.val, 16) != 0;}
    
    unsigned char* bytes(void) {assert(!is_const); return val;}
    const unsigned char* bytes(void) const {return val;}
    
    friend std::ostream& operator<< (std::ostream& os, const UUID& uuid);
    friend std::istream& operator>> (std::istream& os, UUID& uuid);

private:
    /* Non-copiable*/
    UUID(const UUID& uuid);
    UUID operator= (const UUID& uuid);

    unsigned char* const val;
    /* buffer for own value */    
    unsigned char buf[16];
    const bool is_const;
};

std::ostream& operator<< (std::ostream& os, const UUID& uuid);
std::istream& operator>> (std::istream& is, UUID& uuid);

class CTFScopeRoot;

class CTFReader: public CTFMeta
{
public:
    /* Construct object by reading metadata from stream */
    CTFReader(std::istream& s);
    ~CTFReader(void);

    /* Find parameter using its full name(e.g. "trace.byte_order"). */
    const std::string* findParameter(const std::string& paramName) const;

    /* 
     * Return UUID of the trace.
     * 
     * If it is not defined, return NULL.
     */
    const UUID* getUUID(void) const {return uuid;}

    /* One event in the CTF trace. */
    class Event;
    /* One packet in the CTF trace. */
    class Packet;
    /* Iterator through packets in the stream. */
    class PacketIterator;
    /* Iterator through events in the packet. */
    class PacketEventIterator;
    /* Iterator through events in the stream(can cross packet boundaries)*/
    class EventIterator;

    /* One packet with CTF metadata */
    class MetaPacket;
    /* Iterator through metadata packets in the stream. */
    class MetaPacketIterator;
    /* CTF magic */
    static const uint32_t magicValue = 0xC1FC1FC1;

    class RootType;
    class RootVar;
private:
    CTFReader(const CTFReader&);/* Cannot be copied */

    /* Scope for store all types(in hierarchy manner)*/
    CTFScopeRoot* scopeRoot;
    /* Root type which is used in instantiation */
    RootType* typeRoot;
    /* Instantiated root var */
    const RootVar* varRoot;
    
    /* Pointer to UUID(if it is defined for trace )*/
    UUID* uuid;

    /* Trace cached data */
    const CTFVarInt* varMagic;
    const CTFVar* varUUID;

    friend class CTFReaderBuilder;
    friend class PacketContext;
};

class CTFReader::Packet: public CTFContext
{
public:
    /*
     * First packet in the stream.
     */
    Packet(const CTFReader& reader, std::istream& s);
    /* Copy packet */
    Packet(const Packet& packet);
    ~Packet();
    /*
     * Move to the next packet in the stream.
     * 
     * Return true on success and false if current packet is last in the stream.
     */
    bool next(void);

    /* Return size(in bits) of the packet */
    uint32_t getPacketSize(void);
    /* Return size(in bits) of the packet content(without padding) */
    uint32_t getContentSize(void);

    void ref(void) {refs++;}
    void unref(void) {if(--refs == 0) delete this;}
protected:
    int extendMapImpl(int newSize, const char** mapStart_p,
        int* mapStartShift_p);
private:
	void setupPacket(void);

	int refs;

   	std::istream& s;
	off_t streamMapStart;

	char* mapStart;
	int mapSize;
    
    const CTFReader::RootVar* rootVar;
    /* Contain cached values for trace */
    const CTFReader& reader;
    /* Cached values for stream*/
    const CTFVarInt* packetSizeVar;
    const CTFVarInt* contentSizeVar;
    
    friend class CTFReader::Event;
};

class CTFReader::Event: public CTFContext
{
public:
    /* First event in the packet */
    Event(Packet& packet);
    /* Copy event */
    Event(const Event& event);
    ~Event();

    Packet& getPacket(void) const {return *packet;}
    /* 
     * Move to the next event in the stream. 
     * 
     * Return true on success and false if given event is last in the stream.
     */
    bool next(void);
    /* 
     * Move to the next event in the packet. 
     * 
     * Return true on success and false if given event is last in the packet.
     */
    bool nextInPacket(void);

	void ref(void) {refs++;}
	void unref(void) {if(--refs == 0) delete this;}

protected:
    int extendMapImpl(int newSize, const char** mapStart_p,
        int* mapStartShift_p);
private:
    int refs;
    
    /* Allocated map, covered all events in the packet. */
    char* map;
    /* 
     * Size of map.
     * 
     * For reuse map allocation for next packet without realloc().
     */
    int mapSize;
    /* End offset of the last event */
    int eventsEndOffset;

    const CTFReader::RootVar* rootVar;
    
    Packet* packet;
    
    void beginPacket(void);
};


/* Iterator through packets in the stream */
class CTFReader::PacketIterator
{
public:
    /* Create past-the-end iterator*/
    PacketIterator() : packet(NULL) {}
    /* Create iterator pointed to the first packet in the stream */
    PacketIterator(const CTFReader& reader, std::istream& s)
        : packet(new Packet(reader, s)) {}
    /* Copy iterator */
    PacketIterator(const PacketIterator& packetIterator)
        : packet(packetIterator.packet) {if(packet) packet->ref();}
    
    ~PacketIterator(void) {if(packet) packet->unref();}
    
    PacketIterator& operator=(const PacketIterator& iter)
    {
        if(iter.packet) iter.packet->ref();
        if(packet) packet->unref();
        packet = iter.packet;
        return *this;
    }

    /*
     * Clone iterator.
     *
     * Created iterator may be used independently from given one.
     */
    PacketIterator clone(void) const
        {PacketIterator iter; iter.packet = new Packet(*packet); return iter;}

    /* Common iterator declarations and methods */
    typedef int                         difference_type;
    typedef std::forward_iterator_tag   iterator_category;
    typedef Packet                      value_type;
    typedef Packet&                     reference_type;
    typedef Packet*                     pointer_type;

    /* Iterators are compared via their bool representation */
    operator bool(void) const {return packet != NULL;}

    reference_type operator*(void) const
        { return *packet;}
    pointer_type operator->(void) const
        { return packet;}

    PacketIterator& operator++(void)
        {if(!packet->next()) {packet->unref(); packet = NULL;} return *this;}
private:
    Packet* packet;
    friend class CTFReader::EventIterator;
    friend class CTFReader::PacketEventIterator;
};


/* Iterator through events in the packet */
class CTFReader::PacketEventIterator
{
public:
    /* Create past-the-end iterator*/
    PacketEventIterator() : event(NULL) {}
    /* Create iterator which points to the first event in the packet */
    PacketEventIterator(const PacketIterator& packetIterator)
        : event(new Event(*packetIterator.packet)) {}
    
    PacketEventIterator(const PacketEventIterator& eventIterator)
        : event(eventIterator.event) {if(event) event->ref();}
    ~PacketEventIterator(void) {if(event) event->unref();}

    PacketEventIterator& operator=(const PacketEventIterator& iter)
    {
        if(iter.event) iter.event->ref();
        if(event) event->unref();
        event = iter.event;
        return* this;
    }

    /*
     * Clone iterator.
     *
     * Created iterator may be used independently from given one.
     */
    PacketEventIterator clone(void) const
        {PacketEventIterator iter; iter.event = new Event(*event); return iter;}

    /* Common iterator declarations and methods */
    typedef int                         difference_type;
    typedef std::forward_iterator_tag   iterator_category;
    typedef Event                       value_type;
    typedef Event&                      reference_type;
    typedef Event*                      pointer_type;

    /* Iterators are compared via their bool representation */
    operator bool(void) const {return event != NULL;}

    reference_type operator*(void) const { return *event;}
    pointer_type operator->(void) const { return event;}

    PacketEventIterator& operator++(void)
        {if(!event->nextInPacket()) {event->unref(); event = NULL;} return *this;}
private:
    Event* event;
};


/* Iterator through events in the stream */
class CTFReader::EventIterator
{
public:
    /* Create past-the-end iterator*/
    EventIterator() : event(NULL) {}
    /* Create iterator points to the first event in the stream */
    EventIterator(const CTFReader& reader, std::istream& s)
    {
        Packet* packet = new Packet(reader, s);
        event = new Event(*packet);
        packet->unref();
    }
    
    EventIterator(const EventIterator& eventIterator)
        : event(eventIterator.event) {if(event) event->ref();}
    ~EventIterator(void) {if(event) event->unref();}

    EventIterator& operator=(const EventIterator& iter)
    {
        if(iter.event) iter.event->ref();
        if(event) event->unref();
        event = iter.event;
        return* this;
    }


    /*
     * Clone iterator.
     *
     * Created iterator may be used independently from given one.
     */
    EventIterator clone(void) const
        {EventIterator iter; iter.event = new Event(*event); return iter;}

    /* Common iterator declarations and methods */
    typedef int                         difference_type;
    typedef std::forward_iterator_tag   iterator_category;
    typedef Event                       value_type;
    typedef Event&                      reference_type;
    typedef Event*                      pointer_type;


    /* Iterators are compared via their bool representation */
    operator bool(void) const {return event != NULL;}

    reference_type operator*(void) const
        { return *event;}
    pointer_type operator->(void) const
        { return event;}

    EventIterator& operator++(void)
        {if(!event->next()) {event->unref(); event = NULL;} return *this;}
private:
    Event* event;
};


class CTFReader::MetaPacket
{
public:
    /* First metapacket in the stream */
    MetaPacket(std::istream& s);
    ~MetaPacket(void);
    
    /* Pointer to metadata chunk, contained in the packet */
    const char* getMetadata(void) const;
    /* Size of metadata chunk contained in the packet */
    size_t getMetadataSize(void) const;

    /* Return byte order used in the packet */
    CTFTypeInt::ByteOrder getByteOrder(void) const;
    /* Return size(in bits) of the packet*/
    uint32_t getPacketSize(void) const;
    /* Return size(in bits) of the packet content */
    uint32_t getContentSize(void) const;
    /* Return UUID of the meta packet */
    const UUID* getUUID(void) const;

    /* 
     * Move to the next metapacket in the stream.
     * 
     * Return true on success and false if packet is last in the stream.
     */
    bool next(void);

    /* Header of metadata; from CTF specification "as is" */
    struct Header
    {
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
        
        char data[0];
    };

    static const uint32_t magicValue = 0x75D11D57;
    static const uint8_t majorValue = 1;
    static const uint8_t minorValue = 8;

    static const int headerSize = offsetof(Header, data);

    void ref(void) {refs++;}
    void unref(void) {if(--refs == 0) delete this;}
private:
    MetaPacket(const MetaPacket& metaPacket); /* not implemented */
    
    int refs;
    
  	std::istream& s;
	off_t streamMapStart;

    char* metadata;
    size_t metadataSize;
    /* For reuse metadata buffer without realloc. */
    size_t metadataMaxSize;
    
    struct Header header;
    
    UUID uuid;
    
    void setupMetaPacket(void);
};

/* Iterator through packets of metadata */
class CTFReader::MetaPacketIterator
{
public:
    /* Create past-the-end iterator */
    MetaPacketIterator(void) : metaPacket(NULL) {}
    /* Create iterator pointed to the first packet in the stream */
    MetaPacketIterator(std::istream& s)
        : metaPacket(new MetaPacket(s)) {}
    
    MetaPacketIterator(const MetaPacketIterator& iter)
        : metaPacket(iter.metaPacket) {if(metaPacket) metaPacket->ref();}
    
    ~MetaPacketIterator(void) {if(metaPacket) metaPacket->unref();}
    
    MetaPacketIterator& operator=(const MetaPacketIterator& iter)
    {
        if(iter.metaPacket) iter.metaPacket->ref();
        if(metaPacket) metaPacket->unref();
        metaPacket = iter.metaPacket;
        return *this;
    }

    /* Standard iterator types and members */
    typedef int                         difference_type;
    typedef std::forward_iterator_tag   iterator_category;

    typedef MetaPacket                  value_type;
    typedef MetaPacket&                 reference_type;
    typedef MetaPacket*                 pointer_type;

    operator bool(void) const {return metaPacket != NULL;}

    reference_type operator*(void) const
        { return *metaPacket;}
    pointer_type operator->(void) const
        { return metaPacket;}

    MetaPacketIterator& operator++(void)
        {if(!metaPacket->next()) {metaPacket->unref(); metaPacket = NULL;} return *this;}
private:
    MetaPacket* metaPacket;
};


#endif /* CTF_READER_H_INCLUDED */
