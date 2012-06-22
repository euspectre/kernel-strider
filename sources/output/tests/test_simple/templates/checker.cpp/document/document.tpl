/* Checks that all events in the trace are expected ones */

#include <kedr/kedr_trace_reader/kedr_trace_reader.h>
#include <kedr/object_types.h>
#include <stdexcept>

#include <iostream>

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


/* Auxiliary functions */
static int check_int_variable(CTFContext& context,
	const CTFReader& reader, const std::string& varName,
	uint64_t valExpected, bool isHex)
{
	const CTFVarInt& varInt = findInt(reader, varName);
	
	uint64_t val = varInt.getUInt64(context);
	if(val != valExpected)
	{
		std::ios_base::fmtflags oldFlags = std::cerr.flags();
		
		if(isHex) std::cerr << std::hex << std::showbase;
		
		std::cerr << "Expected, that value of variable '" << varName
			<< "' will be " << valExpected << ", but it is " << val << ".\n";
		
		std::cerr.flags(oldFlags);
		
		return -1;
	}
	
	return 0;
}

static int check_tid(CTFReader::Event& event,
	const KEDRTraceReader& traceReader, uint64_t tidExpected)
{
	return check_int_variable(event, traceReader,
		"stream.event.context.tid", tidExpected, true);
}

static int check_type(CTFReader::Event& event,
	const KEDRTraceReader& traceReader, const std::string& typeExpected)
{
	const CTFVarEnum& varEnum = findEnum(traceReader, "stream.event.header.type");
	
	const std::string& type = varEnum.getEnum(event);
	
	if(type == "")
	{
		std::cerr << "Invalid event type.\n";
		return -1;
	}
	
	if(type != typeExpected)
	{
		std::cerr << "Expected that type of event will be '"
			<< typeExpected << "', but it is '" << type << "'.\n";
		return -1;
	}
	
	return 0;
}


/* Checkers */
static int check_event_function_entry(CTFReader::Event& event,
	const KEDRTraceReader& traceReader, uint64_t tid, uint64_t func)
{
	if(check_type(event, traceReader, "fentry")
		|| check_tid(event, traceReader, tid)
		|| check_int_variable(event, traceReader,
			"event.fields.fentry.func", func, true))
	{
		std::cerr << "Function entry event is incorrect.\n";
		return -1;
	}
	return 0;
}

class check_event_memory_accesses
{
public:
	check_event_memory_accesses(CTFReader::Event& event,
		const KEDRTraceReader& traceReader)
		: event(event), traceReader(traceReader) {}
			
	int check_begin(uint64_t tid, int n_subevents)
	{
		if(check_type(event, traceReader, "ma")
			|| check_tid(event, traceReader, tid)
			|| check_int_variable(event, traceReader,
				"event.context.ma.n_subevents", n_subevents, false))
		{
			std::cerr << "Memory access event event is incorrect.\n";
			return -1;
		}
		
		elemIterator = CTFVarArray::ElemIterator(
			findArray(traceReader, "event.fields.ma"), event);
		index = 0;
		
		return 0;
	}
	int check_subevent(uint64_t pc, uint64_t addr, uint64_t size, int access_type)
	{
		if(check_int_variable(*elemIterator, traceReader,
			"event.fields.ma[].pc", pc, true)
			|| check_int_variable(*elemIterator, traceReader,
			"event.fields.ma[].addr", addr, true)
			|| check_int_variable(*elemIterator, traceReader,
			"event.fields.ma[].size", size, false)
			|| check_int_variable(*elemIterator, traceReader,
			"event.fields.ma[].access_type", access_type, true))
		{
			std::cerr << index + 1 << "-th subevent in memory accesses "
				"event is incorrect.\n";
			return -1;
		}
		++elemIterator; ++index;
		
		return 0;
	}

private:
	CTFVarArray::ElemIterator elemIterator;
	int index;
	
	CTFReader::Event& event;
	const KEDRTraceReader& traceReader;
};

int main(int argc, char** argv)
{
    if(argc < 2)
    {
        std::cerr << "Argument with trace directory name should be supplied.\n";
        return -1;
    }
    
    KEDRTraceReader traceReader(argv[1]);
    
    int i = 0;
    KEDRTraceReader::EventIterator iter(traceReader);

	<$constant_def: join(\n\t)$>

	int result;
	
	<$block: join(\n\t)$>
    
    if(iter)
    {
		std::cerr << "Expected, that trace will contain " << i
			<< " events, but it contains more.\n";
		return -1;
    }
    
    return 0;
}