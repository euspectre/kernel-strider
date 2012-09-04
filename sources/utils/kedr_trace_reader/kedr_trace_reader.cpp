#include <kedr/kedr_trace_reader/kedr_trace_reader.h>

#include <iostream>
#include <fstream>
#include <sstream>

#include <stdexcept>

#include <dirent.h> /* for iterate files in directory */

#include <fcntl.h> /* O_RDONLY */
#include <errno.h>

#include <unistd.h> /* read() */

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

KEDRTraceReader::KEDRTraceReader(const std::string& dirname)
	: CTFReader(std::ifstream((dirname + "/metadata").c_str()).seekg(0)),
	dirname(dirname), state(0), stateMask(0)
{
	/* Timestamp precision parameter */
	const std::string* time_precision_str = findParameter("trace.time_precision");
	if(!time_precision_str)
	{
		std::cerr << "'trace.time_precision' parameter is absent for the trace.\n";
		throw std::logic_error("Invalid KEDR trace.");
	}
	
	std::istringstream time_precision_is(*time_precision_str);
	
	time_precision_is >> time_precision;
	if(!time_precision_is
		|| (time_precision_is.peek() != std::istream::traits_type::eof()))
	{
		std::cerr << "Failed to parse 'trace.time_precision' parameter"
			"as 64-bit unsigned integer: " << *time_precision_str << "\n";
		throw std::logic_error("Invalid KEDR trace.");
	}
	
	timestampVar = &findInt(*this, "stream.event.context.timestamp");
	counterVar = &findInt(*this, "stream.event.context.counter");
	lostEventsTotalVar = &findInt(*this, "stream.packet.context.lost_events_total");
	packetCountVar = &findInt(*this, "stream.packet.context.stream_packet_count");
}

void KEDRTraceReader::exceptions(KEDRTraceReader::TraceState except)
{
	TraceState exceptOld = stateMask;
	stateMask = except;
	
	if(!(exceptOld & eventsLostBit)
		&& (stateMask & eventsLostBit)
		&& (state & eventsLostBit))
	{
		/* eventsLost bit become masked while it is in the current state. */
		throw LostEventsException();
	}
}


class KEDRTraceReader::EventIterator::StreamInfo::RefStream
{
public:
	/* Create stream from given file */
	RefStream(const std::string& filename): refs(1), file(filename.c_str())
	{
		if(!file)
		{
			std::cerr << "Failed to open stream file '" << filename << "'.\n";
			throw std::runtime_error("Failed to open stream file");
		}
		
		//debug
		std::cerr << "Open KEDR trace stream file '" << filename << "'.\n";
	}
	
	/* Return standard stream */
	std::istream& getStream(void) {return file;}
	
	void ref(void) {++refs;}
	void unref(void) {if(--refs == 0) delete this;}

private:
	/* Object must be created in the heap */
	~RefStream(void) {}

	int refs;

	std::ifstream file;
};


/* Comparision of timestamps, which takes into account int type overflow */
static inline bool isTimestampAfter(uint64_t ts1, uint64_t ts2)
{
	return ((int64_t)(ts1 - ts2)) > 0;
}

bool KEDRTraceReader::isEventOlder(Event& event1, Event& event2) const
{
	uint64_t timestamp1 = timestampVar->getUInt64(event1);
	uint64_t timestamp2 = timestampVar->getUInt64(event2);
	
	if(isTimestampAfter(timestamp1, timestamp2 + time_precision))
		return false;
	else if(isTimestampAfter(timestamp2, timestamp1 + time_precision))
		return true;
	else
	{
		int32_t counter1 = counterVar->getInt32(event1);
		int32_t counter2 = counterVar->getInt32(event2);
		// TODO: process case when not all bits in counter are meaningfull.
		return counter1 - counter2 < 0;
	}
}

void KEDRTraceReader::setEventsLost(void)
{
	assert(!eventsLost());
	
	state |= eventsLostBit;
	
	//debug
	//std::cerr << "Events lost" << std::endl;
	
	/*if(stateMask & eventsLostBit) */throw LostEventsException();
}

void KEDRTraceReader::EventIterator::reorderLast(void)
{
	StreamInfo& streamInfo = streamEvents.back();

	/* 
	 * Element should be inserted into one of position in
	 * [pos_first, pos_last] range.
	 */
	int pos_first = 0;
	int pos_last = streamEvents.size() - 1;
	
	while(pos_first < pos_last)
	{
		int pos = (pos_first + pos_last) / 2; /* NOTE: < pos_last */
		
		if(traceReader->isEventOlder(*streamInfo.event, *streamEvents[pos].event))
		{
			pos_first = pos + 1;
		}
		else
		{
			pos_last = pos;
		}
	}
	/* Use 'pos_first' as insertion position */
	if(pos_first < (int)streamEvents.size() - 1)
	{
		StreamInfo tmp = streamInfo;
		/* Shift all elements after insertion position */
		for(int i = (int)streamEvents.size() - 1; i > pos_first; i--)
			streamEvents[i] = streamEvents[i - 1];

		streamEvents[pos_first] = tmp;
	}
}

KEDRTraceReader::EventIterator::EventIterator(void) {}

KEDRTraceReader::EventIterator::EventIterator(KEDRTraceReader& traceReader)
	: traceReader(&traceReader)
{
	DIR* dir = opendir(traceReader.dirname.c_str());
	if(!dir)
	{
		std::cerr << "Failed to open trace directory '"
			<< traceReader.dirname << "': " << strerror(errno) << ".\n";
		throw std::runtime_error("Failed to open trace directory");
	}
	try
	{
		for(struct dirent* entry = readdir(dir);
			entry != NULL;
			entry = readdir(dir))
		{
			if(entry->d_type != DT_REG) continue;/* Not a regular file */
			/* Open file and check, that it starts with CTF magic number */
			std::string streamFilename = traceReader.dirname + "/" + entry->d_name;
			int streamFD = open(streamFilename.c_str(), O_RDONLY);
			if(streamFD == -1)
			{
				std::cerr << "Failed to open file '" << streamFilename
					<< "' in trace directory. Ignore.\n";
				continue;
			}
			uint32_t magic;
			int result = read(streamFD, &magic, sizeof(magic));
			close(streamFD);
			/* Ignore file in case of any error */
			if(result != sizeof(magic)) continue;
			
			if((magic != htobe32(CTFReader::magicValue))
				&& (magic != htole32(CTFReader::magicValue))) continue;
			
			/* File contains stream */
			
			StreamInfo::RefStream* refStream =
				new StreamInfo::RefStream(streamFilename);
			Packet* packet =
				new Packet(traceReader, refStream->getStream());
			Event* event = new Event(*packet);
			
			packet->unref();
			
			uint32_t packetCount = traceReader.packetCountVar->getUInt32(*packet);

			StreamInfo streamInfo;
			streamInfo.event = event;
			streamInfo.refStream = refStream;
			streamInfo.packetCounter = packetCount;

			streamEvents.push_back(streamInfo);
			reorderLast();

			if(!traceReader.eventsLost())
			{
				/* Check whether events lost. */
				uint32_t lostEventsTotal =
					traceReader.lostEventsTotalVar->getUInt32(*packet);

				if((packetCount != 0) || (lostEventsTotal != 0))
				{
					traceReader.setEventsLost();
				}
			}
		}
	}
	catch(...)
	{
		closedir(dir);
		for(int i = (int)streamEvents.size() - 1; i >= 0 ; --i)
		{
			streamEvents[i].refStream->unref();
			streamEvents[i].event->unref();
		}
		
		throw;
	}
	closedir(dir);
}

KEDRTraceReader::EventIterator::EventIterator(const EventIterator& iter)
	: traceReader(iter.traceReader), streamEvents(iter.streamEvents)
{
	for(int i = 0; i < (int)streamEvents.size(); i++)
	{
		streamEvents[i].refStream->ref();
		streamEvents[i].event->ref();
	}
}

KEDRTraceReader::EventIterator::~EventIterator(void)
{
	for(int i = (int)streamEvents.size() - 1; i >= 0 ; --i)
	{
		streamEvents[i].refStream->unref();
		streamEvents[i].event->unref();
	}
}

KEDRTraceReader::EventIterator& KEDRTraceReader::EventIterator::operator++(void)
{
	StreamInfo& streamInfo = streamEvents.back();
	Event* event = streamInfo.event;
	
	if(event->nextInPacket())
	{
		/* Packet is not changed. Needn't to check events lost. */
		reorderLast();
	}
	else if(event->next())
	{
		/* Packet changed. */
		Packet* packetNew = &event->getPacket();
			
		uint32_t packetCountOld = streamInfo.packetCounter;
		
		uint32_t packetCountNew	=
			traceReader->packetCountVar->getUInt32(*packetNew);
		
		streamInfo.packetCounter = packetCountNew;

		reorderLast();
		
		if(!traceReader->eventsLost())
		{
			/* Check whether events lost. */
			uint32_t lostEventsTotal =
				traceReader->lostEventsTotalVar->getUInt32(*packetNew);

			if(lostEventsTotal != 0)
			{
				//debug
				std::cerr << "Lost events before packet ends: "
					<< lostEventsTotal << "." << std::endl;
				traceReader->setEventsLost();
			}
			else if(packetCountNew != packetCountOld + 1)
			{
				//debug
				std::cerr << "Lost packets between " << packetCountOld
					<< " and " << packetCountNew << "." << std::endl;
				traceReader->setEventsLost();
			}
		}
	}
	else
	{
		/* Event is last in the stream. Destroy current stream. */
		streamInfo.refStream->unref();
		streamEvents.resize(streamEvents.size() - 1);
	}
	return *this;
}

KEDRTraceReader::EventIterator KEDRTraceReader::EventIterator::clone(void) const
{
	/* Simple copy...*/
	EventIterator result = *this;
	/* ... but replace events with their deep copies */
	for(int i = 0; i < (int)result.streamEvents.size(); i++)
	{
		Event*& event = result.streamEvents[i].event;
		Event* eventCopy = new Event(*event);
		
		event->unref();
		event = eventCopy;
	}
	
	return result;
}