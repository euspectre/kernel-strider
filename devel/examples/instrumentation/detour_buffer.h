#ifndef DETOUR_BUFFER_H_1640_INCLUDED
#define DETOUR_BUFFER_H_1640_INCLUDED

/* detour_buffer.h - operations with detour buffers (the buffers where the
 * code of kernel modules is instrumented and then executed).
 *
 * API for allocation and deallocation of such buffers is provided here. */

/* ====================================================================== */
/* Initialize the detour buffer subsystem. 
 * The function returns 0 on success, error code otherwise. */
int
kedr_init_detour_subsystem(void);

/* Finalize the detour buffer subsystem. */
void
kedr_cleanup_detour_subsystem(void);

/* Allocate detour buffer of the given size (in bytes). 
 * The function returns NULL in case of failure. 
 * 
 * The allocated memory will be within no more than 2Gb from the code of 
 * the kernel modules. This simplifies handling of RIP-relative addressing
 * on x86-64 (and handling of the common near jumps and calls as well).
 *
 * [NB] The allocated memory is not guaranteed to be zeroed. */
void *
kedr_alloc_detour_buffer(unsigned long size);

/* Free the detour buffer. No-op if 'buf' is NULL.*/
void 
kedr_free_detour_buffer(void *buf);

#endif // DETOUR_BUFFER_H_1640_INCLUDED
