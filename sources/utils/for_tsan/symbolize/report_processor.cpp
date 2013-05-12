#include <fstream>
#include <iostream>
#include <iomanip>
#include <cassert>
#include <cstdlib>
#include <cerrno>

#include "report_processor.h"
#include "symbolize_config.h"

using namespace std;
///////////////////////////////////////////////////////////////////////

// Strings
const string digits = "0123456789";

// Errors
const string errOpenFailed = "unable to open file";
const string errConversionFailed = 
	"failed to convert string to number: ";
///////////////////////////////////////////////////////////////////////

bool
isAllowedWhitespaceChar(char c)
{
	return (whitespaceList.find(c) != string::npos);
}

void
CReportProcessor::symbolizeReport(const std::string& reportFile, 
	const CSymbolInfo& si)
{
	// Process the report line by line. The lines that match the 
	// expression "^\s*#\d+\s+<hex_address>:.*$" should be processed,
	// the remaining ones should be output unchanged.
	ifstream inputStream(reportFile.c_str());
	if (!inputStream) {
		throw CProcessingError(errOpenFailed);
	}
	
	string line;
	while (getline(inputStream, line)) {
		string::size_type pos;
		string::size_type next;
		
		pos = line.find_first_not_of(whitespaceList);
		if (pos == string::npos || line.at(pos) != '#' || 
			pos == line.length() - 1) {
			cout << line << endl;
			continue;
		}
		++pos;
		
		next = line.find_first_not_of(digits, pos);
		if (next == pos || next == string::npos || 
			!isAllowedWhitespaceChar(line.at(next))) {
			cout << line << endl;
			continue;
		}
		
		pos = next;
		next = line.find_first_not_of(whitespaceList, pos);
		if (next == string::npos) {
			cout << line << endl;
			continue;
		}
		
		pos = next;
		string beforeAddress(line, 0, pos);
		
		const char *str = line.c_str() + pos;
		char *rest = NULL;
		
		errno = 0;
		unsigned long address = strtoul(str, &rest, 16);
		if (errno != 0 || *rest != ':') {
			cout << line << endl;
			continue;
		}
		
		string hexAddress(line, pos, (rest - str));
		string afterAddress(line, pos + (rest - str));
		
		// On x86-64, TSan may use the higher 6 bits of the address
		// for its own data and it does not restore these bits when
		// outputting the address, just zeroes them (OK for user-
		// space applications but not for the kernel). A workaround 
		// is provided here if an address in the report begins with
		// "0x3ff", this part of the address is replaced with 
		// "0xffff".
		string firstPart(hexAddress, 0, 5);
		if (firstPart == "0x3ff" || firstPart == "0x3FF") {
			hexAddress = "0xffff" + hexAddress.substr(5);
			
			errno = 0;
			address = strtoul(hexAddress.c_str(), NULL, 16);
			if (errno != 0) {
				throw CProcessingError(errConversionFailed +
					hexAddress);
			}
		}
		
		const SymbolData *sym = si.symbolForAddress(address);
		if (sym == NULL) {
			cout << line << endl;
			continue;
		}
		
		cout << beforeAddress << hex << sym->name << "+0x" 
			<< (address - sym->address) << " (" << hexAddress
			<< ")" << afterAddress << endl;
	}
}
///////////////////////////////////////////////////////////////////////
