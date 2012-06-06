///////////////////////////////////////////////////////////////////////
// This application replaces the addresses of the code locations with
// their "resolved" variants (including section/symbol name and the offset
// of the location into that section or symbol) in the report produced by 
// ThreadSanitizer. The report and the symbol data are loaded from the files
// given as the arguments to this application, the result is output to 
// stdout.
//
// The report file must have the format that ThreadSanitizer offline uses
// for its output. See the details here:
// http://code.google.com/p/data-race-test/wiki/ThreadSanitizerOffline
//
// The file with the symbol information must have the following format.
// The blank lines are ignored. Each non-blank line defines a symbol or 
// a section:
//	\s*<address, 0x%lx>\s+<name>
// Example:
//	0xf7e49000 .data
//	0xf7e47000 .text
//	0xf7e47304 .text.unlikely
///////////////////////////////////////////////////////////////////////

/* ========================================================================
 * Copyright (C) 2012, KEDR development team
 *
 * Authors: 
 *      Eugene A. Shatokhin <spectre@ispras.ru>
 *      Andrey V. Tsyvarev  <tsyvarev@ispras.ru>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published
 * by the Free Software Foundation.
 ======================================================================== */

#include <iostream>
#include <iomanip>
#include <cstddef>
#include <cstdlib>

#include <stdexcept>
#include <string>

#include "symbol_info.h"
#include "report_processor.h"
#include "config.h"

using namespace std;

///////////////////////////////////////////////////////////////////////
// Common data
const string appName = PROJECT_APP_NAME;

///////////////////////////////////////////////////////////////////////
// Output information about the usage of the tool.
static void
usage();

///////////////////////////////////////////////////////////////////////
int 
main(int argc, char* argv[])
{
	if (argc != 3) {
		usage();
		return EXIT_SUCCESS;
	}
	string reportFile = argv[1];
	string symbolFile = argv[2];
	
	try {
		// Load symbol data
		CSymbolInfo symbolInfo(symbolFile);
		
		//<>
		/*const SymbolData *sym;
		unsigned long address;
		
		address = 0x567;
		sym = symbolInfo.symbolForAddress(address);
		if (sym == NULL) {
			cout << "[DBG] " << "not found symbol for 0x" 
				<< hex << address << endl;
		}
		else {
			cout << "[DBG] " << sym->name << hex << "+0x" 
				<< (address - sym->address) << endl;
		}
		
		address = 0xf7e4747a;
		sym = symbolInfo.symbolForAddress(address);
		if (sym == NULL) {
			cout << "[DBG] " << "not found symbol for 0x" 
				<< hex << address << endl;
		}
		else {
			cout << "[DBG] " << sym->name << hex << "+0x" 
				<< (address - sym->address) << endl;
		}
		
		address = 0xf7e47480;
		sym = symbolInfo.symbolForAddress(address);
		if (sym == NULL) {
			cout << "[DBG] " << "not found symbol for 0x" 
				<< hex << address << endl;
		}
		else {
			cout << "[DBG] " << sym->name << hex << "+0x" 
				<< (address - sym->address) << endl;
		}
		
		address = 0xf7e4805f;
		sym = symbolInfo.symbolForAddress(address);
		if (sym == NULL) {
			cout << "[DBG] " << "not found symbol for 0x" 
				<< hex << address << endl;
		}
		else {
			cout << "[DBG] " << sym->name << hex << "+0x" 
				<< (address - sym->address) << endl;
		}
		
		address = 0xffe47467;
		sym = symbolInfo.symbolForAddress(address);
		if (sym == NULL) {
			cout << "[DBG] " << "not found symbol for 0x" 
				<< hex << address << endl;
		}
		else {
			cout << "[DBG] " << sym->name << hex << "+0x" 
				<< (address - sym->address) << endl;
		}*/
		//<>
		
		// Process the report
		CReportProcessor::symbolizeReport(reportFile, symbolInfo);
	} 
	catch (bad_alloc& e) {
		cerr << "Error: not enough memory" << endl;
		return EXIT_FAILURE;
	}
	catch (CSymbolInfo::CLoadingError& e) {
		cerr << "Failed to load " << symbolFile << ": " << e.what() 
			<< endl;
		return EXIT_FAILURE;
	}
	catch (CReportProcessor::CProcessingError& e) {
		cerr << "Failed to symbolize the report (" << reportFile 
			<< "):\n" << e.what() << endl;
		return EXIT_FAILURE;
	}
	catch (runtime_error& e) {
		cerr << "Error: " << e.what() << endl;
		return EXIT_FAILURE;
	}
	return EXIT_SUCCESS;
}

///////////////////////////////////////////////////////////////////////
static void 
usage()
{
	cout << "Usage: " << appName << " "
		 << "<raw_tsan_report_file> " 
		 << "<symbol_data_file>" << endl;
	return;
}
