/* This application produces a trace in the text format from the binary
 * trace file saved by "simple_trace_recorder". 
 * Except for "target load/unload" events, the format is the same that 
 * "kedr_test_reporter" uses if symbol resolution is not enabled.
 *
 * "target load/unload" events are reported as follows:
 *    LOAD target=<mod,0x%lx>
 *    UNLOAD target=<mod,0x%lx>
 * 
 * The resulting trace will be output to stdout.
 *
 * Usage:
 *	test_trace_to_text <input_trace_file>
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
#include <iomanip>
#include <string>
#include <stdexcept>

#include <kedr/object_types.h>

#include "recorder.h"

using namespace std;
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
	cerr << "Usage:\n\ttest_trace_to_text <input_trace_file>" 
		<< endl;
}
/* ====================================================================== */

static const char *
barrier_type_to_string(enum kedr_barrier_type bt)
{
	switch (bt) {
	case KEDR_BT_FULL:
		return "FULL";
	case KEDR_BT_LOAD:
		return "LOAD";
	case KEDR_BT_STORE:
		return "STORE";
	default:
		return "*UNKNOWN*";
	}
}

static const char *
lock_type_to_string(enum kedr_lock_type t)
{
	switch (t) {
	case KEDR_LT_MUTEX: 
		return "MUTEX";
	case KEDR_LT_SPINLOCK:
		return "SPINLOCK";
	case KEDR_LT_RLOCK:
		return "RLOCK";
	case KEDR_LT_WLOCK:
		return "WLOCK";
	default:
		return "*UNKNOWN*";
	}
}

static const char *
sw_type_to_string(enum kedr_sw_object_type t)
{
	switch (t) {
	case KEDR_SWT_COMMON: 
		return "COMMON";
	default:
		return "*UNKNOWN*";
	}
}
/* ====================================================================== */


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

static const char *
get_maccess_type(__u32 read_mask, __u32 write_mask, unsigned int event_no)
{
	__u32 event_bit = 1 << event_no;
	
	if (write_mask & event_bit) {
		if (read_mask & event_bit)
			return "UPDATE";
		else 
			return "WRITE";
	}
	else if (read_mask & event_bit) {
		return "READ";
	}
	else {
		/* Neither read nor write? Invalid event. */
		cerr << "Record #" << nrec << 
			": neither read nor write bit is set for event #" <<
			event_no << "." << endl;
		throw runtime_error(
			"invalid event information.");
	}
	return NULL;
}

static void
report_memory_events(unsigned long tid, const struct kedr_tr_event_mem *ev)
{
	unsigned int nr_events = ev->header.nr_events;

	for (unsigned int i = 0; i < nr_events; ++i) {
		const struct kedr_tr_event_mem_op *mem_op = &ev->mem_ops[i];
		unsigned long pc = code_address_from_raw(mem_op->pc);
		
		printf("TID=0x%lx %s pc=%lx addr=%lx size=%lu\n", tid, 
			get_maccess_type(ev->read_mask, ev->write_mask, i), 
			pc, (unsigned long)mem_op->addr,
			(unsigned long)mem_op->size);
	}
}

static void
report_locked_memory_event(unsigned long tid, 
	const struct kedr_tr_event_mem *ev)
{
	const struct kedr_tr_event_mem_op *mem_op = &ev->mem_ops[0];
	unsigned long pc = code_address_from_raw(mem_op->pc);
		
	printf("TID=0x%lx LOCKED %s pc=%lx addr=%lx size=%lu\n", tid, 
		get_maccess_type(ev->read_mask, ev->write_mask, 0), 
		pc, (unsigned long)mem_op->addr,
		(unsigned long)mem_op->size);
}

static void
report_io_memory_event(unsigned long tid, 
	const struct kedr_tr_event_mem *ev)
{
	const struct kedr_tr_event_mem_op *mem_op = &ev->mem_ops[0];
	unsigned long pc = code_address_from_raw(mem_op->pc);
		
	printf("TID=0x%lx IO_MEM %s pc=%lx addr=%lx size=%lu\n", tid, 
		get_maccess_type(ev->read_mask, ev->write_mask, 0), 
		pc, (unsigned long)mem_op->addr,
		(unsigned long)mem_op->size);
}

static void
report_load_unload_event(const struct kedr_tr_event_module *ev, bool is_load)
{
	printf("%s target=0x%lx\n", (is_load ? "LOAD" : "UNLOAD"), 
		(unsigned long)ev->mod);
}

static void
report_func_event(unsigned long tid, const struct kedr_tr_event_func *ev, 
	bool is_entry)
{
	unsigned long func = code_address_from_raw(ev->func);
	printf("TID=0x%lx %s %lx\n", tid, (is_entry ? "FENTRY" : "FEXIT"), 
		func);
}

static void 
report_call_event(unsigned long tid, const struct kedr_tr_event_call *ev,
	bool is_pre)
{
	unsigned long pc = code_address_from_raw(ev->pc);
	unsigned long func = code_address_from_raw(ev->func);
	printf("TID=0x%lx CALL_%s pc=%lx %lx\n", tid, 
		(is_pre ? "PRE" : "POST"), pc, func);
}

static void 
report_block_event(unsigned long tid, const struct kedr_tr_event_block *ev)
{
	unsigned long pc = code_address_from_raw(ev->pc);
	printf("TID=0x%lx BLOCK_ENTER pc=%lx\n", tid, pc);
}

static void 
report_barrier_event(unsigned long tid, 
	const struct kedr_tr_event_barrier *ev, bool is_pre)
{
	unsigned long pc = code_address_from_raw(ev->pc);
	enum kedr_barrier_type bt = 
		(enum kedr_barrier_type)ev->header.obj_type;
	printf("TID=0x%lx BARRIER %s %s pc=%lx\n", tid, 
		barrier_type_to_string(bt), (is_pre ? "PRE" : "POST"), pc);
}

static void 
report_alloc_event(unsigned long tid, 
	const struct kedr_tr_event_alloc_free *ev, bool is_pre)
{
	unsigned long pc = code_address_from_raw(ev->pc);
	if (is_pre) { 
		printf("TID=0x%lx ALLOC PRE pc=%lx size=%lu\n", tid, pc,
			(unsigned long)ev->size);
	}
	else {
		printf("TID=0x%lx ALLOC POST pc=%lx addr=%lx size=%lu\n", 
			tid, pc, (unsigned long)ev->addr,
			(unsigned long)ev->size);
	}
}

static void 
report_free_event(unsigned long tid, 
	const struct kedr_tr_event_alloc_free *ev, bool is_pre)
{
	unsigned long pc = code_address_from_raw(ev->pc);
	printf("TID=0x%lx FREE %s pc=%lx addr=%lx\n", tid, 
		(is_pre ? "PRE" : "POST"), pc, (unsigned long)ev->addr);
}

static void 
report_signal_event(unsigned long tid, const struct kedr_tr_event_sync *ev,
	bool is_pre)
{
	unsigned long pc = code_address_from_raw(ev->pc);
	enum kedr_sw_object_type ot = 
		(enum kedr_sw_object_type)ev->header.obj_type;
	
	printf("TID=0x%lx SIGNAL %s %s pc=%lx id=%lx\n", tid,
		sw_type_to_string(ot), (is_pre ? "PRE" : "POST"), pc,
		(unsigned long)ev->obj_id);
}

static void 
report_wait_event(unsigned long tid, const struct kedr_tr_event_sync *ev, 
	bool is_pre)
{
	unsigned long pc = code_address_from_raw(ev->pc);
	enum kedr_sw_object_type ot = 
		(enum kedr_sw_object_type)ev->header.obj_type;
	
	printf("TID=0x%lx WAIT %s %s pc=%lx id=%lx\n", tid,
		sw_type_to_string(ot), (is_pre ? "PRE" : "POST"), pc,
		(unsigned long)ev->obj_id);
}


static void 
report_lock_event(unsigned long tid, const struct kedr_tr_event_sync *ev,
	bool is_pre)
{
	unsigned long pc = code_address_from_raw(ev->pc);
	enum kedr_lock_type lt = (enum kedr_lock_type)ev->header.obj_type;
	
	printf("TID=0x%lx LOCK %s %s pc=%lx id=%lx\n", tid,
		lock_type_to_string(lt), (is_pre ? "PRE" : "POST"), pc,
		(unsigned long)ev->obj_id);
}

static void 
report_unlock_event(unsigned long tid, const struct kedr_tr_event_sync *ev,
	bool is_pre)
{
	unsigned long pc = code_address_from_raw(ev->pc);
	enum kedr_lock_type lt = (enum kedr_lock_type)ev->header.obj_type;

	printf("TID=0x%lx UNLOCK %s %s pc=%lx id=%lx\n", tid,
		lock_type_to_string(lt), (is_pre ? "PRE" : "POST"), pc,
		(unsigned long)ev->obj_id);
}

static void
do_convert(FILE *fd)
{
	struct kedr_tr_event_header *record = NULL;
	
	for (;;) {
		record = read_record(fd);
		if (record == NULL)
			break;
		
		unsigned long tid = (unsigned long)record->tid;
		
		switch (record->type) {
		case KEDR_TR_EVENT_TARGET_LOAD:
			report_load_unload_event(
				(struct kedr_tr_event_module *)record, 
				true);
			break;
		
		case KEDR_TR_EVENT_TARGET_UNLOAD:
			report_load_unload_event(
				(struct kedr_tr_event_module *)record, 
				false);
			break;
		
		case KEDR_TR_EVENT_FENTRY:
			report_func_event(tid,
				(struct kedr_tr_event_func *)record, true);
			break;
		
		case KEDR_TR_EVENT_FEXIT:
			report_func_event(tid,
				(struct kedr_tr_event_func *)record, false);
			break;
		
		case KEDR_TR_EVENT_CALL_PRE:
			report_call_event(tid,
				(struct kedr_tr_event_call *)record, true);
			break;
		
		case KEDR_TR_EVENT_CALL_POST:
			report_call_event(tid,
				(struct kedr_tr_event_call *)record, false);
			break;
		
		case KEDR_TR_EVENT_MEM:
			report_memory_events(tid,
				(struct kedr_tr_event_mem *)record);
			break;
		
		case KEDR_TR_EVENT_MEM_LOCKED:
			report_locked_memory_event(tid,
				(struct kedr_tr_event_mem *)record);
			break;
		
		case KEDR_TR_EVENT_MEM_IO:
			report_io_memory_event(tid,
				(struct kedr_tr_event_mem *)record);
			break;
		
		case KEDR_TR_EVENT_BARRIER_PRE:
			report_barrier_event(tid,
				(struct kedr_tr_event_barrier *)record,
				true);
			break;
		
		case KEDR_TR_EVENT_BARRIER_POST:
			report_barrier_event(tid,
				(struct kedr_tr_event_barrier *)record,
				false);
			break;

		case KEDR_TR_EVENT_ALLOC_PRE:
			report_alloc_event(tid,
				(struct kedr_tr_event_alloc_free *)record,
				true);
			break;
		
		case KEDR_TR_EVENT_ALLOC_POST:
			report_alloc_event(tid,
				(struct kedr_tr_event_alloc_free *)record,
				false);
			break;
		
		case KEDR_TR_EVENT_FREE_PRE:
			report_free_event(tid,
				(struct kedr_tr_event_alloc_free *)record,
				true);
			break;
		
		case KEDR_TR_EVENT_FREE_POST:
			report_free_event(tid,
				(struct kedr_tr_event_alloc_free *)record,
				false);
			break;
		
		case KEDR_TR_EVENT_SIGNAL_PRE:
			report_signal_event(tid, 
				(struct kedr_tr_event_sync *)record, true);
			break;
		
		case KEDR_TR_EVENT_SIGNAL_POST:
			report_signal_event(tid, 
				(struct kedr_tr_event_sync *)record, false);
			break;
		
		case KEDR_TR_EVENT_WAIT_PRE:
			report_wait_event(tid, 
				(struct kedr_tr_event_sync *)record, true);
			break;
			
		case KEDR_TR_EVENT_WAIT_POST:
			report_wait_event(tid, 
				(struct kedr_tr_event_sync *)record, false);
			break;
		
		case KEDR_TR_EVENT_LOCK_PRE:
			report_lock_event(tid, 
				(struct kedr_tr_event_sync *)record, true);
			break;
		
		case KEDR_TR_EVENT_LOCK_POST:
			report_lock_event(tid, 
				(struct kedr_tr_event_sync *)record, false);
			break;
			
		case KEDR_TR_EVENT_UNLOCK_PRE:
			report_unlock_event(tid, 
				(struct kedr_tr_event_sync *)record, true);
			break;
		
		case KEDR_TR_EVENT_UNLOCK_POST:
			report_unlock_event(tid, 
				(struct kedr_tr_event_sync *)record, false);
			break;
		
		case KEDR_TR_EVENT_BLOCK_ENTER:
			report_block_event(tid, 
				(struct kedr_tr_event_block *)record);
			break;

		default: 
			cerr << "Record #" << nrec << 
				": unknown event type: " << record->type <<
				"." << endl;
			free(record);
			throw runtime_error("invalid event information.");
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
