#ifndef MODULE_MS_ALLOC_H_1209_INCLUDED
#define MODULE_MS_ALLOC_H_1209_INCLUDED

/* module_ms_alloc.h - API for allocation and deallocation of memory
 * in the module mapping space. 
 *
 * Such memory buffers can be used to accomodate the instrumented code
 * and the special data it uses. 
 * It can be crucial that such code lies within the range of a near jump
 * (+/-2Gb) from the original code. Similar requirements arise for the
 * global data accesses using RIP-relative addressing. On x86-32, this is
 * not significant but on x86-64, it is. Allocating memory in the 
 * module mapping space allows to meet these requirements. */

/* ====================================================================== */
/* Initialize the subsystem. 
 * The function returns 0 on success, error code otherwise. */
int
kedr_init_module_ms_alloc(void);

/* Finalize the subsystem. */
void
kedr_cleanup_module_ms_alloc(void);

/* Allocate a buffer of the given size (in bytes). 
 * The function returns NULL in case of failure. 
 * 
 * The allocated memory will be within no more than 2Gb from the code of 
 * the kernel modules and the kernel proper. This simplifies handling of 
 * RIP-relative addressing on x86-64 and handling of the common near 
 * jumps and calls as well).
 * [NB] The allocated memory is not guaranteed to be zeroed. */
void *
kedr_module_alloc(unsigned long size);

/* Free the buffer previously allocated with kedr_module_alloc(). 
 * No-op if 'buf' is NULL.*/
void 
kedr_module_free(void *buf);

#endif // MODULE_MS_ALLOC_H_1209_INCLUDED
