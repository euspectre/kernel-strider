#include "ctf_reader_parser_base.tab.hh"
#include "ctf_reader_scanner.h"
#include "ctf_ast.h"

#include <iostream>

class CTFReaderParser
{
public:
	CTFReaderParser(std::istream& stream, CTFAST& ast);
	
	/* Parse given stream and build given ast according to it */
	void parse(void);
private:
	CTFReaderScanner scanner;
	yy::parser parserBase;
};