#ifndef TRACE_PROCESSOR_H_1605_INCLUDED
#define TRACE_PROCESSOR_H_1605_INCLUDED

#include <linux/types.h>
#include <unistd.h>

#include <cstdio>

#include <stdexcept>
#include <string>
#include <vector>
#include <map>

#include <utils/simple_trace_recorder/recorder.h>
#include <lzo/minilzo.h>
/* ====================================================================== */

/* An instance of this class performs the actual processing of the trace:
 * - launches a handler application (TSan) in a separate process, 
 * - reads the trace from stdin and converts it appropriately,
 * - feeds the converted trace to the handler application,
 * - retrieves the report of the application from its stderr,
 * - converts the report (resolves addresses, etc.) and outputs to stdout.
 * 
 * The handler application is launched when an object of this class is 
 * created and stops (after getting EOF in stdin) when the object is 
 * destroyed. 
 *
 * The handler applcaition is expected to process data from its stdin line 
 * by line. After reading a line, the application may output zero or more 
 * lines of report (each line is expected to be terminated by '\n'). */
class TraceProcessor
{
public:
	/* The methods of TraceProcessor throw this kind of exceptions when
	 * an error occurs. */
	class Error: public std::runtime_error
	{
	public:
		Error(const std::string &what_arg) :
			std::runtime_error(what_arg)
		{}
	};

public:
	/* Creates the object, starts the handler application in a new
	 * process, initializes all the necessary facilities. 
	 * 
	 * 'args' - args[0] is the path to the application's executable 
	 * 	file. If the array contains more elements, the rest are the 
	 *	arguments to the application (argv[1] .. argv[argc - 1]).
	 *
	 * The ctor throws TraceProcessor::Error() on failure. */
	TraceProcessor(const std::vector<const char *> &args);
	
	/* [NB] The destructor waits for the handler application to exit
	 * among other things and retrieves its remaining output. */
	~TraceProcessor();
	
	/* Read the trace from stdin, process it and output the results to
	 * stdout. */
	void process_trace();
	
private:
	static void launch_app(const char *file, const char * const argv[]);

private:
	void put_line(const std::string &s);
	void do_line(const std::string &s);
	void process_report_line(const std::string &s);
	void do_report_line();
	bool data_available();
	void process_remaining_output();
	void output_thread_list();
	
	struct kedr_tr_event_header *read_record(FILE *fd);
	unsigned int get_tsan_thread_id(__u64 tid);
	void output_tsan_event(const char *name, unsigned int tid, 
			       unsigned long pc, unsigned long addr_id, 
			       unsigned long size);
	
	void report_memory_events(const struct kedr_tr_event_mem *ev);
	void report_block_event(const struct kedr_tr_event_block *ev);
	void report_call_pre_event(const struct kedr_tr_event_call *ev);
	void report_call_post_event(const struct kedr_tr_event_call *ev);
	void report_alloc_event(const struct kedr_tr_event_alloc_free *ev);
	void report_free_event(const struct kedr_tr_event_alloc_free *ev);
	void report_signal_event(const struct kedr_tr_event_sync *ev);
	void report_wait_event(const struct kedr_tr_event_sync *ev);
	void report_lock_event(const struct kedr_tr_event_sync *ev);
	void report_unlock_event(const struct kedr_tr_event_sync *ev);
	
	void handle_target_load_event(struct kedr_tr_event_module *ev);
	void handle_target_unload_event(struct kedr_tr_event_module *ev);

	void handle_fentry_event(struct kedr_tr_event_func *ev);
	void handle_fexit_event(struct kedr_tr_event_func *ev);

	void handle_thread_start_event(struct kedr_tr_event_tstart *ev);
	void handle_thread_end_event(struct kedr_tr_event_tend *ev);
	
	void process_record(struct kedr_tr_event_header *record);
	void process_compressed_events(struct kedr_tr_event_header *record);
	
private:
	int in_pipe[2];
	int out_pipe[2];
	pid_t pid;
	unsigned int nrec;
	unsigned int nr_tids;
	
	/* The mapping between the raw thread IDs reported by KernelStrider 
	 * and the IDs used by TSan offline. */
	typedef std::map<__u64, unsigned int> tid_map_t;
	tid_map_t tid_map;

	/* Names of the threads corresponding to the IDs used by TSan */
	std::vector<std::string> thread_names;
};
/* ====================================================================== */

/* Splits a given string ('source') into tokens. 
 * 'delim' lists the delimiter characters. 
 * 'keep_empty' determines if the empty tokens should be included in the 
 * resulting list.
 * 
 * Based on http://stackoverflow.com/a/10051869/689077. */
std::vector<std::string> 
split(const std::string &source, const char *delim = " ", 
      bool keep_empty = false);
/* ====================================================================== */
#endif // TRACE_PROCESSOR_H_1605_INCLUDED
