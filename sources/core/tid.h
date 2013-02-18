#ifndef TID_H_1743_INCLUDED
#define TID_H_1743_INCLUDED

#include <linux/hardirq.h>
#include <kedr/kedr_mem/block_info.h>

/* Non-zero if we are in IRQ (harqirq or softirq) context, 0 otherwise.
 * Equivalent to (in_irq() || in_serving_softirq()).
 * Unlike in_interrupt(), the function returns 0 in process context with BH
 * disabled (with spin_lock_bh(), etc.)
 * NMIs are not taken into account as the kernel modules usually do not
 * meddle with them.
 * 
 * [NB] Some kernels prior to 2.6.37 may not define in_serving_softirq() but
 * everything needed to define it should be available. */
static inline unsigned long
kedr_in_interrupt(void)
{
	return in_irq() || (softirq_count() & SOFTIRQ_OFFSET);
}

/* Initialize the thread handling subsystem.
 * Call this function from the init function of the core, before the core
 * starts watching for the targets to load.
 * 'msec' - the timeout interval (in milliseconds) of the timer used to
 * periodically check if some threads have ended.*/
int
kedr_thread_handling_init(unsigned int gc_msec);

/* Cleanup of the thread handling subsystem. Call this from the cleanup
 * function of the core after it has stopped watching for targets. */
void
kedr_thread_handling_cleanup(void);

/* Starts thread handling. Call this before a session starts. */
void
kedr_thread_handling_start(void);

/* Starts thread handling. Call this before a session starts. */
void
kedr_thread_handling_stop(void);

/* Check if the thread the function is called from executed the code of the
 * targets before during the current session. If it did not ("new thread"),
 * the function would generate "thread start" event.
 *
 * The function may also generate "thread end" events if it finds a thread
 * that has already finished but not yet reported as such.
 *
 * Call this function from the handlers of function entries.
 * The function may fail, for example, if there is not enough memory. */
int
kedr_thread_handle_changes(void);

/* If sampling is enabled, returns the index into the arrays with the 
 * sampling data for the current thread. The arrays are expected to contain 
 * (KEDR_SAMPLING_NUM_TIDS_IRQ + KEDR_SAMPLING_NUM_TIDS) elements, the first
 * ones being used for IRQ handling "threads", the rest - for the "normal" 
 * threads. 
 *
 * Once a thread has been assigned an index, the index will remain the same,
 * so the subsequent calls to this function in the thread will return the 
 * same value.
 *
 * More than one thread can have a given 'tindex'. 
 * 
 * For an IRQ pseudo-thread, its index is tid % KEDR_SAMPLING_NUM_TIDS_IRQ.
 * For a "normal thread", 'tindex' is 
 *   (KEDR_SAMPLING_NUM_TIDS_IRQ + n),
 * 'n' being a non-negative integer less than KEDR_SAMPLING_NUM_TIDS. 
 * 
 * If sampling is disabled, the function always returns 0. 
 *
 * It is allowed to call this function in the process context and in the 
 * atomic context. */
unsigned long
kedr_get_tindex(void);

#endif /* TID_H_1743_INCLUDED */
