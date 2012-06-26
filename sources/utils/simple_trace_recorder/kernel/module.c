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
 * The module is not required to notify the user-space part about each new 
 * event stored in the buffer. Instead, this is done for each 'notify_mark'
 * pages written and also when "target unload" event is received. */

/* ========================================================================
 * Copyright (C) 2012, KEDR development team
 * Authors: 
 *      Eugene A. Shatokhin <spectre@ispras.ru>
 *      Andrey V. Tsyvarev  <tsyvarev@ispras.ru>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published
 * by the Free Software Foundation.
 ======================================================================== */
 
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

#include <kedr/kedr_mem/core_api.h>
#include <kedr/object_types.h>

#include "../recorder.h"
/* ====================================================================== */

#define KEDR_MSG_PREFIX "[kedr_simple_trace_recorder] "
/* ====================================================================== */

MODULE_AUTHOR("Eugene A. Shatokhin");
MODULE_LICENSE("GPL");
/* ====================================================================== */

/* If you need really large data buffers (> 256Mb), you can try increasing
 * this limit. */
#define KEDR_TR_MAX_DATA_PAGES 65536

/* Number of data pages in the buffer. Must be a power of 2. Must be less 
 * than or equal to KEDR_TR_MAX_DATA_PAGES. */
unsigned int nr_data_pages = 128;
module_param(nr_data_pages, uint, S_IRUGO);

/* For each 'notify_mark' data pages filled in the buffer, this module wakes
 * up the process waiting (in poll()) for the data to be come available for
 * reading. */
unsigned int notify_mark = 1;
module_param(notify_mark, uint, S_IRUGO);

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
static const char *debugfs_dir_name = "kedr_simple_trace_recorder";

static struct dentry *buffer_file = NULL;
static const char *buffer_file_name = "buffer";

/* A mutex to serialize operations with the buffer file. */
static DEFINE_MUTEX(buffer_file_mutex);

/* The buffer itself. */
static unsigned long *page_buffer = NULL;

/* The first page of the buffer, contains service data. */
static struct kedr_tr_start_page *start_page = NULL;

/* The total size of the data pages in the buffer. */
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
/* ====================================================================== */

/* A wait queue for the reader to wait on until enough data become 
 * available. */
static DECLARE_WAIT_QUEUE_HEAD(reader_queue);
/* ====================================================================== */

/* A spinlock to serialize the accesses to the buffer from the event 
 * handlers. */
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
		h->event_size = 0; /* just in case */
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
 * the function will return (__u32)(-1), which means that the event is lost. 
 *
 * Must be called with 'eh_lock' locked. */
static __u32
record_write_common(__u32 wp, __u32 rp, unsigned int size)
{
	__u32 no_space = (__u32)(-1);

	if (!buffer_has_space(wp, rp, size)) {
		++events_lost;
		return no_space;
	}
	
	if (!fits_to_page(wp, size)) {
		wp = complete_buffer_page(wp);
		if (!buffer_has_space(wp, rp, size)) {
			++events_lost;
			set_write_pos_and_notify(wp, rp);
			return no_space;
		}
	}
	return wp;
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
	vma->vm_flags |= VM_RESERVED;
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
 * file oeprations are provided here just in case. 
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
handle_load_unload_impl(enum kedr_tr_event_type et, struct module *mod)
{
	__u32 wp;
	__u32 rp;
	unsigned long irq_flags;
	struct kedr_tr_event_module *ev;
	unsigned int size = (unsigned int)sizeof(*ev);
	
	spin_lock_irqsave(&eh_lock, irq_flags);
	rp = get_read_pos();
	wp = record_write_common(start_page->write_pos, rp, size);
	if (wp == (__u32)(-1))
		goto out;
	
	ev = buffer_pos_to_addr(wp);
	ev->header.type = et;
	ev->header.event_size = size;
	ev->header.nr_events = 0;
	ev->header.obj_type = 0;
	ev->header.tid = 0;
	ev->mod = (__u64)(unsigned long)mod;
	
	wp += size;
	set_write_pos_and_notify(wp, rp);
	
	if (et == KEDR_TR_EVENT_TARGET_UNLOAD) {
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
handle_function_event_impl(enum kedr_tr_event_type et, unsigned long tid, 
	unsigned long func)
{
	__u32 wp;
	__u32 rp;
	unsigned long irq_flags;
	struct kedr_tr_event_func *ev;
	unsigned int size = (unsigned int)sizeof(*ev);

	if (no_call_events)
		return;
	
	spin_lock_irqsave(&eh_lock, irq_flags);
	rp = get_read_pos();
	wp = record_write_common(start_page->write_pos, rp, size);
	if (wp == (__u32)(-1))
		goto out;

	ev = buffer_pos_to_addr(wp);
	ev->header.type = et;
	ev->header.event_size = size;
	ev->header.nr_events = 0;
	ev->header.obj_type = 0;
	ev->header.tid = (__u64)tid;
	ev->func = (__u32)func;

	wp += size;
	set_write_pos_and_notify(wp, rp);
out:
	spin_unlock_irqrestore(&eh_lock, irq_flags);
}

static void 
handle_call_impl(enum kedr_tr_event_type et, unsigned long tid, 
	unsigned long pc, unsigned long func)
{
	__u32 wp;
	__u32 rp;
	unsigned long irq_flags;
	struct kedr_tr_event_call *ev;
	unsigned int size = (unsigned int)sizeof(*ev);	
	
	if (no_call_events)
		return;
	
	spin_lock_irqsave(&eh_lock, irq_flags);
	rp = get_read_pos();
	wp = record_write_common(start_page->write_pos, rp, size);
	if (wp == (__u32)(-1))
		goto out;

	ev = buffer_pos_to_addr(wp);
	ev->header.type = et;
	ev->header.event_size = size;
	ev->header.nr_events = 0;
	ev->header.obj_type = 0;
	ev->header.tid = (__u64)tid;
	ev->func = (__u32)func;
	ev->pc = (__u32)pc;

	wp += size;
	set_write_pos_and_notify(wp, rp);
out:
	spin_unlock_irqrestore(&eh_lock, irq_flags);
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
	ev->header.tid = (__u64)tid;
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
	
	nr = ev->header.nr_events;
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
	
	++ev->header.nr_events;
}

static void
report_block_enter_event(__u64 tid, __u32 pc)
{
	__u32 wp;
	__u32 rp;
	unsigned long irq_flags;
	struct kedr_tr_event_block *ev;
	unsigned int size = (unsigned int)sizeof(*ev);	
	
	spin_lock_irqsave(&eh_lock, irq_flags);
	rp = get_read_pos();
	wp = record_write_common(start_page->write_pos, rp, size);
	if (wp == (__u32)(-1))
		goto out;

	ev = buffer_pos_to_addr(wp);
	ev->header.type = KEDR_TR_EVENT_BLOCK_ENTER;
	ev->header.event_size = size;
	ev->header.nr_events = 0;
	ev->header.obj_type = 0;
	ev->header.tid = tid;
	ev->pc = pc;

	wp += size;
	set_write_pos_and_notify(wp, rp);
out:
	spin_unlock_irqrestore(&eh_lock, irq_flags);
}

static void
end_memory_events(struct kedr_event_handlers *eh, unsigned long tid, 
	void *data)
{
	__u32 wp;
	__u32 rp;
	unsigned long irq_flags;
	unsigned int size;
	struct kedr_tr_event_mem *ev = (struct kedr_tr_event_mem *)data;
	void *where = NULL;
	
	if (ev == NULL || ev->header.nr_events == 0) {
		kfree(ev);
		return;
	}
	
	report_block_enter_event(ev->header.tid, ev->mem_ops[0].pc);
	
	size = sizeof(struct kedr_tr_event_mem) + 
		(ev->header.nr_events - 1) * 
		sizeof(struct kedr_tr_event_mem_op);
	ev->header.event_size = size;
	
	spin_lock_irqsave(&eh_lock, irq_flags);
	rp = get_read_pos();
	wp = record_write_common(start_page->write_pos, rp, size);
	if (wp == (__u32)(-1))
		goto out;
	
	where = buffer_pos_to_addr(wp);
	memcpy(where, ev, size);
	
	wp += size;
	set_write_pos_and_notify(wp, rp);
out:
	spin_unlock_irqrestore(&eh_lock, irq_flags);
	kfree(ev);
}

static void
handle_locked_and_io_impl(enum kedr_tr_event_type et, unsigned long tid, 
	unsigned long pc, unsigned long addr, unsigned long sz, 
	enum kedr_memory_event_type type)
{
	__u32 wp;
	__u32 rp;
	unsigned long irq_flags;
	struct kedr_tr_event_mem *ev;
	unsigned int size = (unsigned int)sizeof(*ev);	
	
	spin_lock_irqsave(&eh_lock, irq_flags);
	rp = get_read_pos();
	wp = record_write_common(start_page->write_pos, rp, size);
	if (wp == (__u32)(-1))
		goto out;

	ev = buffer_pos_to_addr(wp);
	ev->header.type = et;
	ev->header.event_size = size;
	ev->header.nr_events = 1;
	ev->header.obj_type = 0;
	ev->header.tid = (__u64)tid;
	
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

	wp += size;
	set_write_pos_and_notify(wp, rp);
out:
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
	__u32 wp;
	__u32 rp;
	unsigned long irq_flags;
	struct kedr_tr_event_barrier *ev;
	unsigned int size = (unsigned int)sizeof(*ev);	
	
	spin_lock_irqsave(&eh_lock, irq_flags);
	rp = get_read_pos();
	wp = record_write_common(start_page->write_pos, rp, size);
	if (wp == (__u32)(-1))
		goto out;

	ev = buffer_pos_to_addr(wp);
	ev->header.type = et;
	ev->header.event_size = size;
	ev->header.nr_events = 0;
	ev->header.obj_type = (__u16)type;
	ev->header.tid = (__u64)tid;
	ev->pc = pc;

	wp += size;
	set_write_pos_and_notify(wp, rp);
out:
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
	__u32 wp;
	__u32 rp;
	unsigned long irq_flags;
	struct kedr_tr_event_alloc_free *ev;
	unsigned int size = (unsigned int)sizeof(*ev);	
	
	spin_lock_irqsave(&eh_lock, irq_flags);
	rp = get_read_pos();
	wp = record_write_common(start_page->write_pos, rp, size);
	if (wp == (__u32)(-1))
		goto out;

	ev = buffer_pos_to_addr(wp);
	ev->header.type = et;
	ev->header.event_size = size;
	ev->header.nr_events = 0;
	ev->header.obj_type = 0;
	ev->header.tid = (__u64)tid;
	ev->pc = pc;
	ev->size = sz;
	ev->addr = (__u64)addr;

	wp += size;
	set_write_pos_and_notify(wp, rp);
out:
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
	__u32 wp;
	__u32 rp;
	unsigned long irq_flags;
	struct kedr_tr_event_sync *ev;
	unsigned int size = (unsigned int)sizeof(*ev);	
	
	spin_lock_irqsave(&eh_lock, irq_flags);
	rp = get_read_pos();
	wp = record_write_common(start_page->write_pos, rp, size);
	if (wp == (__u32)(-1))
		goto out;

	ev = buffer_pos_to_addr(wp);
	ev->header.type = et;
	ev->header.event_size = size;
	ev->header.nr_events = 0;
	ev->header.obj_type = (__u16)obj_type;
	ev->header.tid = (__u64)tid;
	ev->obj_id = (__u64)obj_id;
	ev->pc = pc;

	wp += size;
	set_write_pos_and_notify(wp, rp);
out:
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

struct kedr_event_handlers eh = {
	.owner 			= THIS_MODULE,
	
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
	kedr_unregister_event_handlers(&eh);
	
	test_remove_debugfs_files();
	debugfs_remove(debugfs_dir_dentry);
	
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
	
	if (notify_mark < 1 || notify_mark > nr_data_pages) {
		pr_warning(KEDR_MSG_PREFIX
"'notify_mark' must be a positive value not greater than 'nr_data_pages'.\n");
		return -EINVAL;
	}
	
	buffer_size = nr_data_pages << PAGE_SHIFT;
	
	ret = create_page_buffer();
	if (ret != 0)
		return ret;
	
	debugfs_dir_dentry = debugfs_create_dir(debugfs_dir_name, NULL);
	if (debugfs_dir_dentry == NULL) {
		pr_warning(KEDR_MSG_PREFIX
			"Failed to create a directory in debugfs\n");
		ret = -EINVAL;
		goto out;
	}
	if (IS_ERR(debugfs_dir_dentry)) {
		pr_warning(KEDR_MSG_PREFIX "Debugfs is not supported\n");
		ret = -ENODEV;
		goto out;
	}

	ret = test_create_debugfs_files();
	if (ret != 0)
		goto out_rmdir;
	
	/* [NB] Register event handlers only after everything else has 
	 * been initialized. */
	ret = kedr_register_event_handlers(&eh);
	if (ret != 0)
		goto out_rm_files;
	
	return 0;

out_rm_files:
	test_remove_debugfs_files();
out_rmdir:
	debugfs_remove(debugfs_dir_dentry);
out:
	destroy_page_buffer();
	return ret;
}

module_init(test_init_module);
module_exit(test_cleanup_module);
/* ====================================================================== */