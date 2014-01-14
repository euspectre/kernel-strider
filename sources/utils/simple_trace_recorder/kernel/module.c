/* ========================================================================
 * Copyright (C) 2013-2014, ROSA Laboratory
 * Copyright (C) 2012, KEDR development team
 * Authors: 
 *      Eugene A. Shatokhin
 *      Andrey V. Tsyvarev
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published
 * by the Free Software Foundation.
 ======================================================================== */

/* This module outputs the information about the events it receives from the 
 * core to a buffer attached to a file in debugfs 
 * ("kedr_simple_trace_recorder/buffer"). A user-space application
 * could poll that file and when this kernel module indicates that the 
 * data are available, mmap that file to retrieve the data. Note that read()
 * and write() should not be used for this file.
 *
 * [NB] If more than one process operates on the file simultaneously, the 
 * behaviour is undefined. Using several processes to read data from the 
 * same buffer makes no sense anyway.
 * 
 * The buffer consists of 2^N + 1 pages. The first page is used for the 
 * service data (current read and write positions, etc.). The remaining 2^N
 * pages are called "data pages", they actually contain the event 
 * structures.
 * 
 * [NB] The module is not required to notify the user-space part about each 
 * new event stored in the buffer. This is done for each 'notify_mark' pages
 * written and also when "session end" event is received. 
 *
 * Notes for developers.
 * Some of the events written to the output buffer are compressed, see 
 * recorder.h for details.
 * 3 buffers are involved: 
 * - B0 - accumulator
 * - B1 - temporary storage for compressed data
 * - B2 - the output buffer visible from the user space.
 * 
 * If an event structure is created in the output buffer directly (e.g.
 * "session end"), it cannot cross page boundary because the buffer may be
 * non-contiguous from the kernel's point of view.
 * If an event structure is first created somewhere else and is then copied
 * to the output buffer (e.g. a compressed event), this restriction does not
 * apply. The copying procedure must take the structure of the output buffer
 * in account, of course. 
 * 
 * To serialize the accesses to the buffers B0, B1 and B2 from this module, 
 * 'eh_lock' must be used. */

#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/init.h>
#include <linux/errno.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/spinlock.h>
#include <linux/mutex.h>
#include <linux/debugfs.h>
#include <linux/bug.h>		/* BUG_ON */
#include <linux/log2.h>		/* is_power_of_2 */
#include <linux/mm.h>
#include <linux/poll.h>
#include <linux/wait.h>
#include <linux/sched.h>
#include <linux/vmalloc.h>
#include <linux/string.h>
#include <linux/lzo.h>		/* LZO1X compression support */

#include <kedr/kedr_mem/core_api.h>
#include <kedr/object_types.h>

#include <simple_trace_recorder/recorder.h>
#include <kedr_st_rec_config.h>
/* ====================================================================== */

#define KEDR_MSG_PREFIX "[" KEDR_ST_REC_KMODULE_NAME "] "
/* ====================================================================== */

MODULE_AUTHOR("Eugene A. Shatokhin");
MODULE_LICENSE("GPL");
/* ====================================================================== */

#define KEDR_TR_NO_SPACE (__u32)(-1)

/* If you need really large data buffers (> 256Mb), you can try increasing
 * this limit although it is not recommended (the more kernel memory we 
 * require the more problems for the whole system). */
#define KEDR_TR_MAX_DATA_PAGES 65536
#define KEDR_TR_B0_DATA_PAGES 32

/* Number of data pages in the buffer B0. Must be a power of 2 because
 * 'nr_data_pages' (see below) must be a power of 2 as well. 
 * Do not use large numbers here because the compression of B0 may 
 * occasionally be done in interrupt context. */
static unsigned int b0_nr_data_pages = KEDR_TR_B0_DATA_PAGES;

/* Number of data pages in the output buffer. Must be a power of 2. Must be 
 * less than or equal to KEDR_TR_MAX_DATA_PAGES but no less than 
 * 2 * b0_nr_data_pages because B1 must at least fit in. The size of B1 is
 * always less than 2 * sizeof(B0). */
unsigned int nr_data_pages = 4 * KEDR_TR_B0_DATA_PAGES;
module_param(nr_data_pages, uint, S_IRUGO);

/* For each 'notify_mark' data pages filled in the buffer, this module wakes
 * up the process waiting (in poll()) for the data to become available for
 * reading. */
static unsigned int notify_mark = 1;

/* If non-zero, function entry/exit and call pre/post events will not be
 * recorded in the trace. 
 * This can be used to reduce the intensity of the event stream and the size
 * of the trace if there are much more such events than memory and
 * synchronization events. 
 * Note that call-related events are often used to maintain the call stack 
 * information. If recording of such events is disabled, that information 
 * will not be available, only the address of the instruction that generated
 * the event will be in the trace. */
int no_call_events = 0;
module_param(no_call_events, int, S_IRUGO);
/* ====================================================================== */

/* A directory for the module in debugfs and the file needed to access the
 * buffer. */
static struct dentry *debugfs_dir_dentry = NULL;
static const char *debugfs_dir_name = KEDR_ST_REC_KMODULE_NAME;

static struct dentry *buffer_file = NULL;
static const char *buffer_file_name = "buffer";

/* A mutex to serialize operations with the buffer file. */
static DEFINE_MUTEX(buffer_file_mutex);

/* The buffer B0. */
static void *b0_buffer = NULL;

/* The buffer B1. */
static void *b1_buffer = NULL;

/* The output buffer (B2). */
static unsigned long *page_buffer = NULL;

/* The first page of the output buffer, contains service data. */
static struct kedr_tr_start_page *start_page = NULL;

/* The total size of the data in the buffer B0. */
static unsigned int b0_data_size;

/* The total number of events stored in B0, OR compressed in B1 */
static unsigned int cached_events_num;

/* The total size of the data pages in the output buffer. */
static unsigned int buffer_size;

/* 0 if the file has been opened, non-zero otherwise. */
static atomic_t buffer_file_available = ATOMIC_INIT(1);

/* Set this to a non-zero value to indicate that the next call to poll() 
 * method should report the data is available even if the amount of the data  
 * is less than 'notify_mark' defines. 
 * This allows to make sure that as soon as the reader calls poll() again
 * it will be notified that the target module has been unloaded and will not
 * wait.
 *
 * The accesses to this variable must be protected by 'eh_lock'. */
static int signal_on_next_poll = 0;

/* The LZO1X compressor working memory */
static void *lzo_wrkmem = NULL;
/* ====================================================================== */

/* A wait queue for the reader to wait on until enough data become 
 * available. */
static DECLARE_WAIT_QUEUE_HEAD(reader_queue);
/* ====================================================================== */

/* A spinlock to serialize the accesses to the output buffer, as well as the
 * buffers B0 and B1 used for data compression, from the event handlers. */
static DEFINE_SPINLOCK(eh_lock);
/* ====================================================================== */

/* Number of the events that could not be stored in the buffer due
 * to the insufficient space in it. The user-space reader 
 * application probably did not keep up with the speed the data were
 * written to the buffer. 
 *
 * [NB] When the buffer is full, the subsequent events are discarded. 
 *
 * This counter should be updated with 'eh_lock' taken. */
static u64 events_lost = 0;

/* The file in debugfs for this counter. */
static struct dentry *events_lost_file = NULL;
/* ====================================================================== */

/* Are there at least 'notify_mark' pages of data available for reading in 
 * the buffer? 
 * 'wp' and 'rp' are the current read and write positions in the buffer.
 * Must be called with 'eh_lock' locked. */
static int
enough_data_available(__u32 wp, __u32 rp)
{
	unsigned int available = (wp - rp) & (buffer_size - 1);
	return (available >= (notify_mark << PAGE_SHIFT));
}

/* Must be called with 'eh_lock' locked. 
 * Note that the reader may miss the notification if it is not waiting on 
 * 'reader_queue' at the moment. This should not be a problem, though: as 
 * long as there is a reason to wake up the reader, the notifications will
 * be sent again and again. wake_up() should be rather cheap 
 * performance-wise if there are no processes to wake up. */
static void
notify_reader(void)
{
	wake_up(&reader_queue);
}
/* ====================================================================== */

/* Use this function to properly retrieve the value of 'read_pos'. Do not 
 * attempt to use 'start_page->read_pos' directly. */
static __u32
get_read_pos(void)
{
	__u32 read_pos = start_page->read_pos;
	smp_mb();
	return read_pos;
}

/* Use this function after writing to the buffer to properly set 'write_pos'
 * and to make sure other CPUs will see this write to 'write_pos' only 
 * after these writes to the buffer.
 * 
 * The function notifies the reader if there is enough data available.
 * 'rp' - read position as it was before the writing to the buffer began.
 * 
 * Must be called with 'eh_lock' locked. */
static void
set_write_pos_and_notify(__u32 new_write_pos, __u32 rp)
{
	/* Make sure all writes to the buffer have completed before we
	 * update 'write_pos'. */
	smp_wmb();
	start_page->write_pos = new_write_pos;
	
	if (enough_data_available(new_write_pos, rp))
		notify_reader();
}

/* Returns non-zero if the buffer has enough space for a data chunk of size
 * 'size', 0 otherwise. 
 * 'wp' and 'rp' are the current write and read positions, respectively. 
 * 
 * [NB] The buffer is considered totally full when '(wp + 1) mod buffer_size'
 * equals 'rp' ("wp is right behind rp"). If there is no space in the buffer
 * for a large event, the event is discarded. Some of the subsequent events 
 * still may make it to the buffer if they fit into the remaining space. 
 *
 * Must be called with 'eh_lock' locked. */
static int
buffer_has_space(__u32 wp, __u32 rp, unsigned int size)
{
	unsigned int mask = buffer_size - 1;
	unsigned int wp_dist;
	unsigned int end_dist;
	
	/* [NB] 'size < buffer_size'. */
	wp_dist = (wp - rp) & mask;
	end_dist = (wp + size - rp) & mask;
	
	return (end_dist >= wp_dist);
}

static int
b0_buffer_has_space(unsigned int size)
{
	unsigned long b0_buffer_space = b0_nr_data_pages << PAGE_SHIFT;
	return (b0_buffer_space - b0_data_size >= size);
}

/* Returns the address of a memory location in the buffer corresponding to
 * the given position. It is OK for 'pos' to be greater than or equal to 
 * 'buffer_size', 'pos' modulo 'buffer_size' is the corresponding position
 * in the buffer in this case. */
static void *
buffer_pos_to_addr(__u32 pos)
{
	unsigned int page_idx;
	unsigned int offset = (unsigned int)pos & (PAGE_SIZE - 1);
	
	pos &= (buffer_size - 1);
	/* Data pages start from #1 in 'page_buffer', hence +1 here. */
	page_idx = ((unsigned int)pos >> PAGE_SHIFT) + 1;
	
	return (void *)(page_buffer[page_idx] + offset);
}

/* The area in B0 where the next event structure should be written. */
static void *
b0_buffer_write_pos(void)
{
	return (void *)((unsigned long)b0_buffer + b0_data_size);
}

/* Returns non-zero if a record of the given size would not cross page 
 * boundary when written to the buffer at the position 'wp'; 0 otherwise. 
 * 
 * Must be called with 'eh_lock' locked. */
static int
fits_to_page(__u32 wp, unsigned int size)
{
	unsigned int offset = (unsigned int)wp & (PAGE_SIZE - 1);
	return (offset + size <= PAGE_SIZE);
}

/* This function should be called if fits_to_page() returns 0 for an object.
 * The function returns the position corresponding to the next page and, if
 * possible, writes a special event to the current page to indicate that
 * the reader should skip to the next page. 
 * 
 * Must be called with 'eh_lock' locked. */
static __u32
complete_buffer_page(__u32 wp)
{
	if (fits_to_page(wp, sizeof(struct kedr_tr_event_header))) {
		struct kedr_tr_event_header *h;
		h = buffer_pos_to_addr(wp);
		h->type = KEDR_TR_EVENT_SKIP;
		h->event_size = 0; /* all fields must be filled */
	}
	return ((wp + PAGE_SIZE) & ~(PAGE_SIZE - 1));
}

/* Performs the common operations needed before writing a record to the 
 * buffer: checks if there is available space, deals with the page 
 * boundaries, etc. 
 * 'wp' and 'rp' are the write and read positions in the buffer, 
 * respectively; 'size' is the size of the record.
 * If there is enough space in the buffer to write the record, the function 
 * will return the position where the record should be written. Otherwise,
 * the function will return KEDR_TR_NO_SPACE, which means that the event is 
 * lost. 
 *
 * Must be called with 'eh_lock' locked. */
static __u32
record_write_common(__u32 wp, __u32 rp, unsigned int size)
{
	if (!buffer_has_space(wp, rp, size)) {
		++events_lost;
		return KEDR_TR_NO_SPACE;
	}
	
	if (!fits_to_page(wp, size)) {
		wp = complete_buffer_page(wp);
		if (!buffer_has_space(wp, rp, size)) {
			++events_lost;
			set_write_pos_and_notify(wp, rp);
			return KEDR_TR_NO_SPACE;
		}
	}
	return wp;
}

static __u32
lzo1x_compress_buf(void *buf, size_t buf_size)
{
	__u32 event_size;
	size_t compressed_size;
	struct kedr_tr_event_compressed *ec = b1_buffer;
	int ret;
	
	ret = lzo1x_1_compress(buf, buf_size, &ec->compressed[0], 
			       &compressed_size, lzo_wrkmem);
	if (ret != LZO_E_OK) {
		pr_warning(KEDR_MSG_PREFIX 
			"lzo1x_compress_buf() failed, error: %d.\n", ret);
		return 0;
	}

	event_size = sizeof(struct kedr_tr_event_compressed) - 1 + 
		compressed_size;
	ec->header.type = KEDR_TR_EVENT_COMPRESSED;
	ec->header.event_size = event_size;
	ec->orig_size = buf_size;
	ec->compressed_size = compressed_size;
	return event_size;
}

/* Compress the contents of B0 to B1 and copy the result to the output 
 * buffer if there is enough space there. Otherwise, the events from B0 are 
 * considered lost. After this function completes, B0 and B1 will be 
 * available as if they were empty again. */
static void
compress_b0_to_output(void)
{
	__u32 wp;
	__u32 rp;
	__u32 nbytes;
	__u32 pos = 0;
	void *where = NULL;

	rp = get_read_pos();
	wp = start_page->write_pos;
	
	nbytes = lzo1x_compress_buf(b0_buffer, b0_data_size);
	b0_data_size = 0; /* Mark the buffer empty. */
	
	if (nbytes == 0 || !buffer_has_space(wp, rp, nbytes)) { 
		events_lost += cached_events_num;
		cached_events_num = 0;
		return;
	}

	/* Write the event page by page. Note that it is not needed to start
	 * from a page boundary here. */
	while (nbytes != 0) {
		__u32 next_page = (wp + PAGE_SIZE) & ~(PAGE_SIZE - 1);
		__u32 avail = next_page - wp;
		__u32 to_write = (nbytes > avail) ? avail : nbytes;
		
		where = buffer_pos_to_addr(wp);
		memcpy(where, b1_buffer + pos, to_write);
		pos += to_write;
		wp += to_write;
		nbytes -= to_write;
	}
	cached_events_num = 0;
	set_write_pos_and_notify(wp, rp);
}
/* ====================================================================== */

static int 
buffer_mmap_fault(struct vm_area_struct *vma, struct vm_fault *vmf)
{
	/* Write access only makes sense for the first page of the buffer
	 * but not for the data pages. This is a weak check though, as it
	 * only happens at the first attempt to access the page. */
	if (vmf->pgoff != 0 && (vmf->flags & FAULT_FLAG_WRITE))
		return VM_FAULT_SIGBUS;
	
	if (vmf->pgoff >= nr_data_pages + 1)
		return VM_FAULT_SIGBUS;
	
	vmf->page = virt_to_page((void *)page_buffer[vmf->pgoff]);
	if (!vmf->page)
		return VM_FAULT_SIGBUS;
	
	get_page(vmf->page);
	return 0;
}

static struct vm_operations_struct buffer_mmap_vm_ops = {
	.fault = buffer_mmap_fault,
};


static int 
buffer_file_mmap(struct file *file, struct vm_area_struct *vma)
{
	unsigned int nr_map_pages;
	nr_map_pages = (vma->vm_end - vma->vm_start) >> PAGE_SHIFT;
	
	if (nr_map_pages != nr_data_pages + 1) {
		pr_warning(KEDR_MSG_PREFIX 
	"An attempt to map %u pages while the buffer has %u.\n",
			nr_map_pages, nr_data_pages + 1);
		return -EINVAL;
	}
	
	vma->vm_ops = &buffer_mmap_vm_ops;
	vma->vm_flags |= VM_IO;
	return 0;
}

/* Make sure the file cannot be opened again (by another application or an 
 * instance of the same application) if it is already open. 
 * Note that it does not guarantee that no operations with this file will
 * execute simultaneously. For example, if the application is multithreaded
 * or creates new processes, these new threads/processes might be able to 
 * operate on this file without opening it explicitly. 
 * Still, this "single open" technique gives some protection which may help,
 * for example if (unintentionally) several user-space trace readers are 
 * launched. */
static int 
buffer_file_open(struct inode *inode, struct file *filp)
{
	if (!atomic_dec_and_test(&buffer_file_available)) {
		/* Some process has already opened this file. */
		atomic_inc(&buffer_file_available);
		return -EBUSY;
	}
	return nonseekable_open(inode, filp);
}

static int
buffer_file_release(struct inode *inode, struct file *filp)
{
	atomic_inc(&buffer_file_available); /* Release the file. */
	return 0;
}

/* read() and write() system calls should not be used for the file. The
 * file operations are provided here just in case. 
 * A user-space application should use poll() to wait until the data become 
 * available and then - mmap() to get to the data. */
static ssize_t 
buffer_file_write(struct file *filp, const char __user *buf, size_t count,
	loff_t *f_pos)
{
	return -EINVAL;
}

static ssize_t 
buffer_file_read(struct file *filp, char __user *buf, size_t count,
	loff_t *f_pos)
{
	return -EINVAL;
}

static unsigned int 
buffer_file_poll(struct file *filp, poll_table *wait)
{
	unsigned int ret = 0;
	unsigned long irq_flags;
	
	poll_wait(filp, &reader_queue, wait);
	
	spin_lock_irqsave(&eh_lock, irq_flags);
	if (signal_on_next_poll) {
		ret = POLLIN | POLLRDNORM;
		signal_on_next_poll = 0;
		goto out;
	}
	
	/* If there are already enough data available, notify the caller, 
	 * so that it would not sleep needlessly. */
	if (enough_data_available(start_page->write_pos, get_read_pos()))
		ret = POLLIN | POLLRDNORM;
out:	
	spin_unlock_irqrestore(&eh_lock, irq_flags);
	return ret;
}

static const struct file_operations buffer_file_ops = {
	.owner 		= THIS_MODULE,
	.open 		= buffer_file_open,
	.release 	= buffer_file_release,
	.read 		= buffer_file_read,
	.write		= buffer_file_write,
	.mmap		= buffer_file_mmap,
	.poll		= buffer_file_poll,
};
/* ====================================================================== */

static void
handle_session_event_impl(enum kedr_tr_event_type et)
{
	__u32 wp;
	__u32 rp;
	unsigned long irq_flags;
	struct kedr_tr_event_session *ev;
	unsigned int size = (unsigned int)sizeof(*ev);

	spin_lock_irqsave(&eh_lock, irq_flags);
	
	/* If session is ending, output the events accumulated in B0. */
	if (et == KEDR_TR_EVENT_SESSION_END && cached_events_num != 0) {
		/* B0 -> [LZO] -> B1 => B2 */
		compress_b0_to_output();
	}

	rp = get_read_pos();
	wp = record_write_common(start_page->write_pos, rp, size);
	if (wp == KEDR_TR_NO_SPACE)
		goto out;

	ev = buffer_pos_to_addr(wp);
	ev->header.type = et;
	ev->header.event_size = size;

	wp += size;
	set_write_pos_and_notify(wp, rp);

	if (et == KEDR_TR_EVENT_SESSION_END) {
		/* This helps if the reader is not currently waiting... */
		signal_on_next_poll = 1;

		/* ...and this - if it is. */
		notify_reader();
	}
	else {
		signal_on_next_poll = 0;
	}

out:
	spin_unlock_irqrestore(&eh_lock, irq_flags);
}

static void
handle_load_unload_impl(enum kedr_tr_event_type et, struct module *mod)
{
	unsigned long irq_flags;
	struct kedr_tr_event_module *ev;
	unsigned int size = (unsigned int)sizeof(*ev);

	/* [NB] It is OK to access the fields of 'mod' here because the
	 * target module cannot go away while "target load" and "target
	 * unload" handlers are executed. Therefore, 'mod' remains valid,
	 * the core ensures that. */
	
	spin_lock_irqsave(&eh_lock, irq_flags);
	if (!b0_buffer_has_space(size))
		compress_b0_to_output();

	ev = b0_buffer_write_pos();
	memset(ev, 0, size);
	
	ev->header.type = et;
	ev->header.event_size = size;
	strncpy(ev->name, module_name(mod), KEDR_TARGET_NAME_LEN);

	if (et == KEDR_TR_EVENT_TARGET_LOAD) {
		ev->init_addr = (__u32)(unsigned long)mod->module_init;
		if (ev->init_addr != 0)
			ev->init_size = (__u32)mod->init_text_size;

		ev->core_addr = (__u32)(unsigned long)mod->module_core;
		if (ev->core_addr != 0)
			ev->core_size = (__u32)mod->core_text_size;
	}
	
	++cached_events_num;
	b0_data_size += size;

	spin_unlock_irqrestore(&eh_lock, irq_flags);
}

static void
handle_function_event_impl(enum kedr_tr_event_type et, unsigned long tid, 
	unsigned long func)
{
	unsigned long irq_flags;
	struct kedr_tr_event_func *ev;
	unsigned int size = (unsigned int)sizeof(*ev);

	if (no_call_events)
		return;
	
	spin_lock_irqsave(&eh_lock, irq_flags);
	if (!b0_buffer_has_space(size))
		compress_b0_to_output();

	ev = b0_buffer_write_pos();
	ev->header.type = et;
	ev->header.event_size = size;
	ev->tid = (__u64)tid;
	ev->func = (__u32)func;

	++cached_events_num;
	b0_data_size += size;

	spin_unlock_irqrestore(&eh_lock, irq_flags);
}

static void 
handle_call_impl(enum kedr_tr_event_type et, unsigned long tid, 
	unsigned long pc, unsigned long func)
{
	unsigned long irq_flags;
	struct kedr_tr_event_call *ev;
	unsigned int size = (unsigned int)sizeof(*ev);	
	
	if (no_call_events)
		return;
	
	spin_lock_irqsave(&eh_lock, irq_flags);
	if (!b0_buffer_has_space(size))
		compress_b0_to_output();

	ev = b0_buffer_write_pos();
	ev->header.type = et;
	ev->header.event_size = size;
	ev->tid = (__u64)tid;
	ev->func = (__u32)func;
	ev->pc = (__u32)pc;

	++cached_events_num;
	b0_data_size += size;

	spin_unlock_irqrestore(&eh_lock, irq_flags);
}

static void
on_session_start(struct kedr_event_handlers *eh)
{
	handle_session_event_impl(KEDR_TR_EVENT_SESSION_START);
}

static void
on_session_end(struct kedr_event_handlers *eh)
{
	handle_session_event_impl(KEDR_TR_EVENT_SESSION_END);
}

static void 
on_load(struct kedr_event_handlers *eh, struct module *mod)
{
	handle_load_unload_impl(KEDR_TR_EVENT_TARGET_LOAD, mod);
}

static void 
on_unload(struct kedr_event_handlers *eh, struct module *mod)
{
	handle_load_unload_impl(KEDR_TR_EVENT_TARGET_UNLOAD, mod);
}

static void 
on_function_entry(struct kedr_event_handlers *eh, unsigned long tid, 
	unsigned long func)
{
	handle_function_event_impl(KEDR_TR_EVENT_FENTRY, tid, func);
}

static void 
on_function_exit(struct kedr_event_handlers *eh, unsigned long tid, 
	unsigned long func)
{
	handle_function_event_impl(KEDR_TR_EVENT_FEXIT, tid, func);
}

static void 
on_call_pre(struct kedr_event_handlers *eh, unsigned long tid, 
	unsigned long pc, unsigned long func)
{
	handle_call_impl(KEDR_TR_EVENT_CALL_PRE, tid, pc, func);
}

static void 
on_call_post(struct kedr_event_handlers *eh, unsigned long tid, 
	unsigned long pc, unsigned long func)
{
	handle_call_impl(KEDR_TR_EVENT_CALL_POST, tid, pc, func);
}

static void 
begin_memory_events(struct kedr_event_handlers *eh, unsigned long tid, 
	unsigned long num_events, void **pdata)
{
	struct kedr_tr_event_mem *ev;
	
	ev = kzalloc(sizeof(struct kedr_tr_event_mem) + 
		(num_events - 1) * sizeof(struct kedr_tr_event_mem_op), 
		GFP_ATOMIC);
	if (ev == NULL) {
		pr_warning(KEDR_MSG_PREFIX 
"begin_memory_events(): not enough memory to record %lu access(es).\n",
			num_events);
		*pdata = NULL;
		return;
	}
	
	/* ev->header.nr_events is now 0 */
	ev->header.type = KEDR_TR_EVENT_MEM;
	ev->tid = (__u64)tid;
	*pdata = ev; 
}

static void 
on_memory_event(struct kedr_event_handlers *eh, unsigned long tid, 
	unsigned long pc, unsigned long addr, unsigned long size, 
	enum kedr_memory_event_type type,
	void *data)
{
	struct kedr_tr_event_mem *ev = (struct kedr_tr_event_mem *)data;
	__u32 event_bit;
	unsigned int nr;
	
	if (addr == 0 || ev == NULL)
		return;
	
	nr = ev->nr_events;
	event_bit = 1 << nr;
	
	ev->mem_ops[nr].addr = (__u64)addr;
	ev->mem_ops[nr].size = (__u32)size;
	ev->mem_ops[nr].pc   = (__u32)pc;
	
	switch (type) {
	case KEDR_ET_MREAD:
		ev->read_mask |= event_bit;
		break;
	case KEDR_ET_MWRITE:
		ev->write_mask |= event_bit;
		break;
	case KEDR_ET_MUPDATE:
		ev->read_mask |= event_bit;
		ev->write_mask |= event_bit;
		break;
	default:
		pr_warning(KEDR_MSG_PREFIX 
	"on_memory_event(): unknown type of memory access: %d.\n",
			(int)type);
	};
	
	++ev->nr_events;
}

static void
report_block_enter_event(__u64 tid, __u32 pc)
{
	unsigned long irq_flags;
	struct kedr_tr_event_block *ev;
	unsigned int size = (unsigned int)sizeof(*ev);	
	
	spin_lock_irqsave(&eh_lock, irq_flags);
	if (!b0_buffer_has_space(size))
		compress_b0_to_output();

	ev = b0_buffer_write_pos();
	ev->header.type = KEDR_TR_EVENT_BLOCK_ENTER;
	ev->header.event_size = size;
	ev->tid = tid;
	ev->pc = pc;

	++cached_events_num;
	b0_data_size += size;

	spin_unlock_irqrestore(&eh_lock, irq_flags);
}

static void
end_memory_events(struct kedr_event_handlers *eh, unsigned long tid, 
	void *data)
{
	unsigned long irq_flags;
	unsigned int size;
	struct kedr_tr_event_mem *ev = (struct kedr_tr_event_mem *)data;
	void *where = NULL;
	
	if (ev == NULL || ev->nr_events == 0) {
		kfree(ev);
		return;
	}
	
	report_block_enter_event(ev->tid, ev->mem_ops[0].pc);
	
	size = sizeof(struct kedr_tr_event_mem) + 
		(ev->nr_events - 1) * sizeof(struct kedr_tr_event_mem_op);
	ev->header.event_size = size;
	
	spin_lock_irqsave(&eh_lock, irq_flags);
	if (!b0_buffer_has_space(size))
		compress_b0_to_output();

	where = b0_buffer_write_pos();
	memcpy(where, ev, size);
	
	++cached_events_num;
	b0_data_size += size;

	spin_unlock_irqrestore(&eh_lock, irq_flags);
	kfree(ev);
}

static void
handle_locked_and_io_impl(enum kedr_tr_event_type et, unsigned long tid, 
	unsigned long pc, unsigned long addr, unsigned long sz, 
	enum kedr_memory_event_type type)
{
	unsigned long irq_flags;
	struct kedr_tr_event_mem *ev;
	unsigned int size = (unsigned int)sizeof(*ev);	
	
	spin_lock_irqsave(&eh_lock, irq_flags);
	if (!b0_buffer_has_space(size))
		compress_b0_to_output();

	ev = b0_buffer_write_pos();
	ev->header.type = et;
	ev->header.event_size = size;
	ev->nr_events = 1;
	ev->tid = (__u64)tid;
	
	ev->mem_ops[0].addr = (__u64)addr;
	ev->mem_ops[0].size = (__u32)sz;
	ev->mem_ops[0].pc   = (__u32)pc;
	
	switch (type) {
	case KEDR_ET_MREAD:
		ev->read_mask = 1;
		break;
	case KEDR_ET_MWRITE:
		ev->write_mask = 1;
		break;
	case KEDR_ET_MUPDATE:
		ev->read_mask = 1;
		ev->write_mask = 1;
		break;
	default:
		pr_warning(KEDR_MSG_PREFIX 
	"handle_locked_and_io_impl(): unknown type of memory access: %d.\n",
			(int)type);
	};

	++cached_events_num;
	b0_data_size += size;

	spin_unlock_irqrestore(&eh_lock, irq_flags);
}

static void
on_locked_op_post(struct kedr_event_handlers *eh, unsigned long tid, 
	unsigned long pc, unsigned long addr, unsigned long size, 
	enum kedr_memory_event_type type, void *data)
{
	handle_locked_and_io_impl(KEDR_TR_EVENT_MEM_LOCKED, tid, pc, addr,
		size, type);
}

static void
on_io_mem_op_post(struct kedr_event_handlers *eh, unsigned long tid, 
	unsigned long pc, unsigned long addr, unsigned long size, 
	enum kedr_memory_event_type type, void *data)
{
	handle_locked_and_io_impl(KEDR_TR_EVENT_MEM_IO, tid, pc, addr, size,
		type);
}

static void
handle_memory_barrier_impl(enum kedr_tr_event_type et, unsigned long tid, 
	unsigned long pc, enum kedr_barrier_type type)
{
	unsigned long irq_flags;
	struct kedr_tr_event_barrier *ev;
	unsigned int size = (unsigned int)sizeof(*ev);	
	
	spin_lock_irqsave(&eh_lock, irq_flags);
	if (!b0_buffer_has_space(size))
		compress_b0_to_output();

	ev = b0_buffer_write_pos();
	ev->header.type = et;
	ev->header.event_size = size;
	ev->obj_type = (__u32)type;
	ev->tid = (__u64)tid;
	ev->pc = pc;

	++cached_events_num;
	b0_data_size += size;

	spin_unlock_irqrestore(&eh_lock, irq_flags);
}

static void
on_memory_barrier_pre(struct kedr_event_handlers *eh, unsigned long tid, 
	unsigned long pc, enum kedr_barrier_type type)
{
	handle_memory_barrier_impl(KEDR_TR_EVENT_BARRIER_PRE, tid, pc, 
		type);
}

static void
on_memory_barrier_post(struct kedr_event_handlers *eh, unsigned long tid, 
	unsigned long pc, enum kedr_barrier_type type)
{
	handle_memory_barrier_impl(KEDR_TR_EVENT_BARRIER_POST, tid, pc, 
		type);
}

static void 
handle_alloc_free_impl(enum kedr_tr_event_type et, unsigned long tid,
	unsigned long pc, unsigned long sz, unsigned long addr)
{
	unsigned long irq_flags;
	struct kedr_tr_event_alloc_free *ev;
	unsigned int size = (unsigned int)sizeof(*ev);	
	
	spin_lock_irqsave(&eh_lock, irq_flags);
	if (!b0_buffer_has_space(size))
		compress_b0_to_output();

	ev = b0_buffer_write_pos();
	ev->header.type = et;
	ev->header.event_size = size;
	ev->tid = (__u64)tid;
	ev->pc = pc;
	ev->size = sz;
	ev->addr = (__u64)addr;

	++cached_events_num;
	b0_data_size += size;

	spin_unlock_irqrestore(&eh_lock, irq_flags);
}

static void 
on_alloc_pre(struct kedr_event_handlers *eh, unsigned long tid,
	unsigned long pc, unsigned long size)
{
	handle_alloc_free_impl(KEDR_TR_EVENT_ALLOC_PRE, tid, pc, size, 0);
}

static void 
on_alloc_post(struct kedr_event_handlers *eh, unsigned long tid,
	unsigned long pc, unsigned long size, unsigned long addr)
{
	handle_alloc_free_impl(KEDR_TR_EVENT_ALLOC_POST, tid, pc, size, 
		addr);
}

static void 
on_free_pre(struct kedr_event_handlers *eh, unsigned long tid,
	unsigned long pc, unsigned long addr)
{
	handle_alloc_free_impl(KEDR_TR_EVENT_FREE_PRE, tid, pc, 0, addr);
}

static void 
on_free_post(struct kedr_event_handlers *eh, unsigned long tid,
	unsigned long pc, unsigned long addr)
{
	handle_alloc_free_impl(KEDR_TR_EVENT_FREE_POST, tid, pc, 0, addr);
}

static void
handle_sync_event_impl(enum kedr_tr_event_type et, unsigned long tid,
	unsigned long pc, unsigned long obj_id, unsigned int obj_type)
{
	unsigned long irq_flags;
	struct kedr_tr_event_sync *ev;
	unsigned int size = (unsigned int)sizeof(*ev);	
	
	spin_lock_irqsave(&eh_lock, irq_flags);
	if (!b0_buffer_has_space(size))
		compress_b0_to_output();

	ev = b0_buffer_write_pos();
	ev->header.type = et;
	ev->header.event_size = size;
	ev->obj_type = (__u32)obj_type;
	ev->tid = (__u64)tid;
	ev->obj_id = (__u64)obj_id;
	ev->pc = pc;

	++cached_events_num;
	b0_data_size += size;

	spin_unlock_irqrestore(&eh_lock, irq_flags);
}

static void 
on_lock_pre(struct kedr_event_handlers *eh, unsigned long tid,
	unsigned long pc, unsigned long lock_id, enum kedr_lock_type type)
{
	handle_sync_event_impl(KEDR_TR_EVENT_LOCK_PRE, tid, pc, lock_id, 
		type);
}

static void 
on_lock_post(struct kedr_event_handlers *eh, unsigned long tid,
	unsigned long pc, unsigned long lock_id, enum kedr_lock_type type)
{
	handle_sync_event_impl(KEDR_TR_EVENT_LOCK_POST, tid, pc, lock_id, 
		type);
}

static void 
on_unlock_pre(struct kedr_event_handlers *eh, unsigned long tid,
	unsigned long pc, unsigned long lock_id, enum kedr_lock_type type)
{
	handle_sync_event_impl(KEDR_TR_EVENT_UNLOCK_PRE, tid, pc, lock_id, 
		type);
}

static void 
on_unlock_post(struct kedr_event_handlers *eh, unsigned long tid,
	unsigned long pc, unsigned long lock_id, enum kedr_lock_type type)
{
	handle_sync_event_impl(KEDR_TR_EVENT_UNLOCK_POST, tid, pc, lock_id, 
		type);
}

static void 
on_signal_pre(struct kedr_event_handlers *eh, unsigned long tid,
	unsigned long pc, unsigned long obj_id, 
	enum kedr_sw_object_type type)
{
	handle_sync_event_impl(KEDR_TR_EVENT_SIGNAL_PRE, tid, pc, obj_id,
		type);
}

static void 
on_signal_post(struct kedr_event_handlers *eh, unsigned long tid,
	unsigned long pc, unsigned long obj_id, 
	enum kedr_sw_object_type type)
{
	handle_sync_event_impl(KEDR_TR_EVENT_SIGNAL_POST, tid, pc, obj_id,
		type);
}

static void 
on_wait_pre(struct kedr_event_handlers *eh, unsigned long tid,
	unsigned long pc, unsigned long obj_id, 
	enum kedr_sw_object_type type)
{
	handle_sync_event_impl(KEDR_TR_EVENT_WAIT_PRE, tid, pc, obj_id,
		type);
}

static void 
on_wait_post(struct kedr_event_handlers *eh, unsigned long tid,
	unsigned long pc, unsigned long obj_id, 
	enum kedr_sw_object_type type)
{
	handle_sync_event_impl(KEDR_TR_EVENT_WAIT_POST, tid, pc, obj_id,
		type);
}

static void
on_thread_start(struct kedr_event_handlers *eh, unsigned long tid,
	const char *comm)
{
	unsigned long irq_flags;
	struct kedr_tr_event_tstart *ev;
	unsigned int size = (unsigned int)sizeof(*ev);

	spin_lock_irqsave(&eh_lock, irq_flags);
	if (!b0_buffer_has_space(size))
		compress_b0_to_output();

	ev = b0_buffer_write_pos();
	memset(ev, 0, size);
	
	ev->header.type = KEDR_TR_EVENT_THREAD_START;
	ev->header.event_size = size;
	ev->tid = (__u64)tid;

	/* The trailing 0 has been already written by memset. */
	strncpy(&ev->comm[0], comm, KEDR_COMM_LEN);

	++cached_events_num;
	b0_data_size += size;

	spin_unlock_irqrestore(&eh_lock, irq_flags);

}

static void
on_thread_end(struct kedr_event_handlers *eh, unsigned long tid)
{
	unsigned long irq_flags;
	struct kedr_tr_event_tend *ev;
	unsigned int size = (unsigned int)sizeof(*ev);

	spin_lock_irqsave(&eh_lock, irq_flags);
	if (!b0_buffer_has_space(size))
		compress_b0_to_output();

	ev = b0_buffer_write_pos();
	memset(ev, 0, size);

	ev->header.type = KEDR_TR_EVENT_THREAD_END;
	ev->header.event_size = size;
	ev->tid = (__u64)tid;

	++cached_events_num;
	b0_data_size += size;

	spin_unlock_irqrestore(&eh_lock, irq_flags);
}

struct kedr_event_handlers eh = {
	.owner 			= THIS_MODULE,

	.on_session_start	= on_session_start,
	.on_session_end		= on_session_end,

	.on_target_loaded 	= on_load,
	.on_target_about_to_unload = on_unload,

	.on_function_entry 	= on_function_entry,
	.on_function_exit 	= on_function_exit,
	.on_call_pre 		= on_call_pre,
	.on_call_post		= on_call_post,

	.begin_memory_events	= begin_memory_events,
	.end_memory_events	= end_memory_events,
	.on_memory_event	= on_memory_event,

	/* We do not need to set pre handlers for locked memory operations
	 * and I/O operations accessing memory, post handlers are enough. */
	.on_locked_op_post	= on_locked_op_post,
	.on_io_mem_op_post	= on_io_mem_op_post,

	.on_memory_barrier_pre	= on_memory_barrier_pre,
	.on_memory_barrier_post	= on_memory_barrier_post,
	
	.on_alloc_pre 		= on_alloc_pre,
	.on_alloc_post 		= on_alloc_post,
	.on_free_pre 		= on_free_pre,
	.on_free_post 		= on_free_post,

	.on_lock_pre 		= on_lock_pre,
	.on_lock_post 		= on_lock_post,
	.on_unlock_pre 		= on_unlock_pre,
	.on_unlock_post 	= on_unlock_post,
	
	.on_signal_pre 		= on_signal_pre,
	.on_signal_post 	= on_signal_post,
	.on_wait_pre 		= on_wait_pre,
	.on_wait_post 		= on_wait_post,

	.on_thread_start	= on_thread_start,
	.on_thread_end		= on_thread_end,

	/* [NB] Add more handlers here if necessary. */
};
/* ====================================================================== */

static void
destroy_page_buffer(void)
{
	unsigned int i;
	
	if (page_buffer == NULL)
		return;
	
	for (i = 0; i < nr_data_pages + 1; ++i) {
		if (page_buffer[i] != 0)
			free_page(page_buffer[i]);
	}
	
	vfree(page_buffer);
	page_buffer = NULL;
}

static int __init
create_page_buffer(void)
{
	unsigned int i;
	size_t sz;
		
	BUG_ON(!is_power_of_2(nr_data_pages));
	sz = sizeof(*page_buffer) * (nr_data_pages + 1);
	
	page_buffer = vmalloc(sz);
	if (page_buffer == NULL)
		return -ENOMEM;
	memset(page_buffer, 0, sz);

	for (i = 0; i < nr_data_pages + 1; ++i) {
		page_buffer[i] = get_zeroed_page(GFP_KERNEL);
		if (page_buffer[i] == 0) {
			destroy_page_buffer();
			return -ENOMEM;
		}
	}
	start_page = (struct kedr_tr_start_page *)page_buffer[0];
	
	/* [NB] 'read_pos' and 'write_pos' are both 0 now. */
	return 0;
}
/* ====================================================================== */

static void
destroy_b0_buffer(void)
{
	vfree(b0_buffer);
}

static int __init
create_b0_buffer(void)
{
	b0_buffer = vmalloc(b0_nr_data_pages << PAGE_SHIFT);
	if (b0_buffer == NULL)
		return -ENOMEM;
	
	/* Just to make sure no older kernel data can leak to userspace via
	 * this buffer. */
	memset(b0_buffer, 0, b0_nr_data_pages << PAGE_SHIFT);
	return 0;
}

static void
destroy_b1_buffer(void)
{
	vfree(b1_buffer);
}

static int __init
create_b1_buffer(void)
{
	unsigned int b1_size; 
	
	/* (-1) for unsigned char compressed[1]. */
	b1_size = (unsigned int)sizeof(struct kedr_tr_event_compressed) - 1
		+ lzo1x_worst_compress(b0_nr_data_pages * PAGE_SIZE);
	
	b1_buffer = vmalloc(b1_size);
	if (b1_buffer == NULL)
		return -ENOMEM;
	
	memset(b1_buffer, 0, b1_size); /* just in case */
	return 0;
}
/* ====================================================================== */

static void 
test_remove_debugfs_files(void)
{
	if (buffer_file != NULL)
		debugfs_remove(buffer_file);
	if (events_lost_file != NULL)
		debugfs_remove(events_lost_file);
}

static int 
test_create_debugfs_files(void)
{
	const char *name = "ERROR";
	
	BUG_ON(debugfs_dir_dentry == NULL);
		
	buffer_file = debugfs_create_file(buffer_file_name, 
		S_IRUSR | S_IRGRP | S_IWUSR | S_IWGRP,
		debugfs_dir_dentry, NULL, &buffer_file_ops);
	if (buffer_file == NULL) {
		name = buffer_file_name;
		goto out;
	}
	
	name = "events_lost";
	events_lost_file = debugfs_create_u64(name, S_IRUGO, 
		debugfs_dir_dentry, &events_lost);
	if (events_lost_file == NULL)
		goto out;
	
	return 0;
out:
	pr_warning(KEDR_MSG_PREFIX 
		"Failed to create a file in debugfs (\"%s\").\n",
		name);
	test_remove_debugfs_files();
	return -ENOMEM;
}

static void __exit
test_cleanup_module(void)
{
	/* Unregister the event handlers first. */
	kedr_unregister_event_handlers(&eh);
	
	test_remove_debugfs_files();
	debugfs_remove(debugfs_dir_dentry);
	
	vfree(lzo_wrkmem);
	destroy_page_buffer();
	return;
}

static int __init
test_init_module(void)
{
	int ret = 0;
	
	if (!is_power_of_2(nr_data_pages)) {
		pr_warning(KEDR_MSG_PREFIX
	"Invalid value of 'nr_data_pages' (%u): must be a power of 2.\n",
			nr_data_pages);
		return -EINVAL;
	}
	
	if (nr_data_pages > KEDR_TR_MAX_DATA_PAGES) {
		pr_warning(KEDR_MSG_PREFIX
	"'nr_data_pages' must not exceed %u.\n", 
			KEDR_TR_MAX_DATA_PAGES);
		return -EINVAL;
	}
	
	if (nr_data_pages < 2 * b0_nr_data_pages) {
		pr_warning(KEDR_MSG_PREFIX
	"'nr_data_pages' must not be less than %u.\n", 
			2 * b0_nr_data_pages);
		return -EINVAL;
	}
	
	if (notify_mark < 1 || notify_mark > nr_data_pages) {
		pr_warning(KEDR_MSG_PREFIX
"'notify_mark' must be a positive value not greater than 'nr_data_pages'.\n");
		return -EINVAL;
	}
	
	buffer_size = nr_data_pages << PAGE_SHIFT;
	
	ret = create_page_buffer();
	if (ret != 0)
		return ret;

	ret = create_b0_buffer();
	if (ret != 0)
		goto out_free_pgbuf;

	ret = create_b1_buffer();
	if (ret != 0)
		goto out_free_b0;
	
	debugfs_dir_dentry = debugfs_create_dir(debugfs_dir_name, NULL);
	if (debugfs_dir_dentry == NULL) {
		pr_warning(KEDR_MSG_PREFIX
			"Failed to create a directory in debugfs\n");
		ret = -EINVAL;
		goto out_free_b1;
	}
	if (IS_ERR(debugfs_dir_dentry)) {
		pr_warning(KEDR_MSG_PREFIX "Debugfs is not supported\n");
		ret = -ENODEV;
		goto out_free_b1;
	}

	ret = test_create_debugfs_files();
	if (ret != 0)
		goto out_rmdir;
	
	/* [NB] Register event handlers only after everything else has 
	 * been initialized. */
	ret = kedr_register_event_handlers(&eh);
	if (ret != 0)
		goto out_rm_files;
	
	/* Allocate space for the LZO1X compressor working memory */
	lzo_wrkmem = vmalloc(LZO1X_1_MEM_COMPRESS);
	if(lzo_wrkmem == NULL) {
		pr_warning(KEDR_MSG_PREFIX
			"Failed to allocate lzo wrkmem (%lu bytes)\n",
			(unsigned long)LZO1X_1_MEM_COMPRESS);
		ret = -ENOMEM;
		goto out_rm_files;
	}
	return 0;

out_rm_files:
	test_remove_debugfs_files();
out_rmdir:
	debugfs_remove(debugfs_dir_dentry);
out_free_b1:
	destroy_b1_buffer();
out_free_b0:
	destroy_b0_buffer();	
out_free_pgbuf:
	destroy_page_buffer();
	return ret;
}

module_init(test_init_module);
module_exit(test_cleanup_module);
/* ====================================================================== */
