/* common.h - API provided by the function handling plugin that processes 
 * operations common to many kinds of kernel modules. */

#ifndef FH_DRD_COMMON_H_1226_INCLUDED
#define FH_DRD_COMMON_H_1226_INCLUDED

#include <linux/list.h>

struct kedr_local_storage;

/* kedr_*_(start|end) functions declared below should be called at the start
 * and at the end of the code fragments of appropriate type: BH functions
 * (timer/tasklet/softirq functions, etc.), IRQ handlers, fragments with
 * BH disabled, fragments with IRQ disabled, respectively.
 *
 * These functions will generate appropriate events to pass the information
 * about the happens-before relations involved here to the trace.
 *
 * Common arguments:
 * - tid - thread ID;
 * - func - address of the BH function or the IRQ handler;
 * - pc - address of the current instruction. */

void
kedr_bh_start(unsigned long tid, unsigned long func);

void
kedr_bh_end(unsigned long tid, unsigned long func);

void
kedr_bh_disabled_start(unsigned long tid, unsigned long pc);

void
kedr_bh_disabled_end(unsigned long tid, unsigned long pc);

/* [NB] kedr_irq_*() functions call kedr_bh_disabled_*() among other things.
 * The caller does not need to additionally call kedr_bh_disabled_*() here.
 */

void
kedr_irq_start(unsigned long tid, unsigned long func);

void
kedr_irq_end(unsigned long tid, unsigned long func);

void
kedr_irq_disabled_start(unsigned long tid, unsigned long pc);

void
kedr_irq_disabled_end(unsigned long tid, unsigned long pc);

/* The structure used to specify the callback handlers in the situations
 * where several handlers should be allowed for a given function at the
 * same time. 
 * Unlike the handlers in kedr_func_info, these handlers get 'data' as 
 * their second parameter. 
 * If a handler is NULL, it means it is not set. */
struct kedr_fh_drd_handlers
{
	struct list_head list;
	void (*pre)(struct kedr_local_storage *, void *);
	void (*post)(struct kedr_local_storage *, void *);
	void *data;
};

/* ====================================================================== */
#endif /* FH_DRD_COMMON_H_1226_INCLUDED */
