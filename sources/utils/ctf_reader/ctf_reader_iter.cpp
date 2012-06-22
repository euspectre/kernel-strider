/* Implementation of the Packets and Events and their iterators */

#include <kedr/ctf_reader/ctf_reader.h>
#include "ctf_root_type.h"

#include <cerrno> /* errno */

#include <fcntl.h> /* open */
#include <unistd.h> /* other file-related operations, like llseek, read */

#include <stdexcept> /* standard exceptions */

#include <cstdlib> /* in C++ malloc is defined here */

#include <iostream> /* stream for output errors */

#include <cassert> /* assert */

#include <sstream> /* ostringstream */

#include <cstring> /* memcpy */

#include <endian.h> /* process endianess in packets with metadata*/

/*
 * Read given number of bytes from stream started at given offset.
 * 
 * If cannot read all bytes for any reason, throw exception.
 */
void readFromStreamAt(std::istream& s,
	char* buf, size_t size, off_t offset)
{
	if(!s.seekg(offset, std::ios_base::beg))
	{
		std::cerr << "Failed to set position in the stream to "
            << offset << ".\n";
		throw std::runtime_error("Failed to set position in the stream");
	}

	if(!s.read(buf, size))
	{
		std::cerr << "Failed to read " << size << " bytes from stream at "
			<< offset << " offset.\n";
		throw std::runtime_error("Failed to read from stream");
	}
	
	if(s.gcount() < (int)size)
	{
		std::cerr << "EOF occure before read " << size << " bytes from stream at "
			<< offset << " offset.\n";
		throw std::runtime_error("Failed to read from stream");
	}
}

/*
 * Test whether stream ends at given offset.
 */
bool isStreamEnds(std::istream& s, off_t offset)
{
	if(!s.seekg(offset, std::ios_base::beg))
	{
		std::cerr << "Failed to set position in the stream to "
            << offset << ".\n";
		throw std::runtime_error("Failed to set position in the stream");
	}
	
	return s.peek() == std::istream::traits_type::eof();
}



/* Event */

/*
 * 'map' - allocated(via malloc) array of context's bytes.
 * 
 * 'unmappedBytes' is number of first bytes of context, which
 * are not mapped. That is, mapping 'map' starts from 'unmappedBytes'
 * byte in the context.
 */

CTFReader::Event::Event(Packet& packet) :
    CTFContext(packet.rootVar->eventContextVar, &packet),
    refs(1), map(NULL), mapSize(0),
    rootVar(packet.rootVar), packet(packet)
{
    beginPacket();
    packet.ref();
}

int CTFReader::Event::extendMapImpl(int newSize, const char** mapStart_p,
    int* mapStartShift_p)
{
    (void)newSize;
    (void)mapStart_p;
    (void)mapStartShift_p;
    
    std::cerr << "Requested extension of event context to "
        << newSize << " while it has size " << CTFContext::mapSize() << " .\n";
    throw std::logic_error("Extension of event context shouldn't be requested");
}

CTFReader::Event::Event(const Event& event) :
    CTFContext(event.getContextVar(), event.getBaseContext()),
    refs(1), eventsEndOffset(event.eventsEndOffset),
    rootVar(event.rootVar), packet(event.packet)
{
    /* 
     * Copied context is fully mapped.
     * 
     * So need only copy its mapping(and copy size of EventStart variable).
     * 
     * NOTE: Use fact, that mapping shift is 0.
     */
    int eventStartOffset = rootVar->eventStartVar->
        getEventStart(event);
    /* Effective start of the mapping(int bytes). All bytes before it are unused.*/
    int effectiveStartOffsetBytes = eventStartOffset / 8;
    /* Effective end of the mapping. Because cannot copy part of byte.*/
    int effectiveEndOffsetBytes = (eventsEndOffset + 7) / 8;
    
    mapSize = effectiveEndOffsetBytes - effectiveStartOffsetBytes;
    map = (char*)malloc(mapSize);
    if(!map) throw std::bad_alloc();
    
    memcpy(map, event.mapStart() + effectiveStartOffsetBytes, mapSize);
    
    moveMap(event.CTFContext::mapSize(), map - effectiveStartOffsetBytes, 0);
    
    rootVar->eventStartVar->setEventStart(eventStartOffset, *this);
    
    packet.ref();
}

CTFReader::Event::~Event(void)
{
    packet.unref();
    free(map);
}

CTFReader::Event* CTFReader::Event::nextInPacket(void)
{
    if(tryNextInPacket())
    {
        return this;
    }
    else
    {
        unref();
        return NULL;
    }
}

bool CTFReader::Event::tryNextInPacket(void)
{
    int nextEventStartOffset = rootVar->eventLastVar->getEndOffset(*this);
    
    if(nextEventStartOffset < eventsEndOffset)
    {
        setMap(CTFContext::mapSize(), mapStart(), mapStartShift());
        
        rootVar->eventStartVar->setEventStart(nextEventStartOffset, *this);

        return true;
    }
    else
    {
        if(nextEventStartOffset != eventsEndOffset)
		{
			std::cerr << "End offset of last event in the packet is "
				<< nextEventStartOffset << ", but all event should ends at "
				<< eventsEndOffset << ".\n";
			throw std::logic_error("Incorrect CTF packet");
		}
		return false;
    }
}


CTFReader::Event* CTFReader::Event::next(void)
{
    if(tryNextInPacket())
    {
        return this;
    }
    else
    {
        setMap(0, NULL, 0);

        /* 
         * Last event in the packet. Advance packet,
         * then extract first event from it.
         */
        if(packet.tryNext())
        {
            beginPacket();
            return this;
        }
        else
        {
            /* Packet is last */
            unref();
            return NULL;
        }
    }
}

void CTFReader::Event::beginPacket(void)
{
    int eventsStartOffset = rootVar->packetLastVar->getEndOffset(packet);
    assert(eventsStartOffset != -1);
    
    eventsEndOffset = packet.contentSizeVar->getInt32(packet);
    if(eventsEndOffset <= eventsStartOffset)
        std::logic_error("Non-positive size of packet content.");
    
    int mapStartOffset = eventsStartOffset / 8;
    
    int mapSizeNew = (eventsEndOffset + 7) / 8 - mapStartOffset;
    if(mapSize < mapSizeNew)
    {
        free(map);
        map = (char*)malloc(mapSizeNew);
        if(map == NULL)
        {
            mapSize = 0;
            //TODO: some stable object state if possible.
            throw std::bad_alloc();
        }
        mapSize = mapSizeNew;
    }
    readFromStreamAt(packet.s, map, mapSizeNew,
        packet.streamMapStart + mapStartOffset);
    
    moveMap(eventsEndOffset, map - mapStartOffset, 0);
    rootVar->eventStartVar->setEventStart(eventsStartOffset, *this);
}

/* Event start variable */
void EventStartVar::setEventStart(int eventStartOffset,
    CTFContext& context) const
{
    assert(context.getContextVar() == getVarPlace()->getContextVar());
    
    CTFContext* contextAdjusted = adjustContext(context);
    assert(contextAdjusted);
    
    *contextAdjusted->getCache(eventStartIndex) = eventStartOffset;
}

void EventStartVar::onPlaceChanged(const CTFVarPlace* placeOld)
{
    if(placeOld)
    {
        placeOld->getContextVar()->cancelCacheReservation(eventStartIndex);
    }
    const CTFVarPlace* varPlace = getVarPlace();
    if(varPlace)
    {
        eventStartIndex = varPlace->getContextVar()->reserveCache();
    }
}

int EventStartVar::getEventStart(const CTFContext& context) const
{
    const CTFContext* contextAdjusted = adjustContext(context);
    if(contextAdjusted == NULL) return -1;
    int eventStart = *contextAdjusted->getCache(eventStartIndex);
    assert(eventStart != -1);

    return eventStart;
}

/* Packet */
CTFReader::Packet::Packet(const CTFReader& reader, std::istream& s)
	: CTFContext(reader.varRoot->packetContextVar), refs(1),
    s(s), streamMapStart(0), mapStart(NULL), mapSize(0),
    rootVar(reader.varRoot), reader(reader)
{
	setupPacket();

    const CTFVar* packetSizeVarBase = rootVar->findVar
		("stream.packet.context.packet_size");
	if(packetSizeVarBase == NULL)
		throw std::logic_error
			("Cannot determine size of packets in the stream");
	if(!packetSizeVarBase->isInt())
		throw std::logic_error
			("Type of variable contained packet size is not integer");
	packetSizeVar = static_cast<const CTFVarInt*>(packetSizeVarBase);
	
	const CTFVar* contentSizeVarBase = rootVar->findVar
		("stream.packet.context.content_size");
	if(contentSizeVarBase != NULL)
	{
		if(!contentSizeVarBase->isInt())
			throw std::logic_error
			("Type of variable contained content size is not integer");
		contentSizeVar = static_cast<const CTFVarInt*>
			(contentSizeVarBase);
	}
	else
	{
		contentSizeVar = packetSizeVar;
	}
}

CTFReader::Packet::Packet(const Packet& packet) :
    CTFContext(packet.getContextVar()), refs(1),
    s(packet.s), streamMapStart(packet.streamMapStart),
    mapSize(packet.mapSize),
    rootVar(rootVar),
    reader(packet.reader),
    packetSizeVar(packet.packetSizeVar),
    contentSizeVar(packet.contentSizeVar)
{
    if(mapSize)
    {
        mapStart = (char*)malloc(mapSize);
        if(mapStart == NULL) throw std::bad_alloc();
        
        memcpy(mapStart, packet.mapStart, mapSize);
    }
    else
    {
        mapStart = NULL;
    }
    setupPacket();
}

CTFReader::Packet::~Packet(void)
{
    free(mapStart);
}

CTFReader::Packet* CTFReader::Packet::next(void)
{
    if(tryNext())
    {
        return this;
    }
    else
    {
        unref();
        return NULL;
    };
}

bool CTFReader::Packet::tryNext(void)
{
    int packetSize = packetSizeVar->getInt32(*this);
    if(packetSize % 8)
        throw std::logic_error("Size of packet is not multiple of bytes.");
    
    off_t nextStreamMapStart = streamMapStart + packetSize / 8;
    
    if(isStreamEnds(s, nextStreamMapStart))
    {
        /* current packet is last*/
        return false;
    }

    streamMapStart = nextStreamMapStart;
    mapSize = 0;
    
    /* Flush map */
    setMap(0, NULL, 0);
    
    setupPacket();
   
    return true;
}

int CTFReader::Packet::extendMapImpl(int newSize, const char** mapStart_p,
    int* mapStartShift_p)
{
	int mapSizeNew = (newSize +  7) / 8;
    
    char* mapStartNew = (char*)realloc(mapStart, mapSizeNew);
	if(mapStartNew == NULL)
		throw std::bad_alloc();
    
    mapStart = mapStartNew;

	readFromStreamAt(s, mapStart + mapSize,
        mapSizeNew - mapSize, streamMapStart + mapSize);
    
    mapSize = mapSizeNew;
    
    *mapStart_p = mapStart;
    *mapStartShift_p = 0;
    
    return mapSize * 8;
}

void CTFReader::Packet::setupPacket(void)
{
	const CTFVarInt* varMagic = reader.varMagic;
	if(varMagic)
	{
		varMagic->map(*this);
		uint32_t magic = varMagic->getUInt32(*this);
		if(magic != 0xC1FC1FC1)
		{
			std::cerr << "Magic value for the packet is "
				<< std::hex << std::uppercase << magic
				<< ", but should be " << 0xC1FC1FC1 << std::endl;
			throw std::invalid_argument("Invalid magic number for packet");
		}
	}
	const UUID* uuid = reader.uuid;
	const CTFVar* varUUID = reader.varUUID;
	if(uuid && varUUID)
	{
		varUUID->map(*this);
		UUID uuidPacket((const unsigned char*)varUUID->getMap(*this, NULL));
		if(uuidPacket != *uuid)
		{
			std::cerr << "Trace UUID in packet (" << uuidPacket
				<<") differs from one in metadata (" << *uuid
				<< ").\n";
			throw std::invalid_argument("Invalid trace UUID for packet");
		}
	}
	
	rootVar->packetLastVar->map(*this);
}

uint32_t CTFReader::Packet::getPacketSize(void)
{
	return packetSizeVar->getInt32(*this);
}

uint32_t CTFReader::Packet::getContentSize(void)
{
	return contentSizeVar->getInt32(*this);
}

/* Packet with metadata */

#ifndef _BSD_SOURCE
#error _BSD_SOURCE feature needs for correct processing byte order.
#endif

CTFTypeInt::ByteOrder CTFReader::MetaPacket::getByteOrder(void) const
{
	uint32_t metaMagic = ((CTFMetadataPacketHeader*)header)->magic;
	return (le32toh(metaMagic) == CTFMetadataPacketHeader::magicValue)
		? CTFTypeInt::le : CTFTypeInt::be;
}

uint32_t CTFReader::MetaPacket::getPacketSize(void) const
{
	uint32_t packetSize = ((CTFMetadataPacketHeader*)header)->packet_size;
	
	if(getByteOrder() == CTFTypeInt::le)
		return le32toh(packetSize);
	else
		return be32toh(packetSize);
}

uint32_t CTFReader::MetaPacket::getContentSize(void) const
{
	uint32_t contentSize = ((CTFMetadataPacketHeader*)header)->content_size;
	
	if(getByteOrder() == CTFTypeInt::le)
		return le32toh(contentSize);
	else
		return be32toh(contentSize);
}

void CTFReader::MetaPacket::setupMetaPacket(void)
{
	readFromStreamAt(s, header, sizeof(header), streamMapStart);
	uint32_t metaMagic = ((CTFMetadataPacketHeader*)header)->magic;
	
	if((be32toh(metaMagic) != CTFMetadataPacketHeader::magicValue)
		&& (le32toh(metaMagic) != CTFMetadataPacketHeader::magicValue))
	{
		std::ios_base::fmtflags oldFlags
			= std::cerr.setf(std::ios_base::hex, std::ios_base::basefield);
		std::cerr << "Magic field in metadata packet " << metaMagic
			<< " doesn't correspond to "
			<< (uint32_t)CTFMetadataPacketHeader::magicValue
			<< " in any byte order.\n";
		std::cerr.setf(oldFlags, std::ios_base::basefield);
		
		throw std::logic_error("Incorrect magic field in metadata packet");
	}
	
	uint32_t contentSize = getContentSize();
	
	if(contentSize % 8)
	{
		std::cerr << "Size of metadata content is " << contentSize <<
			" ,that is not multiple of bytes.\n";
		throw std::logic_error("Invalid content size of metadata");
	}
	else if(contentSize <= sizeof(header) * 8)
	{
		std::cerr << "Size of metadata content is " << contentSize <<
			" ,that is not more than size of header.\n";
		throw std::logic_error("Invalid content size of metadata");
	}
	
	size_t metadataSizeNew = contentSize / 8 - sizeof(header);
	if(metadataMaxSize < metadataSizeNew)
	{
		free(metadata);
		metadata = (char*)malloc(metadataSizeNew);
		if(metadata == NULL) throw std::bad_alloc();
		metadataMaxSize = metadataSizeNew;
	}

	metadataSize = metadataSizeNew;
	
	readFromStreamAt(s, metadata, metadataSize, streamMapStart + sizeof(header));
}

CTFReader::MetaPacket::MetaPacket(std::istream& s)
	: refs(1), s(s), streamMapStart(0),
	metadata(NULL), metadataSize(0), metadataMaxSize(0),
	uuid((const unsigned char*)((CTFMetadataPacketHeader*)header)->uuid)
{
	setupMetaPacket();
}

CTFReader::MetaPacket::~MetaPacket(void)
{
	free(metadata);
}

CTFReader::MetaPacket* CTFReader::MetaPacket::next(void)
{
	uint32_t packetSize = getPacketSize();
	uint32_t contentSize = getContentSize();
	
	if(packetSize < contentSize)
	{
		std::cerr << "Size of packet of metadata is " << packetSize
			<< " , but size of content is " << contentSize << " .\n";
		throw std::logic_error("Invalid size of metadata packet");
	}
	else if(packetSize % 8)
	{
		std::cerr << "Size of metadata packet is " << contentSize <<
			" ,that is not multiple of bytes.\n";
		throw std::logic_error("Invalid size of metadata packet");
	}

	off_t streamMapStartNew = streamMapStart + packetSize / 8;
	
	if(isStreamEnds(s, streamMapStartNew))
	{
		unref();
		return NULL;
	}
	else
	{
		streamMapStart = streamMapStartNew;
		setupMetaPacket();
		return this;
	}
}

const UUID* CTFReader::MetaPacket::getUUID(void) const
{
	return &uuid;
}

const char* CTFReader::MetaPacket::getMetadata(void) const
{
	return metadata;
}

size_t CTFReader::MetaPacket::getMetadataSize(void) const
{
	return metadataSize;
}

