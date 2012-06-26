#ifndef REPORTPROCESSOR_H_1115_INCLUDED
#define REPORTPROCESSOR_H_1115_INCLUDED

#include <stdexcept>
#include <string>

#include "symbol_info.h"

// This class is responsible for processing of the report file and 
// "symbolizing" it according to the symbol information.
class CReportProcessor
{
public:
	// This class represents the exceptions thrown if processing fails.
	class CProcessingError : public std::runtime_error 
	{
	public:
		CProcessingError(const std::string& msg = "") 
			: std::runtime_error(msg) {};
	}; 	

private: // Only the static methods are intended to be used.
	CReportProcessor();

public:
	static void
	symbolizeReport(const std::string& reportFile, 
		const CSymbolInfo& si);
};

#endif // REPORTPROCESSOR_H_1115_INCLUDED
