/* 
 * Mechanizm for intercept single callback function.
 * 
 * In the future, it will be moved into KEDR-COI.
 */

/* 
 * Struct represented interceptor for one 'type' of callback function.
 * 
 * Type include not only signature of callback function, but also
 * object type and callback semantic.
 */
struct callback_interceptor;

/* Create interceptor for callback function. */
struct callback_interceptor* callback_interceptor_create(void);
/* 
 * Destroy interceptor for callback function.
 * 
 * If not NULL, 'trace_unforgotten_object' will be called for every
 * object which is currently mapped.
 */
void callback_interceptor_destroy(
    struct callback_interceptor* interceptor,
    void (*trace_unforgotten_object)(const void* object));

/* 
 * Save given callback for given object.
 * 
 * Return 0 on success and negative error on fail.
 * 
 * If other callback has been mapped for that object, return -EEXIST.
 * 
 * NOTE: But attempt to map same callback for object, which already set,
 * will succeed.
 */
int callback_interceptor_map(struct callback_interceptor* interceptor,
    const void* object, void* callback);

/* 
 * Forget callback mapping for given object.
 * 
 * Return 0 if mapping has been erased for object, 1 if object hasn't
 * been mapped.
 */
int callback_interceptor_forget(struct callback_interceptor* interceptor,
    const void* object);

/*
 * Extract callback which has been set for that object.
 * 
 * On success, return 0 and set 'callback' to point to that callback.
 * Otherwise return negative error code
 * (which normally is an unrecoverable error).
 */
int callback_interceptor_get_callback(struct callback_interceptor* interceptor,
    const void* object, void** callback);