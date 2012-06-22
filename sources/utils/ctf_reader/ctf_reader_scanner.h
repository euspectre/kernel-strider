#ifndef CTF_READER_SCANNER_H_INCLUDED
#define CTF_READER_SCANNER_H_INCLUDED

#include <iostream>
#include "ctf_reader_parser_base.tab.hh"
#include "location.hh"

class CTFReaderScanner
{
public:
	CTFReaderScanner(std::istream& s);
	~CTFReaderScanner(void);
	
	int yylex(yy::parser::semantic_type* yylval, yy::location* yylloc);
	
	/* Type of extra-data for scanner */
	struct ExtraData
	{
		std::istream& s;
		
		ExtraData(std::istream& s) : s(s) {}
	};
private:
	/* Not copiable and assignable*/
	CTFReaderScanner(const CTFReaderScanner& scanner);
	CTFReaderScanner& operator =(const CTFReaderScanner& scanner);

	/* Real type is yyscan_t */
	void* _scanner;
	ExtraData extraData;
};

#endif /* CTF_READER_SCANNER_H_INCLUDED */