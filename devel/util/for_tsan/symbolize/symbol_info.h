#ifndef SYMBOLINFO_H_1046_INCLUDED
#define SYMBOLINFO_H_1046_INCLUDED

#include <stdexcept>
#include <string>
#include <vector>
///////////////////////////////////////////////////////////////////////

// Whitespace characters
extern const std::string whitespaceList;
///////////////////////////////////////////////////////////////////////

class SymbolData
{
public:
	unsigned long address;
	std::string name;
public:
	static bool 
	SymbolLess(const SymbolData& left, const SymbolData& right) {
		return (left.address < right.address);
	}
};

// This class is responsible for loading of the symbol information and for
// providing the means to lookup a symbol by an address it it.
class CSymbolInfo
{
public:
	// This class represents the exceptions thrown if loading fails.
	class CLoadingError : public std::runtime_error 
	{
	public:
		CLoadingError(const std::string& msg = "") 
			: std::runtime_error(msg) {};
	}; 	

public:
	// Load the symbol information from the specified file and construct
	// the object. 
	// May throw CLoadingError if something fails during the process.
	CSymbolInfo(const std::string& symbolFile);
	
	// Looks for a symbol the address could belong to. The address is 
	// considered to belong to the symbol S1 if address >= S1.address &&
	// address < S2.address, where S2 is the symbol immediately 
	// following S1 in the sequence of symbols sorted by their addresses.
	//
	// The function returns a pointer to the symbol if found, NULL 
	// otherwise.
	// 
	// [NB] If the address is greater than the greatest address among 
	// the symbols, the address is considered to belong to the symbol
	// with the greatest address.
	const SymbolData* symbolForAddress(unsigned long address) const;

private:
	std::vector<SymbolData> symbols;
};

#endif // SYMBOLINFO_H_1046_INCLUDED
