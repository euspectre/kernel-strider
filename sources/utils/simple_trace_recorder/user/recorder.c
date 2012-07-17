/* The user-space part of the trace recorder. This application polls the 
 * file in debugfs created by the kernel part of the recorder. When the data
 * become available, it mmaps the file, reads the data and saves them to 
 * the file specified in its parameters. 
 * 
 * Usage: 
 * 	kedr_simple_trace_recorder <file_to_save_data_to>
 * 
 * <file_to_save_data_to> - path to the file to save the trace to. If the 
 * file does not exist, it will be created. The previous contents of the 
 * file will be cleared. 
 *
 * The application stops polling the file and exits when it sees "target 
 * unloaded" event or if it is interrupted by a signal. If the signal is 
 * SIGINT (e.g., Ctrl+C) or SIGTERM (e.g., plain 'kill'), the application
 * also saves the remaining available data before exiting. */

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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <signal.h>
#include <poll.h>
#include <sys/stat.h>
#include <sys/mman.h>

#include <simple_trace_recorder/recorder.h>
#include <kedr_st_rec_config.h>
/* ====================================================================== */

/* Memory barriers. */
#if defined(__i386__)
#define tr_smp_mb()	asm volatile("lock; addl $0,0(%%esp)" ::: "memory")
#define tr_smp_rmb()	tr_smp_mb()
#endif

#if defined(__x86_64__)
#define tr_smp_mb()	asm volatile("mfence" ::: "memory")
#define tr_smp_rmb()	asm volatile("lfence" ::: "memory")
#endif
/* ====================================================================== */

static const char *out_file = NULL;
static const char *in_file = 
	KEDR_ST_REC_DEBUGFS_DIR "/" KEDR_ST_REC_KMODULE_NAME "/buffer";

/* The file containing the value of 'nr_data_pages' parameter of the kernel
 * module. */
static const char *param_file = 
	"/sys/module/" KEDR_ST_REC_KMODULE_NAME "/parameters/nr_data_pages";

static unsigned int nr_data_pages = 0;
static unsigned long page_size = 0;
static unsigned int buffer_size = 0;

static unsigned long long nr_events = 0;

static volatile int done = 0;
/* ====================================================================== */

/* Returns the current write position in the buffer. Note that the 
 * corresponding offset from the beginning of the buffer is 'page_size' + 
 * the return value of this function. 
 * 
 * Do not attempt to access the write position without using this function. 
 */
static __u32
get_write_pos(void *buffer)
{
	__u32 write_pos = ((struct kedr_tr_start_page *)buffer)->write_pos;
	tr_smp_rmb();
	return write_pos;	
}

static __u32
get_read_pos(void *buffer)
{
	/* 'read_pos' can be updated by this application only, the kernel 
	 * part does not change it. No barriers are needed here. */
	return ((struct kedr_tr_start_page *)buffer)->read_pos;
}

/* Updates the read position of the data buffer. */
static void
set_read_pos(void *buffer, __u32 new_read_pos)
{
	/* Make sure we have completed reading the records from the buffer
	 * before we update the read position. */
	tr_smp_mb();
	((struct kedr_tr_start_page *)buffer)->read_pos = new_read_pos;
}
/* ====================================================================== */

static void
print_usage(void)
{
	printf("Usage:\n\tkedr_simple_trace_recorder "
		"<file_to_save_data_to>\n");
}

static int 
test_is_power_of_2(unsigned long n)
{
	return (n != 0 && ((n & (n - 1)) == 0));
}

static void 
sig_handler(int sig)
{
	(void)sig;
	
	/* Indicate that the app should read and save the remaining data and
	 * then finish. */
	done = 1;
}

/* Returns the address in the buffer corresponding to the given position.
 * Takes into account that the data begin from page #1 rather than #0 in
 * the buffer. */
static void *
buffer_pos_to_addr(void *buffer, __u32 pos)
{
	pos &= (buffer_size - 1);
	return (void *)((char *)buffer + page_size + pos);
}

static __u32
skip_to_next_page(__u32 rp)
{
	return ((rp + page_size) & ~(page_size - 1));
}

/* Nonzero if at least an event header would not cross the page boundary if
 * located at the position 'rp'. */
static int
enough_space_for_header(__u32 rp)
{
	unsigned int offset = (unsigned int)rp & (page_size - 1);
	return (offset + sizeof(struct kedr_tr_event_header) <= page_size);
}

/* Reads the data currently available in the buffer and writes the event 
 * information to the output file. */
static int
process_data(void *buffer, FILE *outf)
{
	__u32 wp;
	__u32 rp;
	struct kedr_tr_event_header *treh;
	
	rp = get_read_pos(buffer);
	wp = get_write_pos(buffer);
	
	while (((rp - wp) & (buffer_size - 1)) != 0) {
		if (!enough_space_for_header(rp)) {
			rp = skip_to_next_page(rp);
			continue;
		}
		
		treh = buffer_pos_to_addr(buffer, rp);
		
		if (treh->type >= KEDR_TR_EVENT_MAX) {
			fprintf(stderr, "Unknown event type: %u (pos=%u)\n",
				(unsigned int)treh->type, (unsigned int)rp);
			return 1;
		}
		else if (treh->type == KEDR_TR_EVENT_SKIP) {
			rp = skip_to_next_page(rp);
			continue;
		}
		
		/* Sanity check, just in case. */
		if ((unsigned long)treh->event_size >= page_size) {
			fprintf(stderr, 
				"Event size is too large: %u (pos=%u)\n",
				(unsigned int)treh->event_size, 
				(unsigned int)rp);
			return 1;
		}
		
		++nr_events;
		errno = 0;
		fwrite(treh, (size_t)treh->event_size, 1, outf);
		if (errno != 0) {
			fprintf(stderr, 
		"Failed to write an event (pos=%u) to the file: %s\n",
				(unsigned int)rp, 
				strerror(errno));
			return 1;
		}
		rp += treh->event_size;
				
		/* Finish if the target module has been unloaded. */
		if (treh->type == KEDR_TR_EVENT_TARGET_UNLOAD) {
			done = 1;
			break;
		}
	}
	set_read_pos(buffer, rp);
	return 0; /* OK */
}

static int 
save_trace(int fd_in, FILE *outf)
{
	void *buffer = NULL;
	int ret = 0;
	int err = 0;
	size_t mapping_size = (nr_data_pages + 1) * page_size;
	struct sigaction sa;
	struct pollfd pollfd;
	
	sa.sa_handler = sig_handler;
	sigemptyset(&sa.sa_mask);
	sa.sa_flags = 0;
	
	if ((sigaction(SIGINT, &sa, NULL) == -1) || 
	    (sigaction(SIGTERM, &sa, NULL) == -1)) {
		fprintf(stderr, "Failed to set signal handlers.\n");
		return 1;
	}
	
	buffer = mmap(NULL, mapping_size, PROT_READ | PROT_WRITE, 
		MAP_SHARED, fd_in, 0);
	if (buffer == MAP_FAILED) {
		fprintf(stderr, "Failed to mmap() the input file: %s\n",
			strerror(errno));
		return 1;
	}
	
	pollfd.fd = fd_in;
	pollfd.events = POLLIN;
	
	for (;;) {
		err = process_data(buffer, outf);
		if (err || done)
			break;
		
		errno = 0;
		ret = poll(&pollfd, 1, -1);
		if (ret == -1 && errno != EAGAIN && errno != EINTR) {
			fprintf(stderr, 
				"Failed to poll() the input file: %s\n",
				strerror(errno));
			err = 1;
			break;
		}
	}
	
	ret = munmap(buffer, mapping_size);
	if (ret != 0) {
		fprintf(stderr, "Failed to unmmap() the input file: %s\n",
			strerror(errno));
		return 1;
	}
	
	printf("Recorded %llu event(s).\n", nr_events);
	return err;
}

#define KEDR_NR_MAX_DIGITS 9

static int
read_nr_data_pages(void)
{
	char *endp = NULL;
	char *p;
	char value_buf[KEDR_NR_MAX_DIGITS + 1];
	FILE *fd;
	
	memset(&value_buf[0], 0, sizeof(value_buf));
	
	errno = 0;
	fd = fopen(param_file, "r");
	if (fd == NULL) {
		fprintf(stderr, "Failed to open %s: %s\n",
			param_file, strerror(errno));
		return 1;
	}
	
	p = fgets(&value_buf[0], KEDR_NR_MAX_DIGITS + 1, fd);
	if (errno != 0 || p == NULL) {
		fprintf(stderr, "Failed to read %s: %s\n",
			param_file, 
			(errno != 0 ? strerror(errno) : "unexpected EOF"));
		fclose(fd);
		return 1;
	}
	fclose(fd);
	
	errno = 0;
	nr_data_pages = (unsigned int)strtoul(value_buf, &endp, 10);
	if (errno != 0 || (*endp != 0 && *endp != '\n')) {
		fprintf(stderr, "Invalid value of 'nr_data_pages': %s\n",
			value_buf);
		return 1;
	}
	
	if (!test_is_power_of_2(nr_data_pages)) {
		fprintf(stderr, "'nr_data_pages' must be a power of 2.\n");
		return 1;
	}
	return 0;
}

/* ====================================================================== */

int
main(int argc, char *argv[])
{
	int fd_in;
	FILE *outf;
	int ret = 0;
	
	if (argc != 2) {
		print_usage();
		return EXIT_FAILURE;
	}
	
	ret = read_nr_data_pages();
	if (ret != 0)
		return EXIT_FAILURE;
	
	/* Size of a memory page on this system. */
	page_size = (unsigned long)sysconf(_SC_PAGE_SIZE);
	buffer_size = nr_data_pages * page_size;
	
	out_file = argv[1];
	
	errno = 0;
	fd_in = open(in_file, O_RDWR);
	if (fd_in == -1) {
		fprintf(stderr, "Failed to open input file (%s): %s\n",
			in_file, strerror(errno));
		return EXIT_FAILURE;
	}
	
	errno = 0;
	outf = fopen(out_file, "w");
	if (outf == NULL) {
		fprintf(stderr, "Failed to open output file (%s): %s\n",
			out_file, strerror(errno));
		close(fd_in);
		return EXIT_FAILURE;
	}
	
	if (save_trace(fd_in, outf) != 0) {
		fprintf(stderr, "Failed to save the trace.\n");
		ret = EXIT_FAILURE;
	}
	
	close(fd_in);
	fclose(outf);
	return ret;
}
/* ====================================================================== */
