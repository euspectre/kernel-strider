/* handlers.c - operations provided by the framework to be used in the 
 * instrumented code: handling of function entry and exit, etc.
 * Some of these operations may be used during the instrumentation as well.
 * The wrapper and holder functions for these operations are also defined 
 * here. */

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

#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/string.h>
#include <linux/hardirq.h>
#include <linux/sched.h>
#include <linux/smp.h>
#include <linux/rcupdate.h>

#include <kedr/kedr_mem/core_api.h>
#include <kedr/kedr_mem/local_storage.h>
#include <kedr/kedr_mem/functions.h>
#include <kedr/object_types.h>

#include "config.h"
#include "core_impl.h"

#include "handlers.h"
#include "tid.h"

/* ====================================================================== */

/* KEDR_SAVE_SCRATCH_REGS_BUT_AX
 * Save scratch registers (except %eax/%rax) and flags on the stack.
 * 
 * KEDR_RESTORE_SCRATCH_REGS_BUT_AX
 * Restore scratch registers (except %eax/%rax) and flags from the stack.
 *
 * KEDR_PREPARE_ARG_FROM_AX
 * On x86-32, does nothing because the first argument should be passed in 
 * %eax (if 'regparm' compiler option is used) and it is already there now. 
 * On x86-64, copies the value from %rax to %rdi where the first argument 
 * of the function should reside on this architecture. 
 * 
 * These macros can be used when injecting a function call into the 
 * code or making a function call from the instrumented code. The function 
 * to be called is a C function, so, according to x86 ABI, it is responsible
 * for preserving the values of the non-scratch registers. %eax/%rax should 
 * be saved and restored separately by the caller of the wrapper code. This 
 * register is used to pass the argument to the function to be called and 
 * to contain its return value. */
#ifdef CONFIG_X86_64
# define KEDR_SAVE_SCRATCH_REGS_BUT_AX \
	"pushfq\n\t"		\
	"pushq %rcx\n\t"	\
	"pushq %rdx\n\t"	\
	"pushq %rsi\n\t"	\
	"pushq %rdi\n\t"	\
	"pushq %r8\n\t"		\
	"pushq %r9\n\t"		\
	"pushq %r10\n\t"	\
	"pushq %r11\n\t"

# define KEDR_RESTORE_SCRATCH_REGS_BUT_AX \
	"popq %r11\n\t"	\
	"popq %r10\n\t"	\
	"popq %r9\n\t"	\
	"popq %r8\n\t"	\
	"popq %rdi\n\t"	\
	"popq %rsi\n\t"	\
	"popq %rdx\n\t"	\
	"popq %rcx\n\t"	\
	"popfq\n\t"

# define KEDR_PREPARE_ARG_FROM_AX \
	"movq %rax, %rdi\n\t"
	
#else /* CONFIG_X86_32 */
# define KEDR_SAVE_SCRATCH_REGS_BUT_AX \
	"pushf\n\t"		\
	"pushl %ecx\n\t"	\
	"pushl %edx\n\t"

# define KEDR_RESTORE_SCRATCH_REGS_BUT_AX \
	"popl %edx\n\t"		\
	"popl %ecx\n\t"		\
	"popf\n\t"

# define KEDR_PREPARE_ARG_FROM_AX ""
#endif /* #ifdef CONFIG_X86_64 */

/* The "holder-wrapper" technique is inspired by the implementation of 
 * KProbes (kretprobe, actually) on x86. 
 *
 * Each function called via a wrapper of this kind takes its only parameter
 * in %eax/%rax. The return value of the function will also be stored in 
 * this register. Other general-purpose registers as well as the flags 
 * will be preserved by the wrappers.
 *
 * These wrappers allow to reduce code bloat. If it were not for them, one
 * would need to insert the code for saving and restoring registers directly
 * into the instrumented function.
 *
 * The holder functions are not intended to be called. Their main purpose is 
 * to contain the wrapper functions written in assembly. The wrappers must 
 * not have usual prologues and epilogues unlike the ordinary functions. 
 * This is because we need to control which registers the wrapper functions
 * use, as well as when and how they do it.
 * 
 * [NB] "__used" makes the compiler think the function is used even if it is
 * not clearly visible where and how. This prevents the compiler from 
 * removing this function. */

/* KEDR_DEFINE_WRAPPER(__func) defines the wrapper for function '__func',
 * along with its holder. */
#define KEDR_DEFINE_WRAPPER(__func) 		\
static __used void __func ## _holder(void) 	\
{						\
	asm volatile (				\
		".global " #__func "_wrapper\n"	\
		#__func "_wrapper: \n\t"	\
		KEDR_SAVE_SCRATCH_REGS_BUT_AX	\
		KEDR_PREPARE_ARG_FROM_AX	\
		"call " #__func " \n\t"		\
		KEDR_RESTORE_SCRATCH_REGS_BUT_AX \
		"ret\n");			\
}
/* ====================================================================== */

/* The operations that can be used in the instrumented code.
 * These functions are static because they should only be called only via 
 * the wrappers. The description of these functions is given in
 * handlers.h in the comments for the respective wrappers. */
static __used unsigned long
kedr_on_function_entry(struct kedr_func_info *fi)
{
	struct kedr_local_storage *ls;
	void (*pre_handler)(struct kedr_local_storage *);
		
	ls = ls_allocator->alloc_ls(ls_allocator);
	if (ls == NULL)
		return 0;
	
	ls->fi = fi;
	ls->tid = kedr_get_thread_id();
	
	if (sampling_rate != 0) {
		long tindex;
		tindex = kedr_get_tindex();
		if (tindex < 0) {
			pr_warning(KEDR_MSG_PREFIX 
"Failed to obtain index of the thread with ID 0x%lx, error code: %d\n",
				ls->tid, (int)tindex);
			ls_allocator->free_ls(ls_allocator, ls);
			return 0;
		}
		ls->tindex = (unsigned long)tindex;
	}
	
	if (eh_current->on_function_entry != NULL)
		eh_current->on_function_entry(eh_current, ls->tid, 
			ls->fi->addr);
	
	/* Call the pre handler if it is set. */
	rcu_read_lock();
	pre_handler = rcu_dereference(ls->fi->pre_handler);
	if (pre_handler != NULL)
		pre_handler(ls);
	rcu_read_unlock();
	
	return (unsigned long)ls;
}
KEDR_DEFINE_WRAPPER(kedr_on_function_entry);
/* ====================================================================== */

static __used void
kedr_on_function_exit(unsigned long storage)
{
	struct kedr_local_storage *ls = 
		(struct kedr_local_storage *)storage;
	void (*post_handler)(struct kedr_local_storage *);
	
	/* Call the post handler if it is set. */
	rcu_read_lock();
	post_handler = rcu_dereference(ls->fi->post_handler);
	if (post_handler != NULL)
		post_handler(ls);
	rcu_read_unlock();
	
	if (eh_current->on_function_exit != NULL)
		eh_current->on_function_exit(eh_current, ls->tid, 
			ls->fi->addr);
	
	ls_allocator->free_ls(ls_allocator, ls);
}
KEDR_DEFINE_WRAPPER(kedr_on_function_exit);
/* ====================================================================== */

/* Default pre-handler for a function call, just reports the respective
 * event. */
static void 
default_pre_handler(struct kedr_local_storage *ls)
{
	struct kedr_call_info *info = (struct kedr_call_info *)(ls->info);
	
	if (eh_current->on_call_pre != NULL)
		eh_current->on_call_pre(eh_current, ls->tid, info->pc,
			info->target);
}

/* Default post-handler for a function call, just reports the respective
 * event. */
static void 
default_post_handler(struct kedr_local_storage *ls)
{
	struct kedr_call_info *info = (struct kedr_call_info *)(ls->info);
	
	if (eh_current->on_call_post != NULL)
		eh_current->on_call_post(eh_current, ls->tid, info->pc,
			info->target);
}

void
kedr_fill_call_info(unsigned long ci)
{
	struct kedr_call_info *info = (struct kedr_call_info *)ci;

	/* Set the defaults first; the function handling subsystem may
	 * change some or all of these below. */
	info->repl = info->target;
	info->pre_handler = default_pre_handler;
	info->post_handler = default_post_handler;

	if (function_handlers->fill_call_info != NULL)
		function_handlers->fill_call_info(function_handlers, info);
}
KEDR_DEFINE_WRAPPER(kedr_fill_call_info);
/* ====================================================================== */

/* Non-zero for the addresses that may belong to the user space, 
 * 0 otherwise. If the address is valid and this function returns non-zero,
 * it is an address in the user space. */
static int
is_user_space_address(unsigned long addr)
{
	return (addr < TASK_SIZE);
}

/* [NB] On x86-32, both thread stack and IRQ stacks are organized in a 
 * similar way. Each stack is contained in a memory area of size 
 * THREAD_SIZE bytes, the start of the area being aligned at THREAD_SIZE 
 * byte boundary. The beginning of the area is occupied by 'thread_info' 
 * structure, the end - by the stack (growing towards the beginning). For 
 * simplicity, we treat the addresses pointing to 'thread_info' and to the 
 * stack the same way here. 'thread_info' structures are managed by the 
 * kernel proper rather than by the modules, so we may consider these 
 * structures read-only from the modules' point of view.
 * 
 * For details, see kernel/irq_32.c and include/asm/processor.h in arch/x86.
 *
 * Thread stack is organized on x86-64 in a similar way like on x86-32. 
 * IRQ stack has different organization, it is IRQ_STACK_SIZE bytes in size.
 * It seems to be placed at the beginning of some section with per-cpu data.
 * It looks like that the kernel data and code are located immediately 
 * before it. It is very unlikely that a target kernel module will access
 * the kernel data no more than IRQ_STACK_SIZE bytes before the IRQ stack
 * concurrently with the access to the IRQ stack itself. So we may check
 * the address as if the IRQ stack was aligned at IRQ_STACK_SIZE byte
 * boundary.
 * 
 * Other stacks (exception stacks, debug stacks, etc.) are not considered
 * here. */
 
/* Align the pointer by the specified value ('align'). 'align' must be a 
 * power of 2). */
#define KEDR_PTR_ALIGN(p, align) \
	((unsigned long)(p) & ~((align) - 1))

/* Non-zero if the address refers to the current thread's stack or an 
 * IRQ stack, 0 otherwise. */
static int
is_stack_address(unsigned long addr)
{
	unsigned long sp;
#ifdef CONFIG_X86_64
	asm volatile(
		"mov %%rsp, %0\n\t"
		: "=g"(sp)
		: /* no inputs */
		: "memory"
	);

	if (in_interrupt()) {
		return (KEDR_PTR_ALIGN(addr, IRQ_STACK_SIZE) == 
			KEDR_PTR_ALIGN(sp, IRQ_STACK_SIZE));
	}

#else /* x86-32 */
	asm volatile(
		"mov %%esp, %0\n\t"
		: "=g"(sp)
		: /* no inputs */
		: "memory"
	); 
#endif
	return (KEDR_PTR_ALIGN(addr, THREAD_SIZE) == 
		KEDR_PTR_ALIGN(sp, THREAD_SIZE));
}
/* ====================================================================== */

/* For each memory access event that could happen in the block, executes 
 * on_memory_event() handler if set. 
 * 'data' is the pointer, the address of which has been passed to 
 * begin_memory_events() callback. */
static void
report_events(struct kedr_local_storage *ls, void *data)
{
	unsigned long i;
	unsigned long nval = 0;
	struct kedr_block_info *info = (struct kedr_block_info *)ls->info;
	u32 mask_bit = 1;
	u32 write_mask = info->write_mask | ls->write_mask;
	
	if (eh_current->on_memory_event == NULL)
		return;
	
	for (i = 0; i < info->max_events; ++i, mask_bit <<= 1) {
		unsigned long n = nval;
		unsigned long size;
		enum kedr_memory_event_type type = KEDR_ET_MREAD;
		unsigned long addr;
		
		if (info->string_mask & mask_bit) {
			size = ls->values[n + 1];
			nval += 2;
		}
		else {
			size = info->events[i].size;
			nval += 1;
		}
		
		if (write_mask & mask_bit) {
			type = ((info->read_mask & mask_bit) != 0) ? 
				KEDR_ET_MUPDATE :
				KEDR_ET_MWRITE;
		}
		
		addr = ls->values[n];
		
		/* Filter out the accesses to the stack and to the user 
		 * space memory if required. That is, call on_memory_event()
		 * with 0 as 'addr' as if the event did not happen. */
		if ((!process_stack_accesses && is_stack_address(addr)) || 
		    (!process_um_accesses && is_user_space_address(addr)))
			addr = 0;
		
		eh_current->on_memory_event(eh_current, ls->tid, 
			info->events[i].pc, addr, size, type, data);
	}
}

/* Returns 0 if the events from the current block should be discarded, 
 * non-zero if they should be reported. 
 * Sampling is taken into account here, sampling counters are updated as 
 * needed. */
static int
should_report_events(struct kedr_local_storage *ls, 
	struct kedr_block_info *info)
{
	s32 num_to_skip;
	u32 counter;
	struct kedr_sampling_counters *sc;
	
	++blocks_total;
	if (sampling_rate == 0)
		return 1; /* Sampling is disabled, report all events. */
	
	sc = &info->scounters[ls->tindex];
	
	/* Find out how many times the events collected for the block should
	 * still be discarded. Racy but OK as some inaccuracy of the 
	 * counters makes no harm here. */
	num_to_skip = --sc->num_to_skip; 
	if (num_to_skip > 0) {
		++blocks_skipped;
		return 0;
	}
	
	/* Update the execution counter, adjust 'num_to_skip' for the next
	 * round. Also racy, but OK. */
	counter = sc->counter;
	num_to_skip = (counter >> (32 - sampling_rate)) + 1;
	sc->num_to_skip = num_to_skip;
	sc->counter = counter + num_to_skip;
	return 1;
}

static __used void
kedr_on_common_block_end(unsigned long storage)
{
	struct kedr_local_storage *ls = 
		(struct kedr_local_storage *)storage;
	struct kedr_block_info *info = (struct kedr_block_info *)ls->info;
	
	void *data = NULL;
	
	if (should_report_events(ls, info)) {
		if (eh_current->begin_memory_events != NULL) {
			eh_current->begin_memory_events(eh_current, ls->tid, 
				info->max_events, &data);
		}
		
		report_events(ls, data);
		
		if (eh_current->end_memory_events != NULL)
			eh_current->end_memory_events(eh_current, ls->tid, 
				data);
	}
	
	/* Prepare the storage for later use. */
	memset(&ls->values[0], 0, 
		KEDR_MAX_LOCAL_VALUES * sizeof(unsigned long));
	ls->write_mask = 0;
	ls->dest_addr = 0;
}
KEDR_DEFINE_WRAPPER(kedr_on_common_block_end);
/* ====================================================================== */

static __used void
kedr_on_locked_op_pre(unsigned long storage)
{
	struct kedr_local_storage *ls = 
		(struct kedr_local_storage *)storage;
	struct kedr_block_info *info = (struct kedr_block_info *)ls->info;
	
	if (eh_current->on_locked_op_pre != NULL) {
		ls->temp = 0;
		eh_current->on_locked_op_pre(eh_current, ls->tid,
			info->events[0].pc, (void **)&ls->temp);
	}
}
KEDR_DEFINE_WRAPPER(kedr_on_locked_op_pre);

static __used void
kedr_on_locked_op_post(unsigned long storage)
{
	struct kedr_local_storage *ls = 
		(struct kedr_local_storage *)storage;
	struct kedr_block_info *info = (struct kedr_block_info *)ls->info;
	
	/* [NB] Here we make use of the fact that a locked update cannot be
	 * a string operation and it is the only operation 'info' contains
	 * the data for. 
	 *
	 * [NB] A locked operation is not necessarily an update. For 
	 * example, it can be a "read" in case of CMPXCHG*. */
	if (eh_current->on_locked_op_post != NULL) {
		u32 write_mask = info->write_mask | ls->write_mask;
		enum kedr_memory_event_type type = KEDR_ET_MREAD;
		if (write_mask & 1) {
			type = ((info->read_mask & 1) != 0) ? 
				KEDR_ET_MUPDATE :
				KEDR_ET_MWRITE;
		}
		
		eh_current->on_locked_op_post(eh_current, ls->tid,
			info->events[0].pc, ls->values[0], 
			info->events[0].size, type, 
			(void *)ls->temp);
	}
	
	/* Prepare the storage for later use. */
	ls->values[0] = 0;
	ls->write_mask = 0;
}
KEDR_DEFINE_WRAPPER(kedr_on_locked_op_post);
/* ====================================================================== */

static __used void
kedr_on_io_mem_op_pre(unsigned long storage)
{
	struct kedr_local_storage *ls = 
		(struct kedr_local_storage *)storage;
	struct kedr_block_info *info = (struct kedr_block_info *)ls->info;
	
	if (eh_current->on_io_mem_op_pre != NULL) {
		ls->temp = 0;
		eh_current->on_io_mem_op_pre(eh_current, ls->tid,
			info->events[0].pc, (void **)&ls->temp);
	}
}
KEDR_DEFINE_WRAPPER(kedr_on_io_mem_op_pre);

static __used void
kedr_on_io_mem_op_post(unsigned long storage)
{
	struct kedr_local_storage *ls = 
		(struct kedr_local_storage *)storage;
	struct kedr_block_info *info = (struct kedr_block_info *)ls->info;
	
	/* [NB] Here we make use of the fact that an instruction in this 
	 * block is INS or OUTS, that is, a string operation of type X or Y
	 * but not XY. It is either read or write but not update. */
	
	if (eh_current->on_io_mem_op_post != NULL) {
		enum kedr_memory_event_type type = KEDR_ET_MREAD;
		if (info->write_mask & 1)
			type = KEDR_ET_MWRITE;
	
		eh_current->on_io_mem_op_post(eh_current, ls->tid, 
			info->events[0].pc, ls->values[0], ls->values[1],
			type, (void *)ls->temp);
	}
	
	/* Prepare the storage for later use. */
	ls->values[0] = 0;
	ls->values[1] = 0;
}
KEDR_DEFINE_WRAPPER(kedr_on_io_mem_op_post);
/* ====================================================================== */

static __used void
kedr_on_barrier_pre(unsigned long storage)
{
	struct kedr_local_storage *ls = 
		(struct kedr_local_storage *)storage;
	enum kedr_barrier_type bt = (enum kedr_barrier_type)(u8)ls->temp;
	unsigned long pc = ls->temp1;
	
	if (eh_current->on_memory_barrier_pre != NULL)
		eh_current->on_memory_barrier_pre(eh_current, ls->tid, pc,
			bt);
}
KEDR_DEFINE_WRAPPER(kedr_on_barrier_pre);

static __used void
kedr_on_barrier_post(unsigned long storage)
{
	struct kedr_local_storage *ls = 
		(struct kedr_local_storage *)storage;
	enum kedr_barrier_type bt = (enum kedr_barrier_type)(u8)ls->temp;
	unsigned long pc = ls->temp1;
	
	if (eh_current->on_memory_barrier_post != NULL)
		eh_current->on_memory_barrier_post(eh_current, ls->tid, pc,
			bt);
}
KEDR_DEFINE_WRAPPER(kedr_on_barrier_post);
/* ====================================================================== */
