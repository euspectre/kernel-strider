#include <algorithm>
#include <sstream>
#include <iterator>
#include <fstream>
#include <iostream>
#include <iomanip>
#include <cassert>
#include <cstdlib>
#include <cerrno>

#include "symbol_info.h"
#include "symbolize_config.h"

using namespace std;
///////////////////////////////////////////////////////////////////////

// Whitespace characters
extern const string whitespaceList = " \t\n\r\v\a\b\f";
///////////////////////////////////////////////////////////////////////

const string lineString = "line";

// Errors
const string errOpenFailed = "unable to open file";
const string errNameExpected = "symbol name is missing";
const string errInvalidAddress = "invalid symbol address";
///////////////////////////////////////////////////////////////////////

// Utility functions

// Trims the the string, i.e. removes the whitespace characters from both 
// the beginning and the end.
void 
trimString(std::string& s)
{
	if (s.length() == 0) {
		return;
	}
	
	string::size_type beg = s.find_first_not_of(whitespaceList);
	if (beg == string::npos) {
		// the string consists entirely of whitespace characters
		s.clear();
		return;
	}
	
	string::size_type end = s.find_last_not_of(whitespaceList);
	string t(s, beg, end - beg + 1);
	s.swap(t);
	
	return;
}

// Formats the message like the follwing: "line <n>: <text>"
std::string
formatErrorMessage(int lineNumber, const std::string& text)
{
    ostringstream err;
    err << lineString << " " << lineNumber << ": " << text;
    return err.str();
}
///////////////////////////////////////////////////////////////////////

CSymbolInfo::CSymbolInfo(const std::string& symbolFile)
{
	// Load the symbol information from the given file, record by record.
	ifstream inputStream(symbolFile.c_str());
	if (!inputStream) {
		throw CLoadingError(errOpenFailed);
	}
	
	int lineNumber = 0;
	string line;
	
	while (getline(inputStream, line)) {
		++lineNumber;
		trimString(line);

		// If the line is blank, skip it.
		if (line.empty()) {
			continue;
		}
		
		char* next = NULL;
		const char* str = line.c_str();
		SymbolData sym;
		
		errno = 0;
		sym.address = strtoul(str, &next, 16);
		if (errno != 0) {
			string err = formatErrorMessage(lineNumber, 
				errInvalidAddress);
			throw CLoadingError(err);
		}
		
		string rest(line, (next - str));
		trimString(rest);
		
		if (rest.empty()) {
			string err = formatErrorMessage(lineNumber, 
				errNameExpected);
			throw CLoadingError(err);
		}
		
		sym.name = rest;
		symbols.push_back(sym);
	}
	
	sort(symbols.begin(), symbols.end(), SymbolData::SymbolLess);
}

const SymbolData* 
CSymbolInfo::symbolForAddress(unsigned long address) const
{
	if (symbols.empty() || address < symbols[0].address) {
		return NULL;
	}
	
	SymbolData sym;
	sym.address = address;
	
	std::vector<SymbolData>::const_iterator it;
	it = upper_bound(symbols.begin(), symbols.end(), sym, 
		SymbolData::SymbolLess);
	
	assert(it != symbols.begin());
	
	--it;
	return &(*it);
}
///////////////////////////////////////////////////////////////////////
