/* 
 * Test that reader may be created from simple metadata 
 * and that packet and event iterators works on simple stream.
 */

#include <kedr/ctf_reader/ctf_reader.h>

#include <stdexcept>
#include <cassert>
#include <iostream>
#include <fstream>

#include <endian.h>

#include <sstream>

#include <cassert>

std::string sourceDir;

static int test1(void);
static int test2(void);
static int test3(void);
static int test4(void);

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

static const CTFVarArray& findArray(const CTFReader& reader, const std::string& name)
{
	const CTFVar* var = reader.findVar(name);
	if(var == NULL)
	{
		std::cerr << "Failed to find array-like variable '" << name << "'.\n";
		throw std::invalid_argument("Invalid variable name");
	}
	if(!var->isArray())
	{
		std::cerr << "Variable with name '" << name << "' is not array-like.\n";
		throw std::invalid_argument("Invalid variable type");
	}
	return *static_cast<const CTFVarArray*>(var);
}



/* 
 * If test is running not from its source directory,
 * first argument should contain this source directory.
 */
int main(int argc, char *argv[])
{
	if(argc > 1)
	{
		sourceDir = argv[1];
		sourceDir += '/';
	}

	int result;

#define RUN_TEST(test_func, test_name) do {\
    try {result = test_func(); }\
	catch(std::exception& e) \
	{ \
		std::cerr << "Exception occures in '" << test_name << "': " \
			<< e.what() << "." << std::endl; \
		return 1; \
    } \
    if(result) return result; \
}while(0)

	RUN_TEST(test1, "simple");
	RUN_TEST(test2, "complex");
    RUN_TEST(test3, "cross-packet");
    RUN_TEST(test4, "iterator-cloning");

	return 0;
}


int test1(void)
{
	std::string metaFilename = sourceDir + "metadata1";
	std::string streamFilename = sourceDir + "data1";
	
	std::ifstream fs;
	fs.open(metaFilename.c_str());
	if(!fs)
	{
		std::cerr << "Failed to open file '" << metaFilename
		<< "' with metadata." <<std::endl;
		return 1;
	}
	
	CTFReader reader(fs);
	
	const CTFVarInt& varEventField = findInt(reader, "event.fields");
	
	std::ifstream fsStream;
	fsStream.open(streamFilename.c_str());
	if(!fsStream)
	{
		std::cerr << "Failed to open file '" << streamFilename
		<< "' with stream data." <<std::endl;
		return 1;
	}

	int packetNumber = 0;
	for(CTFReader::PacketIterator packet(reader, fsStream);
		packet != CTFReader::PacketIterator();
		++packet, ++packetNumber)
	{
		int eventNumber = 0;
		for(CTFReader::PacketEventIterator event(packet);
			event != CTFReader::PacketEventIterator();
			++event, ++eventNumber)
		{
			int32_t eventFieldVal = varEventField.getInt32(*event);
			int32_t eventFieldVal_expected = eventNumber + 1;
			if(eventFieldVal != eventFieldVal_expected)
			{
				std::cerr << "Expected, that value of event "
					<< eventNumber << " will be " << eventFieldVal_expected
					<< "." << std::endl;
				std::cerr << "But it is " << eventFieldVal << "." << std::endl;
				
				return 1;
			}
		}
		if(eventNumber != 5)
		{
			std::cerr << "Expected, that packet will contain 5 events."
				<< "But it contains " << eventNumber << "." << std::endl;
			return 1;
		}

	}
	if(packetNumber != 1)
	{
		std::cerr << "Expected, that only one packet will be in the stream."
			<< "But it is " << packetNumber << std::endl;
		return 1;
	}
	
	return 0;
}

int test2(void)
{
	std::string metaFilename = sourceDir + "metadata2";
	std::string streamFilename = sourceDir + "data2";
	
	std::ifstream fs;
	fs.open(metaFilename.c_str());
	if(!fs)
	{
		std::cerr << "Failed to open file '" << metaFilename
		<< "' with metadata." <<std::endl;
		return 1;
	}
	
	CTFReader reader(fs);
	
	const CTFVarEnum& varEventType = findEnum(reader,
		"stream.event.header");
	const CTFVarInt& varEventFieldSimple = findInt(reader,
		"event.fields.simple");
	const CTFVarArray& varEventFieldsComplex = findArray(reader,
		"event.fields.complex");
	const CTFVarInt& varEventFieldComplex = findInt(reader,
		"event.fields.complex[]");
	
	std::ifstream fsStream;
	fsStream.open(streamFilename.c_str());
	if(!fsStream)
	{
		std::cerr << "Failed to open file '" << streamFilename
		<< "' with stream data." <<std::endl;
		return 1;
	}
	
	CTFReader::PacketIterator packet(reader, fsStream);
	/*************** First packet ***************/
	
	CTFReader::PacketEventIterator event(packet);
	
	/********** First event ***********/
	
	std::string eventType = varEventType.getEnum(*event);
	
	if(eventType != "simple")
	{
		std::cerr << "Expected, that type of the first event will be "
			<< "'simple', but it is '" << eventType << "' \n";
		return 1;
	}
	
	assert(varEventFieldSimple.isExist(*event));
	int fieldSimpleVal = varEventFieldSimple.getInt32(*event);
	if(fieldSimpleVal != -1)
	{
		std::cerr << "Expected, that value of field of first event will "
			<< "be -1, but it is " << fieldSimpleVal << " \n";
		return 1;
	}
	
	++event;
	
	/************** Second event *******************/
	
	eventType = varEventType.getEnum(*event);
	
	if(eventType != "complex")
	{
		std::cerr << "Expected, that type of the second event will be "
			<< "'complex', but it is '" << eventType << "' \n";
		return 1;
	}

	assert(varEventFieldsComplex.isExist(*event));
	int nElems = varEventFieldsComplex.getNElems(*event);
	if(nElems != 6)
	{
		std::cerr << "Expected, that number of subfields in second event "
			"will be 6, but it is '" << nElems << std::endl;
		return 1;
	}
	
	int i = 0;
	for(CTFVarArray::ElemIterator iter(varEventFieldsComplex, *event);
		iter; ++iter, ++i)
	{
		int fieldComplexVal = varEventFieldComplex.getInt32(*iter);
		if(fieldComplexVal != (i + 1))
		{
			std::cerr << "Expected, that value of " << i << " subfield "
				"of second event will be " << i + 1 << ", but it is "
				<< fieldComplexVal << std::endl;
			return 1;
		}
	}
	
	if(++event != CTFReader::PacketEventIterator())
	{
		std::cerr << "Expected that second event will be last in the packet,"
			"but it is not so.\n";
		return 1;
	}
	
	
	if(++packet == CTFReader::PacketIterator())
	{
		std::cerr << "Expected that stream will have two packets,"
			"but it contains only one.\n";
		return 1;
	}
	/************* Second packet **************/
	event = CTFReader::PacketEventIterator(packet);
	
	/********* First event, second packet *******/
	eventType = varEventType.getEnum(*event);
	
	if(eventType != "complex")
	{
		std::cerr << "Expected, that type of the first event in the "
			"second packet will be 'complex', but it is '"
			<< eventType << "' \n";
		return 1;
	}
	
	nElems = varEventFieldsComplex.getNElems(*event);
	if(nElems != 0)
	{
		std::cerr << "Expected, that number of subfields in second event "
			"will be 0(it is allowable), but it is '" << nElems << std::endl;
		return 1;
	}
	
	if(++event != CTFReader::PacketEventIterator())
	{
		std::cerr << "Expected that second packet will contain only one "
			"event, but it is not so.\n";
		return 1;
	}


	if(++packet != CTFReader::PacketIterator())
	{
		std::cerr << "Expected that stream will have only two packets,"
			"but it is not so.\n";
		return 1;
	}
	
	
	return 0;
}

/* Similar to test1, but check cross-packets event iterator */
int test3(void)
{
	std::string metaFilename = sourceDir + "metadata3";
	std::string streamFilename = sourceDir + "data3";
	
	std::ifstream fs;
	fs.open(metaFilename.c_str());
	if(!fs)
	{
		std::cerr << "Failed to open file '" << metaFilename
		<< "' with metadata." <<std::endl;
		return 1;
	}
	
	CTFReader reader(fs);
	
	const CTFVarInt& varEventField = findInt(reader, "event.fields");
	
	std::ifstream fsStream;
	fsStream.open(streamFilename.c_str());
	if(!fsStream)
	{
		std::cerr << "Failed to open file '" << streamFilename
		<< "' with stream data." <<std::endl;
		return 1;
	}

    int eventNumber = 0;
	for(CTFReader::EventIterator event(reader, fsStream);
		event != CTFReader::EventIterator();
		++event, ++eventNumber)
	{
		int32_t eventFieldVal = varEventField.getInt32(*event);
		int32_t eventFieldVal_expected = eventNumber + 1;
		if(eventFieldVal != eventFieldVal_expected)
		{
			std::cerr << "Expected, that value of event "
				<< eventNumber << " will be " << eventFieldVal_expected
				<< "." << std::endl;
			std::cerr << "But it is " << eventFieldVal << "." << std::endl;
			
			return 1;
		}
	}
    if(eventNumber != 13)
    {
        std::cerr << "Expected, that stream will contain 13 events."
            << "But it contains " << eventNumber << "." << std::endl;
        return 1;
    }
    
    return 0;
}

/* Check that iterator cloning is work correctly. */
int test4(void)
{
	/* Number of events skipped before cloning */
	static const int eventSkipped = 3;
	
	std::string metaFilename = sourceDir + "metadata3";
	std::string streamFilename = sourceDir + "data3";
	
	std::ifstream fs;
	fs.open(metaFilename.c_str());
	if(!fs)
	{
		std::cerr << "Failed to open file '" << metaFilename
		<< "' with metadata." <<std::endl;
		return 1;
	}
	
	CTFReader reader(fs);
	
	const CTFVarInt& varEventField = findInt(reader, "event.fields");
	
	std::ifstream fsStream;
	fsStream.open(streamFilename.c_str());
	if(!fsStream)
	{
		std::cerr << "Failed to open file '" << streamFilename
		<< "' with stream data." <<std::endl;
		return 1;
	}

    CTFReader::EventIterator event(reader, fsStream);
    int eventNumber = 0;
	/* Skip first 5 events. */
	for(;
		event != CTFReader::EventIterator();
		++event, ++eventNumber)
	{
		if(eventNumber == eventSkipped) break;
	}

	CTFReader::EventIterator eventClone = event.clone();
	/* Check cloned iterator */
	for(;
		eventClone != CTFReader::EventIterator();
		++eventClone, ++eventNumber)
	{
		int32_t eventFieldVal = varEventField.getInt32(*eventClone);
		int32_t eventFieldVal_expected = eventNumber + 1;
		if(eventFieldVal != eventFieldVal_expected)
		{
			std::cerr << "Expected, that value of event "
				<< eventNumber << " in cloned iterator will be "
				<< eventFieldVal_expected << "." << std::endl;
			std::cerr << "But it is " << eventFieldVal << "." << std::endl;
			
			return 1;
		}
	}
    if(eventNumber != 13)
    {
        std::cerr << "Expected, that stream(cloned iterator) will contain 13 events. "
            << "But it contains " << eventNumber << "." << std::endl;
        return 1;
    }
    

	/* Check initial iterator */
	eventNumber = eventSkipped;
	for(;
		event != CTFReader::EventIterator();
		++event, ++eventNumber)
	{
		int32_t eventFieldVal = varEventField.getInt32(*event);
		int32_t eventFieldVal_expected = eventNumber + 1;
		if(eventFieldVal != eventFieldVal_expected)
		{
			std::cerr << "Expected, that value of event "
				<< eventNumber << " in initial iterator will be "
				<< eventFieldVal_expected << "." << std::endl;
			std::cerr << "But it is " << eventFieldVal << "." << std::endl;
			
			return 1;
		}
	}
    if(eventNumber != 13)
    {
        std::cerr << "Expected, that stream(initial iterator) will contain 13 events. "
            << "But it contains " << eventNumber << "." << std::endl;
        return 1;
    }

    return 0;
}