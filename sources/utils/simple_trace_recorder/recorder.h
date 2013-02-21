/* recorder.h - common definitions for both the kernel part and the user 
 * part of the simple trace recorder. 
 *
 * Note that "thread create" and "thread join" pre/post events (see 
 * kedr/kedr_mem/core_api.h) are ignored by this output system */

#ifndef RECORDER_H_1045_INCLUDED
#define RECORDER_H_1045_INCLUDED

#include <linux/types.h>
/* ====================================================================== */

/* [NB] Currently, this system might not work if the kernel is 64-bit but
 * the user space part is a 32-bit application. */
/* ====================================================================== */

/* Maximum length of the target module's name to be reported. If the name
 * is longer, only the first KEDR_TARGET_NAME_LEN will be used here. */
#define KEDR_TARGET_NAME_LEN 31

/* Maximum length of a command name (application name) to be reported, not
 * counting the terminating nul character. As the name is commonly obtained
 * from struct task_struct::comm, it makes little sense to make the limit
 * larger than (TASK_COMM_LEN - 1), which is 15 as of kernel 3.7. */
#define KEDR_COMM_LEN 15

/* Meaning of the commonly used fields of the event structures:
 *   tid - thread ID;
 *   pc - program counter (aka PC, instruction pointer, IP) - address of 
 *       a location in the code; 
 *   obj_id - ID of an object (lock, signal/wait object, ...); 
 *   func - start address of the original instance of a function;
 *   addr - start address of a memory area allocated, freed, read from or 
 *       written to;
 *   size - size of a memory area allocated, read from or written to. */

/* Event header. */
struct kedr_tr_event_header
{
	/* Type of the event, see 'enum kedr_tr_event_type'. */
	__u16 type; 
	
	/* Size of the event structure including this header. */
	__u16 event_size; 
	
	/* (meaningful for memory access events only, not used for others)
	 * Number of the events of the given type the event structure 
	 * contains information about. */
	__u16 nr_events; 
	
	/* Type of the object involved in the event (if any). See the 
	 * description of the event structures below for details. */
	__u16 obj_type; 
	
	/* ID of the thread where the event happened. Not used for module
	 * load/unload and session start/end events. */
	__u64 tid;
} __attribute__ ((packed));

/* Types of the events. */
enum kedr_tr_event_type
{
	/* A record for a fake event. Such records can be used to fill the
	 * remaining space of the page in the buffer. 'event_size' does not
	 * matter, if the reader encounters this event, it should always 
	 * skip to the next page of the buffer. 
	 * Note that if the remaining space on the page is not enough for 
	 * even the event header to fit in, the contents of this space will
	 * be unspecified and the reader should also skip to the next page. 
	 */
	KEDR_TR_EVENT_SKIP = 0,
	
	/* "Target module has just loaded", "Target module is about to 
	 * unload" events.
	 * Structure: kedr_tr_event_module. */
	KEDR_TR_EVENT_TARGET_LOAD = 1,
	KEDR_TR_EVENT_TARGET_UNLOAD	= 2,
	
	/* Function entry and exit. 
	 * Structure: kedr_tr_event_func. */
	KEDR_TR_EVENT_FENTRY = 3,
	KEDR_TR_EVENT_FEXIT = 4,
	
	/* Call pre, call post. 
	 * Structure: kedr_tr_event_call. */
	KEDR_TR_EVENT_CALL_PRE = 5,
	KEDR_TR_EVENT_CALL_POST	= 6,
	
	/* A sequence of memory read/write events (no more than 32 events).
	 * Structure: kedr_tr_event_mem. */
	KEDR_TR_EVENT_MEM = 7,
	
	/* A locked memory access event.
	 * Structure: kedr_tr_event_mem. */
	KEDR_TR_EVENT_MEM_LOCKED = 8,
	
	/* An memory access event from an I/O operation.
	 * Structure: kedr_tr_event_mem. */
	KEDR_TR_EVENT_MEM_IO = 9,
	
	/* Memory barrier, pre/post events. 
	 * Structure: kedr_tr_event_barrier. */
	KEDR_TR_EVENT_BARRIER_PRE = 10,
	KEDR_TR_EVENT_BARRIER_POST = 11,
	
	/* Memory allocation and freeing, pre/post events.
	 * Structure: kedr_tr_event_alloc_free. */
	KEDR_TR_EVENT_ALLOC_PRE = 12,
	KEDR_TR_EVENT_ALLOC_POST = 13,
	KEDR_TR_EVENT_FREE_PRE = 14,
	KEDR_TR_EVENT_FREE_POST = 15,
	
	/* Lock and unlock, pre/post events.
	 * Structure: kedr_tr_event_sync. */
	KEDR_TR_EVENT_LOCK_PRE = 16,
	KEDR_TR_EVENT_LOCK_POST = 17,
	KEDR_TR_EVENT_UNLOCK_PRE = 18,
	KEDR_TR_EVENT_UNLOCK_POST = 19,
	
	/* Signal and wait, pre/post events.
	 * Structure: kedr_tr_event_sync. */
	KEDR_TR_EVENT_SIGNAL_PRE = 20,
	KEDR_TR_EVENT_SIGNAL_POST = 21,
	KEDR_TR_EVENT_WAIT_PRE = 22,
	KEDR_TR_EVENT_WAIT_POST = 23,
	
	/* "Block enter" event. Reported before the first memory access in 
	 * a "block" - a multiple entry, multiple exit fragment of the code 
	 * containing no constructs that transfer control outside of the 
	 * function, no barriers, no backward jumps.
	 * Structure: kedr_tr_event_block. */
	KEDR_TR_EVENT_BLOCK_ENTER = 24,

	/* "Session start" and "session end" events. The former is generated
	 * when the first of the target modules has been loaded (before 
	 * "target load" event). The latter is generated when the only
	 * loaded target module is about to unload (after "target unload"
	 * event).
	 * Structure: kedr_tr_event_session. */
	KEDR_TR_EVENT_SESSION_START = 25,
	KEDR_TR_EVENT_SESSION_END = 26,

	/* "Thread start" event. Generated right before the thread enters the
	 * code of the target modules the first time.
	 * Structure: kedr_tr_event_tstart. */
	KEDR_TR_EVENT_THREAD_START = 27,

	/* "Thread end" event. Generated when the core detects the thread has
	 * ended. It is possible that this event is not reported for some
	 * threads, so there can be "thread start" events without matching
	 * "thread end" events.
	 * Structure: kedr_tr_event_tend. */
	KEDR_TR_EVENT_THREAD_END = 28,

	/* The number of event types defined so far. */
	KEDR_TR_EVENT_MAX
};

/* [NB] Only lower 32 bits of 'func' and 'pc' are stored. On x86-64, the 
 * higher 32 bits can be obtained by sign extension of the stored values:
 * full_value = (__u64)(__s64)(__s32)stored_value. */

struct kedr_tr_event_session
{
	struct kedr_tr_event_header header;
} __attribute__ ((packed));

struct kedr_tr_event_module
{
	struct kedr_tr_event_header header;

	/* Name of the module, truncated if longer than KEDR_TARGET_NAME_LEN
	 * characters. C-style string. */
	char name[KEDR_TARGET_NAME_LEN + 1];

	/* Addresses of "init" and "core" areas of the module and size of
	 * the code contained in these areas. 
	 * If an area exists but contains no code, the address will be
	 * non-zero but the size will be 0. If there is no such area at all,
	 * both the address and the size will be 0.
	 * 
	 * Meaningful for "target load" events only.
	 * 
	 * This information provides a way to determine the addresses of the
	 * module's ELF sections (if the algorithm the module loader uses
	 * to arrange the sections does not change significantly in the
	 * future).
	 * See linux/module.h and layout_sections() in kernel/module.c, as
	 * of kernel versions 2.6.32 - 3.6.x.
	 *
	 * [NB] As "init" and "core" areas are in the module mapping space,
	 * it is enough to store the lower 32 bits of them, the rule is the
	 * same as for 'pc' values. */
	__u32 init_addr;
	__u32 core_addr;
	__u32 init_size;
	__u32 core_size;
} __attribute__ ((packed));

struct kedr_tr_event_func
{
	struct kedr_tr_event_header header;
	__u32 func;
} __attribute__ ((packed));

struct kedr_tr_event_call
{
	struct kedr_tr_event_header header;
	__u32 func;
	__u32 pc;
} __attribute__ ((packed));

/* One memory access operation. */
struct kedr_tr_event_mem_op
{
	__u64 addr;
	__u32 size;
	__u32 pc;
} __attribute__ ((packed));

/* For the ordinary (i.e. not locked) memory accesses, this is a sequence of 
 * no more than 32 operations. 
 * For a locked memory access or an I/O operation accessing memory, the 
 * structure contains information about exactly that single operation. 
 * The value of 'header.nr_events' is ignored in this case. */
struct kedr_tr_event_mem
{
	struct kedr_tr_event_header header;
	
	/* For each memory operation in this structure, 1 in the 
	 * corresponding bit of 'read_mask' means that operation was a read
	 * from memory, 0 - read was not performed. Similar for 
	 * 'write_mask'. If the appropriate bits are 1 in both masks, that
	 * operation was an update (read + write). For each operation, at 
	 * least one mask must have the corresponding bit set. */
	__u32 read_mask;
	__u32 write_mask;
	
	/* Memory access operations. The array actually has 
	 * 'header.nr_events' elements. Each element corresponds to an 
	 * operation that actually happened and should be reported.
	 * 
	 * [NB] 'kedr_tr_event_header::event_size' is the full size of this 
	 * structure (i.e. including all items of this array). */
	struct kedr_tr_event_mem_op mem_ops[1];
} __attribute__ ((packed));

/* Memory barrier. 
 * 'header.obj_type' is a type of the barrier, see 'enum kedr_barrier_type'
 * in <kedr/object_types.h>. */
struct kedr_tr_event_barrier
{
	struct kedr_tr_event_header header;
	__u32 pc;
} __attribute__ ((packed));

/* Note:
 * - alloc pre  - 'size' is meaningful, 'addr' is not; 
 * - alloc post - both 'size' and 'addr' are meaningful;
 * - free pre/post - 'addr' is meaningful, 'size' is not. */
struct kedr_tr_event_alloc_free
{
	struct kedr_tr_event_header header;
	__u64 addr;
	__u32 size;
	__u32 pc;
} __attribute__ ((packed));

/* A synchronization event (lock/unlock, signal/wait).
 * 'header.obj_type' is a type of the synchronization object involved:
 * - see 'enum kedr_lock_type' in <kedr/object_types.h> for the locks;
 * - see 'enum kedr_sw_object_type' in <kedr/object_types.h> for the objects
 * used in signal/wait operations. */
struct kedr_tr_event_sync
{
	struct kedr_tr_event_header header;
	__u64 obj_id;
	__u32 pc;
} __attribute__ ((packed));

struct kedr_tr_event_block
{
	struct kedr_tr_event_header header;
	__u32 pc;
} __attribute__ ((packed));

/* "Thread start".
 * 'comm' - the name of the thread or the first part of it if the name is
 * longer than KEDR_COMM_LEN characters. */
struct kedr_tr_event_tstart
{
	struct kedr_tr_event_header header;
	char comm[KEDR_COMM_LEN + 1];
} __attribute__ ((packed));

/* "Thread end" */
struct kedr_tr_event_tend
{
	struct kedr_tr_event_header header;
} __attribute__ ((packed));
/* ====================================================================== */

/* This structure is located at the beginning of the first page of the 
 * buffer and contains service data. The data pages that follow this page 
 * form a circular buffer (similar to the one used in kfifo subsystem). */
struct kedr_tr_start_page
{
	/* Positions in the buffer where the data should be read by the 
	 * user-space app and written by kedr_simple_trace_recorder module,
	 * respectively. Each position is the offset from the start of the
	 * first data page, in bytes. 
	 *
	 * [NB] read_pos == write_pos means the buffer is empty. 
	 * New data can be written to the buffer only within 
	 * [write_pos, read_pos) area (taking position wrapping into 
	 * account). 
	 *
	 * If read_pos == write_pos + 1 mod (size of the data pages), the
	 * buffer is completely full. */
	__u32 read_pos;
	__u32 write_pos;
};
/* ====================================================================== */
#endif /* RECORDER_H_1045_INCLUDED */
