#ifndef TID_H_1743_INCLUDED
#define TID_H_1743_INCLUDED

#include <kedr/kedr_mem/block_info.h>

/* If sampling is enabled, creates the structures necessary to assign 
 * indexes to the threads entering the target modules. Does nothing if 
 * sampling is disabled. 
 * Call this function from the init function of the core, before the core
 * starts watching for the targets to load. */
int
kedr_init_tid_sampling(void);

/* Releases the structures allocated by kedr_init_tid_sampling() and after 
 * that if sampling is enabled, does nothing otherwise. 
 * Cal this function when cleaning up the core, after the latter has stopped
 * watching for the targets. */
void
kedr_cleanup_tid_sampling(void);

/* If sampling is enabled, returns the index into the arrays with the 
 * sampling data for the current thread. The arrays are expected to contain 
 * (KEDR_SAMPLING_NUM_TIDS_IRQ + KEDR_SAMPLING_NUM_TIDS) elements, the first
 * ones being used for IRQ handling "threads", the rest - for the "normal" 
 * threads. 
 *
 * The function returns a negative error code on failure.
 *
 * Once a thread has been assigned an index, the index will remain the same,
 * so the subsequent calls to this function in the thread will return the 
 * same value.
 *
 * More than one thread can have a given 'tindex'. 
 * 
 * For an IRQ "thread", its 'tindex' is tid % KEDR_SAMPLING_NUM_TIDS_IRQ.
 * For a "normal thread", 'tindex' is 
 *   (KEDR_SAMPLING_NUM_TIDS_IRQ + n % KEDR_SAMPLING_NUM_TIDS),
 * 'n' being the number of the thread in the order of the entry to the 
 * target. 
 * 
 * If sampling is disabled, the function always returns 0. 
 *
 * It is allowed to call this function in the process context and in the 
 * atomic context. */
long
kedr_get_tindex(void);

#endif /* TID_H_1743_INCLUDED */
