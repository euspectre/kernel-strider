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

/* kedr_fh_drd_common keeps track of the ordinary locks and write locks
 * taken by the target modules.
 * It is needed to properly handle the callbacks that are known to be 
 * executed under a particular lock (e.g. rtnl_lock()) but the lock could be
 * taken by the target driver or by the kernel proper.
 * 
 * Such callbacks may also execute other callbacks that have the same 
 * locking rules. It may not happen often but it is possible.
 * 
 * To avoid generating nested lock/unlock events for the same lock, the 
 * functions below can be used.
 *
 * Both functions take the PC of the lock or unlock operation, respectively,
 * and the corresponding lock ID. Note that the functions DO NOT generate
 * "lock"/"unlock" events themselves, it is the responsibility of the 
 * caller.
 * 
 * Both functions operate atomically and can therefore be called 
 * concurrently.
 * 
 * Can be called from any context.
 * 
 * kedr_fh_mark_locked() notifies kedr_fh_drd_common plugin that the lock 
 * has been locked or a function has started that is known to execute under
 * this lock. If the lock is already marked as locked, the function does
 * not change it and returns 0. Otherwise, it marks it as locked and 
 * returns 1. A negative value is returned in case of an error.
 * 
 * kedr_fh_mark_unlocked() marks the lock as unlocked.
 *
 * [NB] Do not use these functions for read locks because it is OK for
 * several threads in the driver to keep the same read lock locked at the
 * same time. */
int
kedr_fh_mark_locked(unsigned long pc, unsigned long lock_id);

void
kedr_fh_mark_unlocked(unsigned long pc, unsigned long lock_id);
/* ====================================================================== */
#endif /* FH_DRD_COMMON_H_1226_INCLUDED */
