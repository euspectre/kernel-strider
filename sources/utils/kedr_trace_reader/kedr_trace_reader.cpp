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
	dirname(dirname)
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
}

class KEDRTraceReader::KEDRStream
{
public:
	/* Create stream from given file */
	KEDRStream(const KEDRTraceReader& traceReader, const std::string& filename);
	
	/* Return standard stream */
	std::istream& getStream(void) {return file;}
	
	void ref(void) {++refs;}
	void unref(void) {if(--refs == 0) delete this;}

private:
	/* Object must be created in the heap */
	~KEDRStream(void) {}

	int refs;

	std::ifstream file;
	/* Events-ordering related variables */
	const CTFVarInt* timestampVar;
	const CTFVarInt* counterVar;
	
	const KEDRTraceReader& traceReader;
	
	friend class KEDRTraceReader;
};

KEDRTraceReader::KEDRStream::KEDRStream(
	const KEDRTraceReader& traceReader, const std::string& filename)
	: refs(1), file(filename.c_str()), traceReader(traceReader)
{
	if(!file)
	{
		std::cerr << "Failed to open stream file '" << filename << "'.\n";
		throw std::runtime_error("Failed to open stream file");
	}
	
	//debug
	std::cerr << "Open KEDR trace stream file '" << filename << "'.\n";
	
	timestampVar = &findInt(traceReader, "stream.event.context.timestamp");
	counterVar = &findInt(traceReader, "stream.event.context.counter");
}

bool KEDRTraceReader::isEventOlder(Event& event1, KEDRStream& stream1,
	Event& event2, KEDRStream& stream2)
{
	assert(&stream1.traceReader == &stream2.traceReader);
	
	const KEDRTraceReader& traceReader = stream1.traceReader;
	
	uint64_t timestamp1 = stream1.timestampVar->getUInt64(event1);
	uint64_t timestamp2 = stream2.timestampVar->getUInt64(event2);
	
	if(timestamp1 > timestamp2 + traceReader.time_precision)
		return false;
	else if(timestamp2 > timestamp1 + traceReader.time_precision)
		return true;
	else
	{
		int32_t counter1 = stream1.counterVar->getInt32(event1);
		int32_t counter2 = stream2.counterVar->getInt32(event2);
		// TODO: process case when not all bits in counter are meaningfull.
		return counter1 - counter2 < 0;
	}
}

void KEDRTraceReader::EventIterator::reorderLast(void)
{
	Event* event = streamEvents.back().first;
	KEDRStream* kedrStream = streamEvents.back().second;

	/* 
	 * Element should be inserted into one of position in
	 * [pos_first, pos_last] range.
	 */
	int pos_first = 0;
	int pos_last = streamEvents.size() - 1;
	
	while(pos_first < pos_last)
	{
		int pos = (pos_first + pos_last) / 2; /* NOTE: < pos_last */
		
		if(KEDRTraceReader::isEventOlder(*event, *kedrStream,
			*streamEvents[pos].first, *streamEvents[pos].second))
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
		/* Shift all elements after insertion position */
		for(int i = (int)streamEvents.size() - 1; i > pos_first; i--)
			streamEvents[i] = streamEvents[i - 1];

		streamEvents[pos_first].first = event;
		streamEvents[pos_first].second = kedrStream;
	}
}

KEDRTraceReader::EventIterator::EventIterator(void) {}

KEDRTraceReader::EventIterator::EventIterator(const KEDRTraceReader& traceReader)
{
	DIR* dir = opendir(traceReader.dirname.c_str());
	if(!dir)
	{
		std::cerr << "Failed to open trace directory '"
			<< traceReader.dirname << "': " << strerror(errno) << ".\n";
		throw std::runtime_error("Failed to open trace directory");
	}
	for(struct dirent* entry = readdir(dir); entry != NULL; entry = readdir(dir))
	{
		if(entry->d_type != DT_REG) continue;/* Not a regular file */
		/* Open file and check, that it starts with CTF magic number */
		std::string streamFilename = traceReader.dirname + "/" + entry->d_name;
		int streamFD = open(streamFilename.c_str(), O_RDONLY);
		if(streamFD == -1)
		{
			std::cerr << "Failed to open file '" << streamFilename
				<< "' in trace directory. Ignored.\n";
			continue;
		}
		uint32_t magic;
		int result = read(streamFD, &magic, sizeof(magic));
		close(streamFD);
		if(result != sizeof(magic)) continue;/* Ignore file in any errors */
		
		if((magic != htobe32(CTFReader::magicValue))
			&& (magic != htole32(CTFReader::magicValue))) continue;
		
		/* File contains stream */
		
		KEDRStream* kedrStream = new KEDRStream(traceReader, streamFilename);
		Packet* packet = new Packet(traceReader, kedrStream->getStream());
		Event* event = new Event(*packet);
		packet->unref();
		
		streamEvents.push_back(std::make_pair(event, kedrStream));
		reorderLast();
	}
	closedir(dir);
}

KEDRTraceReader::EventIterator::EventIterator(const EventIterator& iter)
	: streamEvents(iter.streamEvents)
{
	for(int i = 0; i < (int)streamEvents.size(); i++)
	{
		streamEvents[i].first->ref();
		streamEvents[i].second->ref();
	}
}

KEDRTraceReader::EventIterator::~EventIterator(void)
{
	for(int i = (int)streamEvents.size() - 1; i >= 0 ; --i)
	{
		streamEvents[i].first->unref();
		streamEvents[i].second->unref();
	}
}

KEDRTraceReader::EventIterator& KEDRTraceReader::EventIterator::operator++(void)
{
	Event*& event = streamEvents.back().first;
	event = event->next();
	
	if(event)
	{
		reorderLast();
	}
	else
	{
		streamEvents.back().second->unref();
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
		Event*& event = result.streamEvents[i].first;
		Event* eventCopy = new Event(*event);
		
		event->unref();
		event = eventCopy;
	}
	
	return result;
}