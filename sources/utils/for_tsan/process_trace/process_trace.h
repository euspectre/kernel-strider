#ifndef PROCESS_TRACE_H_1623_INCLUDED
#define PROCESS_TRACE_H_1623_INCLUDED

#define APP_NAME "tsan_process_trace"

#define APP_USAGE \
 "Usage:\n" \
 "\t" APP_NAME " [options] <module_files_list> < <trace> > <report>\n" \
 "Execute \'" APP_NAME " --help\' for more information.\n\n"

#define APP_HELP \
 APP_NAME " [options] <module_files_list>\n\n" \
 APP_NAME " reads the trace of events collected for kernel\n" \
 "modules from stdin, invokes ThreadSanitizer to analyze the trace and\n" \
 "to detect potential data races, processes its report and outputs the\n" \
 "results to stdout.\n\n" \
 "" \
 "<module_files_list> - the space-separated list of the kernel module\n" \
 "files the trace was generated for.\n" \
 "If the debug information for the modules is in the separate files and\n" \
 "you would like the report to contain source line information instead\n" \
 "of just sections and offsets, you should list these files with the\n" \
 "the debug info rather than the module files.\n\n" \
 "" \
 "For each specified file, the part of the file name from the start to\n" \
 "\".ko\" or \".debug\" (whatever comes first), non-inclusive, must be the\n" \
 "name of the module.\n\n" \
 "" \
 "Options:\n\n" \
 "" \
 "--help\n\t" \
 "Show this help and exit.\n\n" \
 "" \
 "-d, --dir=<directory>\n\t" \
 "If set, the relative paths to the files listed in <module_files_list>\n\t" \
 "will be considered relative to the given directory rather than to\n\t" \
 "the current directory. Absolute paths are not affected by this\n\t" \
 "option.\n\n" \
 "" \
 "-e, --tsan=<path_to_tsan>\n\t" \
 "Specify the path to ThreadSanitizer offline. If the option is set,\n\t" \
 "that executable will be used as ThreadSanitizer offline. If not set,\n\t" \
 "<arch>-linux-debug-ts_offline and ts_offline will be looked for in\n\t" \
 "$PATH (in that order). <arch> is \"x86\" for 32-bit x86 systems and\n\t" \
 "\"amd64\" for 64-bit ones.\n\n" \
 "" \
 "-s, --sections-only\n\t" \
 "Resolve the call stack addresses only to the form\n\t" \
 "\"<module>: <section>+<offset>\", do not determine the source code\n\t" \
 "locations. Note that if a file from <module_files_list> does not\n\t" \
 "contain debug info, call stack addresses will be output this way\n\t" \
 "for that module anyway, even if this option is not set explicitly.\n\n" \
 "" \
 "--hybrid=<yes|no>\n\t" \
 "Specify whether ThreadSanitizer should operate in pure happens-before\n\t" \
 "mode (default) of in the hybrid mode. See the description of the\n\t" \
 "corresponding option of ThreadSanitizer.\n\n" 

#endif /* PROCESS_TRACE_H_1623_INCLUDED */
