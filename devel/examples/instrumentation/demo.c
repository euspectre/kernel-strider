/* demo.c - demonstration of the instrumentation system. */

#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/spinlock.h>

#include "demo.h"
#include "debug_util.h"

struct kedr_demo_record
{
	unsigned long tid; /* thread ID */
	
	enum {
		KEDR_ET_FENTRY,		/* function entry */
		KEDR_ET_FEXIT,		/* function exit */
		KEDR_ET_MREAD,		/* read from memory */
		KEDR_ET_MWRITE,		/* write to memory */
		KEDR_ET_MUPDATE		/* locked update of memory */
	} event_type;
	
	unsigned long func;	/* address of the original function */
	unsigned long pc;	/* address of the original instruction */
	unsigned long addr;	/* address of the accessed memory area */
	unsigned long size;	/* size of the accessed memory area */
};

/* Number of event records to store. After this number of records is stored, 
 * the subsequent records are discarded. */
#define KEDR_DEMO_NUM_RECORDS 512

/* The array of event records and the current number of elements in it. 
 * Both should be protected by 'rec_lock'. */
static struct kedr_demo_record records[KEDR_DEMO_NUM_RECORDS];
static unsigned int rindex = 0;
static DEFINE_SPINLOCK(rec_lock);

/* The counters. */
static u64 num_reads = 0;
static u64 num_writes = 0;
static u64 num_locked_updates = 0;
/* ====================================================================== */

/* [NB] This function should be called from on_module_load() handler. So
 * it is guaranteed that no event can happen in the target module when this
 * function operates (the target is not running yet). Therefore, locking is
 * not necessary to set 'rindex'. */
int
kedr_demo_init(struct module *mod)
{
	rindex = 0;
	num_reads = 0;
	num_writes = 0;
	num_locked_updates = 0;
        return 0;
}

static void
report_memory_event(const char *event_name, unsigned long tid, 
	unsigned long pc, unsigned long addr, unsigned long size)
{
	const char *fmt = 
		"TID=0x%lx %s at 0x%lx (%pS): addr=0x%lx, size=%lu\n";
	int len;
	char one_char[1]; /* for the 1st call to snprintf */
	char *buf = NULL;
	
	len = snprintf(&one_char[0], 1, fmt, tid, event_name, pc, 
		(void *)pc, addr, size);
	buf = (char *)kzalloc(len + 1, GFP_KERNEL);
	if (buf == NULL) {
		pr_err("[sample] report_memory_event: "
		"not enough memory to prepare a message of size %d\n",
			len);
		return;
	}
	snprintf(buf, len + 1, fmt, tid, event_name, pc, (void *)pc, addr, 
		size);
	debug_util_print_string(buf);
	kfree(buf);
}

static void
report_function_event(const char *event_name, unsigned long tid, 
	unsigned long func)
{
	const char *fmt = "TID=0x%lx %s: addr=0x%lx (\"%pf\")\n";
	int len;
	char one_char[1]; /* for the 1st call to snprintf */
	char *buf = NULL;
	
	len = snprintf(&one_char[0], 1, fmt, tid, event_name, func,
		(void *)func);
	buf = (char *)kzalloc(len + 1, GFP_KERNEL);
	if (buf == NULL) {
		pr_err("[sample] report_function_event: "
		"not enough memory to prepare a message of size %d\n",
			len);
		return;
	}
	snprintf(buf, len + 1, fmt, tid, event_name, func, (void *)func);
	debug_util_print_string(buf);
	kfree(buf);
}

/* [NB] This function should be called from on_module_unload() handler. So
 * it is guaranteed that no event can happen in the target module when this
 * function operates (the target is not running already). Therefore, locking
 * is not necessary when accessing 'rindex', 'records' and the counters. */
void
kedr_demo_fini(struct module *mod)
{
	unsigned int i;
	
	debug_util_print_u64(num_reads, "[Totals] reads: %llu; ");
	debug_util_print_u64(num_writes, "writes: %llu; ");
	debug_util_print_u64(num_locked_updates, "locked updates: %llu.\n");
	
	for (i = 0; i < rindex; ++i) {
		switch (records[i].event_type) {
		case KEDR_ET_FENTRY:
			report_function_event("entry", 
				records[i].tid, records[i].func);
			break;
		case KEDR_ET_FEXIT:
			report_function_event("exit", 
				records[i].tid, records[i].func);
			break;
		case KEDR_ET_MREAD:
			report_memory_event("read", records[i].tid, 
				records[i].pc, records[i].addr, 
				records[i].size);
			break;
		case KEDR_ET_MWRITE:
			report_memory_event("write", records[i].tid, 
				records[i].pc, records[i].addr, 
				records[i].size);
			break;
		case KEDR_ET_MUPDATE:
			report_memory_event("locked update", records[i].tid, 
				records[i].pc, records[i].addr, 
				records[i].size);
			break;
		default: break;
		}
	}
}
/* ====================================================================== */

void 
kedr_demo_on_function_entry(unsigned long tid, unsigned long func)
{
	unsigned long irq_flags;
	spin_lock_irqsave(&rec_lock, irq_flags);
	if (rindex < KEDR_DEMO_NUM_RECORDS) {
		records[rindex].event_type = KEDR_ET_FENTRY;
		records[rindex].tid = tid;
		records[rindex].func = func;
		++rindex;
	}
	spin_unlock_irqrestore(&rec_lock, irq_flags);
}

void 
kedr_demo_on_function_exit(unsigned long tid, unsigned long func)
{
	unsigned long irq_flags;
	spin_lock_irqsave(&rec_lock, irq_flags);
	if (rindex < KEDR_DEMO_NUM_RECORDS) {
		records[rindex].event_type = KEDR_ET_FEXIT;
		records[rindex].tid = tid;
		records[rindex].func = func;
		++rindex;
	}
	spin_unlock_irqrestore(&rec_lock, irq_flags);
}

void 
kedr_demo_on_mem_read(unsigned long tid, unsigned long pc, 
	unsigned long addr, unsigned long size)
{
	unsigned long irq_flags;
	spin_lock_irqsave(&rec_lock, irq_flags);
	if (rindex < KEDR_DEMO_NUM_RECORDS) {
		records[rindex].event_type = KEDR_ET_MREAD;
		records[rindex].tid = tid;
		records[rindex].pc = pc;
		records[rindex].addr = addr;
		records[rindex].size = size;
		++rindex;
	}
	++num_reads;
	spin_unlock_irqrestore(&rec_lock, irq_flags);
}

void 
kedr_demo_on_mem_write(unsigned long tid, unsigned long pc, 
	unsigned long addr, unsigned long size)
{
	unsigned long irq_flags;
	spin_lock_irqsave(&rec_lock, irq_flags);
	if (rindex < KEDR_DEMO_NUM_RECORDS) {
		records[rindex].event_type = KEDR_ET_MWRITE;
		records[rindex].tid = tid;
		records[rindex].pc = pc;
		records[rindex].addr = addr;
		records[rindex].size = size;
		++rindex;
	}
	++num_writes;
	spin_unlock_irqrestore(&rec_lock, irq_flags);
}

void 
kedr_demo_on_mem_locked_update(unsigned long tid, unsigned long pc, 
	unsigned long addr, unsigned long size)
{
	unsigned long irq_flags;
	spin_lock_irqsave(&rec_lock, irq_flags);
	if (rindex < KEDR_DEMO_NUM_RECORDS) {
		records[rindex].event_type = KEDR_ET_MUPDATE;
		records[rindex].tid = tid;
		records[rindex].pc = pc;
		records[rindex].addr = addr;
		records[rindex].size = size;
		++rindex;
	}
	++num_locked_updates;
	spin_unlock_irqrestore(&rec_lock, irq_flags);
}
/* ====================================================================== */
