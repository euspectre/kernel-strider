/* 
 * Encapsulation of several interceptors from the sence of KEDR COI
 * in one for simplify using.
 */

#ifndef VMA_OPERATIONS_INTERCEPTOR_H
#define VMA_OPERATIONS_INTERCEPTOR_H

/* 
 * May be used for types of handlers and for check handlers-related
 * macros.
 */
#include "vma_operations_interceptor_internal.h"

/* 
 * Initialize interceptor for vma operations and connect it
 * to the interceptor for file operations.
 * 
 * 'file_interceptor' should be interceptor for file operations.
 */
int vma_operations_interceptor_register(struct kedr_coi_interceptor* file_interceptor);
/* 
 * Disconnect interceptor for vma operations from interceptor for file
 * operations and destroy former.
 * 
 * 'file_interceptor' should be same as one in register() function.
 */
int vma_operations_interceptor_unregister(struct kedr_coi_interceptor* file_interceptor);

/* 
 * Same as register() and unregister(), but for generated interceptor
 * for file operations.
 */
int vma_operations_interceptor_connect(
	int (*file_payload_register)(struct kedr_coi_payload* payload));
int vma_operations_interceptor_disconnect(
	int (*file_payload_unregister)(struct kedr_coi_payload* payload));

int vma_operations_interceptor_payload_register(struct kedr_coi_payload* payload);
int vma_operations_interceptor_payload_unregister(struct kedr_coi_payload* payload);

/* 
 * Because this is not a factory interceptor, it should be
 * started ans stopped explicitly.
 */
int vma_operations_interceptor_start(void);
int vma_operations_interceptor_stop(void);

#endif