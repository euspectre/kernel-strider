#ifndef DEBUG_UTIL_H_1104_INCLUDED
#define DEBUG_UTIL_H_1104_INCLUDED

/* debug_util.h - utility functions for output of debug data, etc. 
 * 
 * The functions listed below that return void print an error message to 
 * the system log if an error occurs. */

struct dentry;

/* Initializes debugging facilities. 
 * The function may create files in debugfs in the directory specified by 
 * 'debugfs_dir_dentry' among other things .
 * Returns 0 on success, a negative error code on failure.
 *
 * This function should usually be called from the module's initialization
 * function. */
int 
debug_util_init(struct dentry *debugfs_dir_dentry);

/* Finalizes debug output subsystem.
 * The function removes files if debug_util_init() created some, etc.
 *
 * This function should usually be called from the module's cleanup
 * function. */
void
debug_util_fini(void);

/* Clears the output data. For example, it may clear the contents of the 
 * files that stored information for the previous analysis session for 
 * the target module.
 *
 * This function should usually be called from on_target_load() handler
 * or the like to clear the old data. */
void
debug_util_clear(void);

/* Output a string pointed to by 's' to a debug "stream" (usually, a file 
 * in debugfs).
 *
 * This function cannot be used in atomic context. */
void
debug_util_print_string(const char *s);

/* Outputs a sequence of bytes of length 'count' as is to a debug "stream" 
 * 
 * The caller must ensure that the memory area pointed to by 'bytes' is at 
 * least 'count' bytes is size.
 *
 * This function cannot be used in atomic context. */
void
debug_util_print_raw_bytes(const void *bytes, unsigned int count);

/* Outputs the given u64 value using the specified format string 'fmt'. 
 * The format string must contain "%llu", "%llx" or the like. 
 * 
 * This function cannot be used in atomic context. */
void
debug_util_print_u64(u64 data, const char *fmt);

/* Outputs the given unsigned long value using the specified format string
 * 'fmt'. The format string must contain "%lu", "%lx" or the like. 
 * 
 * This function cannot be used in atomic context. */
void
debug_util_print_ulong(unsigned long data, const char *fmt);

/* Outputs a sequence of bytes of length 'count' to a debug "stream". 
 * Each byte is output as a hex number, the consecutive bytes are separated
 * by spaces, e.g. "0D FA 7E".
 * 
 * The caller must ensure that the memory area pointed to by 'bytes' is at 
 * least 'count' bytes is size.
 *
 * This function cannot be used in atomic context. */
void
debug_util_print_hex_bytes(const void *bytes, unsigned int count);

/* Outputs a formatted string to a debug "stream". The rules for the format
 * and the arguments are the same as for snprintf(). 
 * If the function is successful, it returns the number of characters 
 * it has written to the stream, also similar to snprintf() and the like. 
 * If the function fails, it returns a negative error code (e.g. -ENOMEM if
 * it has failed to allocate the internal buffer). 
 *
 * This function cannot be used in atomic context. */
int
debug_util_print(const char *fmt, ...);

#endif // DEBUG_UTIL_H_1104_INCLUDED
