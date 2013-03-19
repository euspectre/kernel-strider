/* tsan_process_trace - this application processes the trace collected
 * by KernelStrider. 
 *
 * tsan_process_trace invokes the offline variant of ThreadSanitizer to 
 * analyze the trace, preprocesses its report to resolve source line numbers
 * and outputs the result to stdout.
 *
 * Execute 'tsan_process_trace --help' for a more detailed description of 
 * the options. */

/* ========================================================================
 * Copyright (C) 2013, ROSA Laboratory
 *
 * Author: 
 *      Eugene A. Shatokhin <eugene.shatokhin@rosalab.ru>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published
 * by the Free Software Foundation.
 ======================================================================== */

#include <iostream>
#include <string>
#include <vector>
#include <stdexcept>

#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

#include <getopt.h>

#include <libelf.h>

#include "process_trace.h"
#include "module_info.h"
#include "trace_processor.h"

using namespace std;
/* ====================================================================== */

/* Path to TSan executable (see '-e' option). */
static string tsan_app = "";

/* See the description of '-s' option. */
static bool sections_only = false;

/* The argument to '--hybrid' option. An empty string means the option is
 * not set at all. */
static string hybrid_arg = "";
/* ====================================================================== */

/* Debug mode can be used to debug the software that has collected the trace
 * as well as this application, tsan_process_trace. See "--debug" option. */
bool debug_mode = false;
/* ====================================================================== */

static string
find_tsan_in_dirs(const string &name, const vector<string> &dirs)
{
	vector<string>::const_iterator pos;
	
	for (pos = dirs.begin(); pos != dirs.end(); ++pos) {
		string app = *pos;
		assert(!app.empty());
		
		if (app[app.size() - 1] != '/')
			app += '/';
		
		app += name;
		
		int fd = open(app.c_str(), O_RDONLY);
		if (fd != -1) {
			close(fd);
			return app;
		}
	}
	return string("");
}

/* Looks for ThreadSanitizer offline application in $PATH. Returns the path
 * to the application if found, string("") otherwise. */
static string
find_tsan_in_path()
{
	/* Possible names of TSan executable. */
	string name1 = (sizeof(void *) == 4) ? 
		"x86-linux-debug-ts_offline" :
		"amd64-linux-debug-ts_offline";
	string name2 = "ts_offline";
		
	const char *path = getenv("PATH");
	if (path == NULL || path[0] == 0)
		return NULL;
	
	vector<string> dirs = split(string(path), ":");
	string app = find_tsan_in_dirs(name1, dirs);
	if (!app.empty())
		return app;
	
	return find_tsan_in_dirs(name2, dirs);
}
/* ====================================================================== */

static void
show_usage()
{
	cerr << APP_USAGE;
}

static void
show_help()
{
	cerr << APP_HELP;
}

/* Process the command line arguments. Returns true if successful, false 
 * otherwise. */
static bool
process_args(int argc, char *argv[])
{
	int c;
	string module_dir = ".";
	
	struct option long_options[] = {
		{"help", no_argument, NULL, 'h'},
		{"dir", required_argument, NULL, 'd'},
		{"tsan", required_argument, NULL, 'e'},
		{"sections-only", no_argument, NULL, 's'},
		{"hybrid", required_argument, NULL, 'y'},
		{"debug", no_argument, NULL, 'b'},
		{NULL, 0, NULL, 0}
	};
	
	while (true) {
		int index = 0;
		c = getopt_long(argc, argv, "d:e:s", long_options, &index);
		if (c == -1)
			break;  /* all options have been processed */
		
		switch (c) {
		case 0:
			break;
		case 'h':
			show_help();
			exit(EXIT_SUCCESS);
			break;
		case 'd':
			if (optarg[0] == 0) {
				cerr << 
			"The specified directory path is empty.";
				cerr << endl;
				return false;
			}
			module_dir = string(optarg);
			break;
		case 'e':
			if (optarg[0] == 0) {
				cerr << 
			"The specified path to ThreadSanitizer is empty.";
				cerr << endl;
				return false;
			}
			tsan_app = string(optarg);
			break;
		case 's':
			sections_only = true;
			break;
		case 'y':
			hybrid_arg = string(optarg);
			if (hybrid_arg != "yes" && hybrid_arg != "no") {
				cerr << 
		"\'--hybrid\' requires \"yes\" or \"no\" as an argument.";
				cerr << endl;
				return false;
			}
			break;
		case 'b':
			debug_mode = true;
			break;
		case '?':
			/* Unknown option, getopt_long() should have already 
			printed an error message. */
			return false;
		default: 
			assert(false); /* Should not get here. */
		}
	}
	
	if (optind == argc) {
		cerr << "No modules specified." << endl;
		return false;
	}
	
	assert(!module_dir.empty());
	if (module_dir[module_dir.size() - 1] != '/')
		module_dir += '/';
	
	if (!sections_only) {
		try {
			DwflWrapper::init();
		}
		catch (runtime_error &e) {
			cerr << e.what() << endl;
			return false;
		}
	}
	
	try {
		for (int i = optind; i < argc; ++i) {
			ModuleInfo::add_module(argv[i], module_dir);
		}
	}
	catch (ModuleInfo::Error &e) {
		cerr << e.what() << endl;
		return false;
	}
	
	/* Find TSan if the path to it is not specified. */
	if (tsan_app.empty() && !debug_mode) {
		tsan_app = find_tsan_in_path();
		if (tsan_app.empty()) {
			cerr << 
		"Failed to find ThreadSanitizer offline in $PATH.";
			cerr << endl;
			return false;
		}
	}
	return true;
}
/* ====================================================================== */

int 
main(int argc, char *argv[])
{
	if (argc == 1) {
		show_usage();
		return EXIT_FAILURE;
	}
	
	if (elf_version(EV_CURRENT) == EV_NONE) {
		cerr << "Failed to initialize libelf: " << elf_errmsg(-1) 
			<< endl;
		return EXIT_FAILURE;
	}
	
	if (!process_args(argc, argv))
		return EXIT_FAILURE;

	const char *show_pc = "--show_pc";
	vector<const char *> args;
	
	string hybrid("--hybrid=");
	args.push_back(tsan_app.c_str());
	args.push_back(show_pc);
	if (!hybrid_arg.empty()) {
		hybrid += hybrid_arg;
		args.push_back(hybrid.c_str());
	}
		
	try {
		TraceProcessor tp(args);
		tp.process_trace();
	}
	catch (runtime_error &e) {
		cerr << e.what() << endl;
		return EXIT_FAILURE;
	}
	return EXIT_SUCCESS;
}
/* ====================================================================== */
