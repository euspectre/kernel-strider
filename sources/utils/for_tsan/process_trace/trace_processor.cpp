/* trace_processor.cpp - the facilities to actually process the trace 
 * collected in the kernel and to output the report. */

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
#include <sstream>

#include <cassert>
#include <cstdlib>
#include <cstdio>
#include <cstring>
#include <cstddef>

#include <unistd.h>
#include <fcntl.h>

#include <errno.h>
#include <poll.h>
#include <sys/wait.h>

#include <kedr/object_types.h>

#include "trace_processor.h"
#include "module_info.h"

using namespace std;
/* ====================================================================== */

extern bool debug_mode;
/* ====================================================================== */

/* [NB] The failures in the child process will not be detected until the
 * main process tries to pass the data to the child. */
TraceProcessor::TraceProcessor(const std::vector<const char *> &args)
	: nrec(0), nr_tids(0)
{
	assert(!args.empty());

	if (debug_mode)
		return;
	
	const char *file = args[0];
	
	std::vector<const char *> args_n(args);
	args_n.push_back(NULL);
	const char * const *argv = &args_n[0];
	
	int ret = 0;

	ret = pipe(in_pipe);
	if (ret != 0) {
		throw TraceProcessor::Error(
			string("Failed to create the input pipe: ") +
			strerror(errno));
	}

	/* Set FD_CLOEXEC to prevent making these file descriptors available
	 * to other applications launched by the user of TraceProcessor. */
	fcntl(in_pipe[0], F_SETFD, FD_CLOEXEC);
	fcntl(in_pipe[1], F_SETFD, FD_CLOEXEC);

	ret = pipe(out_pipe);
	if (ret != 0) {
		close(in_pipe[0]);
		close(in_pipe[1]);
		throw TraceProcessor::Error(
			string("Failed to create the output pipe: ") +
			strerror(errno));
	}

	fcntl(out_pipe[0], F_SETFD, FD_CLOEXEC);
	fcntl(out_pipe[1], F_SETFD, FD_CLOEXEC);

	pid = fork();
	if (pid == -1) {
		close(in_pipe[0]);
		close(in_pipe[1]);
		close(out_pipe[0]);
		close(out_pipe[1]);

		throw TraceProcessor::Error(
			string("Failed to create a process: ") +
			strerror(errno));
	}
	else if (pid == 0) {
		/* Child.
		 * [NB] It is recommended to call _exit() rather than exit()
		 * in the child process if an error is detected. Details:
		 * http://www.unixguide.net/unix/programming/1.1.3.shtml */
		close(in_pipe[1]);
		close(out_pipe[0]);

		/* Replace stdin and stderr with in_pipe[0] and out_pipe[1],
		 * respectively. */
		ret = dup2(in_pipe[0], STDIN_FILENO);
		if(ret == -1)
		{
			cerr <<
		"Failed to redirect stdin of the child process: " <<
		strerror(errno) << endl;
			close(in_pipe[0]);
			close(out_pipe[1]);
			_exit(EXIT_FAILURE);
		}

		/* [NB] We are interested in what TSan outputs to stderr
		 * rather than to stdout. */
		ret = dup2(out_pipe[1], STDERR_FILENO);
		if(ret == -1)
		{
			cerr <<
		"Failed to redirect stderr of the child process: " <<
		strerror(errno) << endl;
			close(in_pipe[0]);
			close(out_pipe[1]);
			_exit(EXIT_FAILURE);
		}

		ret = close(in_pipe[0]);
		if (ret == -1) {
			cerr <<
		"Failed to close the read end of the input pipe: " <<
		strerror(errno) << endl;
			close(out_pipe[1]);
			_exit(EXIT_FAILURE);
		}

		ret = close(out_pipe[1]);
		if (ret == -1) {
			cerr <<
		"Failed to close the write end of the output pipe: " <<
		strerror(errno) << endl;
			_exit(EXIT_FAILURE);
		}

		launch_app(file, argv);
	}
	else {
		/* Parent */
		close(in_pipe[0]);
		close(out_pipe[1]);
	}
}

/* [NB] One must not throw exceptions in dtors. So if errors occur, just 
 * report them and go on. */
TraceProcessor::~TraceProcessor()
{
	if (debug_mode) {
		cerr << "\nList of threads:\n\n";
		for (size_t i = 1; i < thread_names.size(); ++i)
			cerr << "T" << i << "\t" << thread_names[i] << "\n";
		
		fflush(stdout);
		fflush(stderr);
		return;
	}
	
	int ret;
	ret = close(in_pipe[1]); 
	if (ret == -1) {
		cerr << 
		"Failed to close the write end of the input pipe." <<
			endl;
	}
	/* The handler application will now receive EOF in its stdin and is
	 * expected to exit. */
	
	try {
		process_remaining_output();
		output_thread_list();
	}
	catch (runtime_error &e) {
		cerr << e.what() << endl;
	}
	
	ret = close(out_pipe[0]); 
	if (ret == -1) {
		cerr << 
		"Failed to close the read end of the output pipe." <<
			endl;
	}
	
	fflush(stdout);
}

/* A wrapper around execvp(). Calls _exit(EXIT_FAILURE) if an internal error
 * is encountered. 
 * Does not return. */
void
TraceProcessor::launch_app(const char *file, const char * const argv[])
{
	assert(file != NULL);
	assert(argv != NULL);

	/* Copy the argument strings to be able to use them in execvp().
	 * According to POSIX, execvp() takes 'char *const argv[]' although
	 * it does not change the strings in that array. See the rationale
	 * there for details. */
	char **args = NULL;
	
	size_t num = 0;
	while (argv[num] != NULL)
		++num;
	
	/* At least the name of the application must be specified. */
	assert(num >= 1);
	
	/* args[num] will be NULL, execvp() requires that.  */
	args = (char **)malloc((num + 1) * sizeof(args[0]));
	if (args == NULL) {
		cerr << "Failed to launch \"" << file << "\": " <<
			"not enough memory." << endl;
		_exit(EXIT_FAILURE);
	}
	memset(args, 0, (num + 1) * sizeof(args[0]));
	
	size_t i;
	for (i = 0; i < num; ++i) {
		args[i] = strdup(argv[i]); 
		if (args[i] == NULL)
			break;
	}
	if (i != num) {
		cerr << "Failed to launch \"" << file << "\": " <<
			"not enough memory for the arguments." << endl;
		for (i = 0; i < num; ++i)
			free(args[i]);
		free(args);
		_exit(EXIT_FAILURE);
	}
	/* [NB] args[] does not leak because execvp() effectively frees it
	 * when replacing the process image. */

	execvp(file, args);
	
	cerr << "Failed to launch \"" << file << "\": " <<
			strerror(errno) << endl;
	_exit(EXIT_FAILURE);
}

static bool
print_verifier_data(const vector<string> &addr_strs)
{
	if (addr_strs.empty())
		return false;
	
	vector<unsigned int> addrs;
	vector<string>::const_iterator it;
	for (it = addr_strs.begin(); it != addr_strs.end(); ++it) {
		char *rest = NULL;
		unsigned int addr = (unsigned int)strtoul(
			it->c_str(), &rest, 16);
		if (rest == NULL || rest[0] != 0)
			return false;
		addrs.push_back(addr);
	}

	assert(!addrs.empty());
	vector<unsigned int>::const_iterator item = addrs.begin();
	
	cout << "   Race verifier data: ";
	ModuleInfo::print_address_plain(*item);
	
	for (++item; item != addrs.end(); ++item) {
		cout << ",";
		ModuleInfo::print_address_plain(*item);
	}
	cout << "\n";
	
	return true;
}

void
TraceProcessor::process_report_line(const string &s)
{
	if (s.empty()) {
		cout << "\n";
		return;
	}
	
	vector<string> parts = split(s, " \t");
	if (parts.empty()) {
		cout << "\n";
		return;
	}
	
	string &first = parts[0];
	if (parts.size() >= 2 && first[0] == '#' && first.size() > 1) {
		/* Seems like a stack trace item. */
		char *rest = NULL;
		unsigned int index = (unsigned int)strtoul(
			first.c_str() + 1, &rest, 10);
		if (rest == NULL || rest[0] != 0) {
			/* Not a stack item number. */
			cout << s << "\n";
			return;
		}
		
		rest = NULL;
		unsigned int addr_eff = (unsigned int)strtoul(
			parts[1].c_str(), &rest, 16);
		if (rest == NULL || rest[0] != ':') {
			/* Not an address. */
			cout << s << "\n";
			return;
		}
		
		ModuleInfo::print_call_stack_item(index, addr_eff);
	}
	else if (parts.size() >= 4 && parts[0] == string("Race") &&
		 parts[1] == string("verifier") &&
		 parts[2] == string("data:")) {
		vector<string> addrs = split(parts[3], ",");
		if (!print_verifier_data(addrs)) {
			cout << s << "\n";
		}
	}
	else {
		cout << s << "\n";
	}
}

void
TraceProcessor::do_report_line()
{
	char c;
	ssize_t len;
	string s = "";

	len = read(out_pipe[0], &c, 1);
	
	while (len == 1) {
		if (c == '\n') {
			process_report_line(s);
			return;
		}
		
		s = s + c;
		len = read(out_pipe[0], &c, 1);
	}

	if (len == 0) {
		if (!s.empty())
			process_report_line(s);
	}
	else if (len == -1) {
		throw TraceProcessor::Error(
	string("Failed to read the output of the handler application: ") + 
			strerror(errno));
	}
	return;
}

/* Puts a line (a string) to be processed to the standard input of
 * the handler application. The string must not contain newline 
 * characters. A newline ('\n') will be appended automatically.
 *
 * Throws TraceProcessor::Error() if errors occur. */
void
TraceProcessor::put_line(const string &s)
{
	/* Writing data to the pipe should either be done completely in one 
	 * step or fail. Note that write() will block if the application
	 * is not ready to consume the data yet (i.e. the pipe is full). */
	ssize_t ret = write(in_pipe[1], s.c_str(), s.size());

	if (ret == -1) {
		throw TraceProcessor::Error(
		string("Failed to pass data to the handler application: ") + 
			strerror(errno));
	}
	else if (ret != (ssize_t)s.size()) {
		throw TraceProcessor::Error(
		string("Failed to pass data to the handler application: ") + 
			"the application accepted only part of the data.");
	}
	
	ret = write(in_pipe[1], "\n", 1);
	if (ret != 1) {
		throw TraceProcessor::Error(
		"Failed to pass a newline to the handler application.");
	}
}

/* Check if there are data available for reading. */
bool 
TraceProcessor::data_available()
{
	struct pollfd pollfd;
	pollfd.fd = out_pipe[0];
	pollfd.events = POLLIN;
	
	errno = 0;
	int ret = poll(&pollfd, 1, 0);
	if (ret == -1) {
		if (errno == EAGAIN)
			return false;
		else {
			throw TraceProcessor::Error(string(
		"Failed to check if a part of the report is available: ") + 
				strerror(errno));
		}
	}
	else if (ret == 1) {
		/* poll returns the number of pollfd instances for which
		 * .revents is non-zero. This way, if POLLHUP or POLLERR 
		 * flags are set for an instance, it will also be reported
		 * this way. We need to check that that POLLIN is set. */
		if (pollfd.revents & POLLIN)
			return true;
	}
	
	return false;
}

void
TraceProcessor::do_line(const string &s)
{
	while (data_available())
		do_report_line();
	
	put_line(s);
}

/* Wait for the handler application to terminate and process its remaining 
 * output. */
void
TraceProcessor::process_remaining_output()
{
	while (true) {
		pid_t p = waitpid(pid, NULL, WNOHANG);
		
		if (p == -1) {
			throw TraceProcessor::Error(string(
		"Failed to wait for the handler application to finish: ") +
				+ strerror(errno));
			break;
		}
		else if (p == 0) {
			/* The child process has not finished yet. */
			while (data_available())
				do_report_line();
			sleep(1);
		}
		else {
			/* The child process has finished. */
			while (data_available())
				do_report_line();
			break;
		}
	}
}
/* ====================================================================== */

/* Returns the code address (pc, start address of a function, ...) 
 * corresponding to the given raw address. Sign-extension is performed if 
 * needed. */
static unsigned long
code_address_from_raw(__u32 raw)
{
	return (unsigned long)(long)(__s32)raw;
}

void
TraceProcessor::output_tsan_event(const char *name, unsigned int tid, 
				  unsigned long pc, unsigned long addr_id, 
				  unsigned long size)
{
	if (pc != 0) 
		pc = ModuleInfo::effective_address(pc);
	
	ostringstream out;
	out << name << hex << " " << tid << " " << pc << " " << addr_id
		<< " " << size;
	
	if (!debug_mode) {
		do_line(out.str());
	}
	else {
		cout << out.str() << "\n";
	}
}

/* Allocates memory for an event record and reads the record from the file.
 * Returns the pointer to the record if successful, NULL if there is nothing
 * to read.
 * Throws 'TraceProcessor::Error' on error. */
struct kedr_tr_event_header *
TraceProcessor::read_record(FILE *fd)
{
	struct kedr_tr_event_header header;
	size_t nr_read;
	struct kedr_tr_event_header *record = NULL;
	size_t rest_size;
	
	errno = 0;
	nr_read = fread(&header, sizeof(header), 1, fd);
	if (nr_read == 0) {
		int e = errno;
		if (feof(fd))
			return NULL;
		
		throw TraceProcessor::Error(strerror(e));
	}
	
	/* OK, having read the header, perform sanity checks and read the 
	 * rest if needed. */
	if ((int)header.event_size < (int)sizeof(header)) {
		ostringstream err;
		err << "Invalid data in the input file, record #" << nrec 
			<< ": invalid value of 'event_size' field: " 
			<< (int)header.event_size;
		throw TraceProcessor::Error(err.str());
	}
	
	record = (struct kedr_tr_event_header *)malloc(
		(size_t)header.event_size);
	if (record == NULL)
		throw TraceProcessor::Error("Out of memory.");
	
	memcpy(record, &header, sizeof(header));
	rest_size = (size_t)header.event_size - sizeof(header);
	if (rest_size > 0) {
		nr_read = fread((void *)((char *)record + sizeof(header)),
			1, rest_size, fd);
		if (nr_read < rest_size) {
			ostringstream err;
			err << "Record #" << nrec 
				<< ": unexpected error or EOF.";
			free(record);
			throw TraceProcessor::Error(err.str());
		}
	}
	
	++nrec;
	return record;
}

unsigned int
TraceProcessor::get_tsan_thread_id(
	const struct kedr_tr_event_header *record)
{
	tid_map_t::iterator it;
	it = tid_map.find(record->tid);
	
	if (it == tid_map.end()) {
		ostringstream err;
		err << "Found an event with a real thread ID "
			<< (void *)(unsigned long)record->tid
	<< " with no previous \"thread start\" event for that thread.";
		throw TraceProcessor::Error(err.str());
	}
	return it->second;
}

void
TraceProcessor::handle_thread_start_event(struct kedr_tr_event_tstart *ev)
{
	tid_map_t::iterator it;
	it = tid_map.find(ev->header.tid);

	if (it != tid_map.end()) {
		ostringstream err;
		err << "Found \"thread start\" event with a real thread ID "
			<< (void *)(unsigned long)ev->header.tid
			<< " but there were events with this thread ID "
			<< "before without \"thread end\". ";
		err << "Missing \"thread end\" event?";
		throw TraceProcessor::Error(err.str());
	}

	++nr_tids;
	it = tid_map.insert(make_pair(ev->header.tid, nr_tids)).first;
	thread_names.push_back(&ev->comm[0]);
	output_tsan_event("THR_START", nr_tids, 0, 0, 0);
}

void
TraceProcessor::handle_thread_end_event(struct kedr_tr_event_tend *ev)
{
	tid_map_t::size_type n;
	n = tid_map.erase(ev->header.tid);

	if (n != 1) {
		ostringstream err;
		err << "Found \"thread end\" event "
			<< "with an unknown real thread ID: "
			<< (void *)(unsigned long)ev->header.tid;
		throw TraceProcessor::Error(err.str());
	}

	/* It is currently not needed to pass THR_END event to TSan. */
}

void
TraceProcessor::report_memory_events(const struct kedr_tr_event_mem *ev)
{
	unsigned int nr_events = (ev->header.type == KEDR_TR_EVENT_MEM) ? 
		ev->header.nr_events : 1;
	unsigned int tid = get_tsan_thread_id(&ev->header);
	
	for (unsigned int i = 0; i < nr_events; ++i) {
		const struct kedr_tr_event_mem_op *mem_op = &ev->mem_ops[i];
		__u32 event_bit = 1 << i;
		const char *name;
		unsigned long pc;
		
		if (ev->write_mask & event_bit) {
			/* [NB] Updates are also treated as writes. */
			name = "WRITE";
		}
		else if (ev->read_mask & event_bit) {
			name = "READ";
		}
		else {
			/* Neither read nor write? Invalid event. */
			ostringstream err;
			err << "Record #" << nrec << 
			": neither read nor write bit is set for event #" 
				<< i << ".";
			throw TraceProcessor::Error(err.str());
		}
		
		pc = code_address_from_raw(mem_op->pc);
		output_tsan_event(name, tid, pc, (unsigned long)mem_op->addr,
				  (unsigned long)mem_op->size);
	}
}

void 
TraceProcessor::report_block_event(const struct kedr_tr_event_block *ev)
{
	unsigned int tid = get_tsan_thread_id(&ev->header);
	unsigned long pc = code_address_from_raw(ev->pc);
	output_tsan_event("SBLOCK_ENTER", tid, pc, 0, 0);
}

void 
TraceProcessor::report_call_pre_event(const struct kedr_tr_event_call *ev)
{
	unsigned int tid = get_tsan_thread_id(&ev->header);
	unsigned long pc = code_address_from_raw(ev->pc);
	output_tsan_event("RTN_CALL", tid, pc, 0, 0);
}

void 
TraceProcessor::report_call_post_event(const struct kedr_tr_event_call *ev)
{
	unsigned int tid = get_tsan_thread_id(&ev->header);
	output_tsan_event("RTN_EXIT", tid, 0, 0, 0);
}

void 
TraceProcessor::report_alloc_event(
	const struct kedr_tr_event_alloc_free *ev)
{
	unsigned int tid = get_tsan_thread_id(&ev->header);
	unsigned long pc = code_address_from_raw(ev->pc);
	output_tsan_event("MALLOC", tid, pc, (unsigned long)ev->addr, 
		(unsigned long)ev->size);
}

void 
TraceProcessor::report_free_event(
	const struct kedr_tr_event_alloc_free *ev)
{
	unsigned int tid = get_tsan_thread_id(&ev->header);
	unsigned long pc = code_address_from_raw(ev->pc);
	output_tsan_event("FREE", tid, pc, (unsigned long)ev->addr, 0);
}

void 
TraceProcessor::report_signal_event(const struct kedr_tr_event_sync *ev)
{
	unsigned int tid = get_tsan_thread_id(&ev->header);
	unsigned long pc = code_address_from_raw(ev->pc);
	output_tsan_event("SIGNAL", tid, pc, (unsigned long)ev->obj_id, 0);
}

void 
TraceProcessor::report_wait_event(const struct kedr_tr_event_sync *ev)
{
	unsigned int tid = get_tsan_thread_id(&ev->header);
	unsigned long pc = code_address_from_raw(ev->pc);
	output_tsan_event("WAIT", tid, pc, (unsigned long)ev->obj_id, 0);
}

void 
TraceProcessor::report_lock_event(const struct kedr_tr_event_sync *ev)
{
	unsigned int tid = get_tsan_thread_id(&ev->header);
	unsigned long pc = code_address_from_raw(ev->pc);
	enum kedr_lock_type lt = (enum kedr_lock_type)ev->header.obj_type;
	const char *name;
	
	if (lt == KEDR_LT_RLOCK) {
		name = "READER_LOCK";
	}
	else if (lt == KEDR_LT_MUTEX || lt == KEDR_LT_SPINLOCK || 
		lt == KEDR_LT_WLOCK) {
		name = "WRITER_LOCK";
	}
	else {
		ostringstream err;
		err << "Record #" << nrec 
			<< ": unknown type of the lock: " 
			<< (unsigned int)lt << ".";
		throw TraceProcessor::Error(err.str());
	}
	
	output_tsan_event(name, tid, pc, (unsigned long)ev->obj_id, 0);
}

void 
TraceProcessor::report_unlock_event(const struct kedr_tr_event_sync *ev)
{
	unsigned int tid = get_tsan_thread_id(&ev->header);
	unsigned long pc = code_address_from_raw(ev->pc);
	output_tsan_event("UNLOCK", tid, pc, (unsigned long)ev->obj_id, 0);
}

void 
TraceProcessor::handle_target_load_event(struct kedr_tr_event_module *ev)
{
	ModuleInfo::on_module_load(ev->name, ev->init_addr, ev->init_size,
				   ev->core_addr, ev->core_size);
}

void 
TraceProcessor::handle_target_unload_event(struct kedr_tr_event_module *ev)
{
	ModuleInfo::on_module_unload(ev->name);
}

void
TraceProcessor::handle_fentry_event(struct kedr_tr_event_func *ev)
{
	ModuleInfo::on_function_entry(ev->func);
}

void
TraceProcessor::handle_fexit_event(struct kedr_tr_event_func *ev)
{
	ModuleInfo::on_function_exit(ev->func);
}
/* ====================================================================== */

/* A simple class that wraps a pointer to a malloc'ed memory block and frees
 * that memory in its dtor. */
class MemHelper
{
public:
	explicit MemHelper(struct kedr_tr_event_header *p = NULL)
		: ptr(p)
	{ }
	
	~MemHelper()
	{
		free(ptr);
	}
	
private:
	/* Prohibit copying and assignment. */
	MemHelper(const MemHelper &other);
	MemHelper & operator=(const MemHelper &p);
	
public:
	/* [NB] Do not call free for this pointer other than in the dtor
	 * of this class. */
	struct kedr_tr_event_header *ptr;
};

void 
TraceProcessor::process_trace()
{
	struct kedr_tr_event_header *record = NULL;
	
	/* A fake "main" thread, T0 */
	output_tsan_event("THR_START", 0, 0, 0, 0);
	thread_names.clear();
	thread_names.push_back("A fake \"main\" thread, T0");
	
	while(true) {
		MemHelper r(read_record(stdin));
		record = r.ptr;
		
		if (record == NULL)
			break;
		
		if (record->type == KEDR_TR_EVENT_TARGET_LOAD) {
			handle_target_load_event(
				(struct kedr_tr_event_module *)record);
			continue;
		} 
		
		if (record->type == KEDR_TR_EVENT_TARGET_UNLOAD) {
			handle_target_unload_event(
				(struct kedr_tr_event_module *)record);
			continue;
		}
		
		switch (record->type) {
		case KEDR_TR_EVENT_FENTRY:
			handle_fentry_event(
				(struct kedr_tr_event_func *)record);
			break;

		case KEDR_TR_EVENT_FEXIT:
			handle_fexit_event(
				(struct kedr_tr_event_func *)record);
			break;

		case KEDR_TR_EVENT_BLOCK_ENTER:
			report_block_event(
				(struct kedr_tr_event_block *)record);
			break;
		
		case KEDR_TR_EVENT_CALL_PRE:
			report_call_pre_event(
				(struct kedr_tr_event_call *)record);
			break;
		
		case KEDR_TR_EVENT_CALL_POST:
			report_call_post_event(
				(struct kedr_tr_event_call *)record);
			break;
		
		case KEDR_TR_EVENT_MEM:
		case KEDR_TR_EVENT_MEM_IO:
		/* We currently do not output memory events from locked
		 * operations to avoid false positives. It is not clear now
		 * how these operations should be treated. In the future,
		 * they should be output somehow too. */
			report_memory_events(
				(struct kedr_tr_event_mem *)record);
			break;
		
		case KEDR_TR_EVENT_ALLOC_POST:
			report_alloc_event(
				(struct kedr_tr_event_alloc_free *)record);
			break;
		
		case KEDR_TR_EVENT_FREE_PRE:
			report_free_event(
				(struct kedr_tr_event_alloc_free *)record);
			break;
		
		case KEDR_TR_EVENT_SIGNAL_PRE:
			report_signal_event(
				(struct kedr_tr_event_sync *)record);
			break;
		
		case KEDR_TR_EVENT_WAIT_POST:
			report_wait_event(
				(struct kedr_tr_event_sync *)record);
			break;
		
		case KEDR_TR_EVENT_LOCK_POST:
			report_lock_event(
				(struct kedr_tr_event_sync *)record);
			break;
			
		case KEDR_TR_EVENT_UNLOCK_PRE:
			report_unlock_event(
				(struct kedr_tr_event_sync *)record);
			break;

		case KEDR_TR_EVENT_THREAD_START:
			handle_thread_start_event(
				(struct kedr_tr_event_tstart *)record);
			break;

		case KEDR_TR_EVENT_THREAD_END:
			handle_thread_end_event(
				(struct kedr_tr_event_tend *)record);
			break;
			
		default: 
			break;
		}
	}
}

/* Output the list of threads with names. */
void 
TraceProcessor::output_thread_list()
{
	if (thread_names.empty())
		return;

	cout << "=======================================================\n";
	cout << "\nList of threads:\n\n";
	

	for (size_t i = 1; i < thread_names.size(); ++i)
		cout << "T" << i << "\t" << thread_names[i] << "\n";
}
/* ====================================================================== */

vector<string> 
split(const string &source, const char *delim, bool keep_empty)
{
	vector<string> results;

	size_t prev = 0;
	size_t next = 0;

	while ((next = source.find_first_of(delim, prev)) != string::npos)
	{
		if (keep_empty || (next - prev != 0))
		{
			results.push_back(source.substr(prev, next - prev));
		}
		prev = next + 1;
	}

	if (prev < source.size())
	{
		results.push_back(source.substr(prev));
	}

	return results;
}
/* ====================================================================== */
