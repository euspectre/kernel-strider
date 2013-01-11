/* This application produces a trace in the format recognized by 
 * ThreadSanitizer offline based on the trace saved by 
 * "simple_trace_recorder". The resulting trace will be output to stdout.
 *
 * Usage:
 *	kedr_convert_trace_to_tsan <input_trace_file>
 */

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

#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cerrno>

#include <iostream>
#include <map>
#include <string>
#include <stdexcept>

#include <kedr/object_types.h>

#include "../recorder.h"

using namespace std;
/* ====================================================================== */

typedef std::map<__u64, unsigned int> tid_map_t;
/* The mapping between the raw thread IDs reported by KernelStrider and the
 * IDs used by TSan offline. */
static tid_map_t tid_map;
/* ====================================================================== */

static unsigned int nrec = 0;
/* ====================================================================== */

/* Returns the code address (pc, start address of a function, ...) 
 * corresponding to the given raw address. Sign-extension is performed if 
 * needed. */
static unsigned long
code_address_from_raw(__u32 raw)
{
	return (unsigned long)(long)(__s32)raw;
}
/* ====================================================================== */

static void 
usage()
{
	cerr << "Usage:\n\tkedr_convert_trace_to_tsan <input_trace_file>" 
		<< endl;
}
/* ====================================================================== */

static void
output_tsan_event(const char *name, unsigned int tid, unsigned long pc,
	unsigned long addr_id, unsigned long size)
{
	printf("%s %x %lx %lx %lx\n", name, tid, pc, addr_id, size);
}

/* Allocates memory for an event record and reads the record from the file.
 * Returns the pointer to the record if successful, NULL if there is nothing
 * to read.
 * Throws 'runtime_error' on error. */
static struct kedr_tr_event_header *
read_record(FILE *fd)
{
	struct kedr_tr_event_header header;
	size_t nr_read;
	struct kedr_tr_event_header *record = NULL;
	size_t rest_size;
	
	errno = 0;
	nr_read = fread(&header, sizeof(header), 1, fd);
	if (nr_read == 0) {
		int err = errno;
		if (feof(fd))
			return NULL;
		
		throw runtime_error(strerror(err));
	}
	
	/* OK, having read the header, perform sanity checks and read the 
	 * rest if needed. */
	if ((int)header.event_size < (int)sizeof(header)) {
		cerr << "Record #" << nrec 
			<< ": invalid value of 'event_size' field: " 
			<< (int)header.event_size << endl;
		throw runtime_error(
			"encountered invalid data in the input file.");
	}
	
	record = (struct kedr_tr_event_header *)malloc(
		(size_t)header.event_size);
	if (record == NULL)
		throw runtime_error("out of memory.");
	
	memcpy(record, &header, sizeof(header));
	rest_size = (size_t)header.event_size - sizeof(header);
	if (rest_size > 0) {
		nr_read = fread((void *)((char *)record + sizeof(header)),
			1, rest_size, fd);
		if (nr_read < rest_size) {
			cerr << "Record #" << nrec 
				<< ": unexpected error or EOF." << endl;
			free(record);
			throw runtime_error(
				"failed to read data.");
		}
	}
	
	++nrec;
	return record;
}

static unsigned int
get_tsan_thread_id(const struct kedr_tr_event_header *record)
{
	static unsigned int nr_tids = 0;
	
	tid_map_t::iterator it;
	it = tid_map.find(record->tid);
	
	if (it == tid_map.end()) {
		++nr_tids;
		it = tid_map.insert(make_pair(record->tid, nr_tids)).first;
		printf("# \"Start\" thread T%x (real TID is %lx)\n", 
			nr_tids, (unsigned long)record->tid);
		output_tsan_event("THR_START", nr_tids, 0, 0, 0);
	}
	return it->second;
}

static void
report_memory_events(const struct kedr_tr_event_mem *ev)
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
			cerr << "Record #" << nrec << 
			": neither read nor write bit is set for event #" 
				<< i << "." << endl;
			throw runtime_error(
				"invalid event information.");
		}
		
		pc = code_address_from_raw(mem_op->pc);
		output_tsan_event(name, tid, pc, (unsigned long)mem_op->addr,
			(unsigned long)mem_op->size);
	}
}

static void 
report_block_event(const struct kedr_tr_event_block *ev)
{
	unsigned int tid = get_tsan_thread_id(&ev->header);
	unsigned long pc = code_address_from_raw(ev->pc);
	output_tsan_event("SBLOCK_ENTER", tid, pc, 0, 0);
}

static void 
report_call_pre_event(const struct kedr_tr_event_call *ev)
{
	unsigned int tid = get_tsan_thread_id(&ev->header);
	unsigned long pc = code_address_from_raw(ev->pc);
	output_tsan_event("RTN_CALL", tid, pc, 0, 0);
}

static void 
report_call_post_event(
	const struct kedr_tr_event_call *ev)
{
	unsigned int tid = get_tsan_thread_id(&ev->header);
	output_tsan_event("RTN_EXIT", tid, 0, 0, 0);
}

static void 
report_alloc_event(const struct kedr_tr_event_alloc_free *ev)
{
	unsigned int tid = get_tsan_thread_id(&ev->header);
	unsigned long pc = code_address_from_raw(ev->pc);
	output_tsan_event("MALLOC", tid, pc, (unsigned long)ev->addr, 
		(unsigned long)ev->size);
}

static void 
report_free_event(const struct kedr_tr_event_alloc_free *ev)
{
	unsigned int tid = get_tsan_thread_id(&ev->header);
	unsigned long pc = code_address_from_raw(ev->pc);
	output_tsan_event("FREE", tid, pc, (unsigned long)ev->addr, 0);
}

static void 
report_signal_event(const struct kedr_tr_event_sync *ev)
{
	unsigned int tid = get_tsan_thread_id(&ev->header);
	unsigned long pc = code_address_from_raw(ev->pc);
	output_tsan_event("SIGNAL", tid, pc, (unsigned long)ev->obj_id, 0);
}

static void 
report_wait_event(const struct kedr_tr_event_sync *ev)
{
	unsigned int tid = get_tsan_thread_id(&ev->header);
	unsigned long pc = code_address_from_raw(ev->pc);
	output_tsan_event("WAIT", tid, pc, (unsigned long)ev->obj_id, 0);
}

static void 
report_lock_event(const struct kedr_tr_event_sync *ev)
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
		cerr << "Record #" << nrec << 
			": unknown type of the lock: " 
				<< (unsigned int)lt << "." << endl;
			throw runtime_error(
				"invalid event information.");
	}
	
	output_tsan_event(name, tid, pc, (unsigned long)ev->obj_id, 0);
}

static void 
report_unlock_event(const struct kedr_tr_event_sync *ev)
{
	unsigned int tid = get_tsan_thread_id(&ev->header);
	unsigned long pc = code_address_from_raw(ev->pc);
	output_tsan_event("UNLOCK", tid, pc, (unsigned long)ev->obj_id, 0);
}

static void
do_convert(FILE *fd)
{
	struct kedr_tr_event_header *record = NULL;
	
	printf("# A fake \"main\" thread, T0\n");
	output_tsan_event("THR_START", 0, 0, 0, 0);
	
	for (;;) {
		record = read_record(fd);
		if (record == NULL)
			break;
		
		switch (record->type) {
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
		
		default: 
			break;
		}

		free(record);
	}
}
/* ====================================================================== */

int 
main(int argc, char *argv[])
{
	FILE *fd;
	int ret = EXIT_SUCCESS;
	
	if (argc != 2) {
		usage();
		return EXIT_FAILURE;
	}

	errno = 0;
	fd = fopen(argv[1], "r");
	if (fd == NULL) {
		cerr << "Failed to open " << argv[1] << ": "
			<< strerror(errno) << endl;
		return EXIT_FAILURE;
	}
	
	try {
		do_convert(fd);
	}
	catch (runtime_error &e) {
		cerr << "Error: " << e.what() << endl;
		ret = EXIT_FAILURE;
	}
	
	fclose(fd);
	return ret;
}
/* ====================================================================== */
