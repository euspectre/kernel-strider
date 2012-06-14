/* resolve_ip.h - support for obtaining information about a function given
 * an address of some location in its instrumented code. This should 
 * simplify the analysis of the problems in the target module that showed up
 * in its instrumented instance.
 * 
 * A user can make a request to the core to resolve IP of a location within 
 * the instrumented code by writing that IP to "kedr_mem_core/i_addr" file
 * in debugfs (as a hex value possibly prefixed with "0x"). 
 * If the core resolves the IP successfully, the information about the 
 * function will be available for reading in other files in that directory
 * in debugfs:
 *
 * - "func_name" - name of the function which instrumented instance the IP 
 *   belongs to (if you need the start address of the original function,
 *   "/proc/kallsyms" may be useful); 
 *
 * - "func_i_start" - start address of the instrumented instance of the 
 *   function. */

#ifndef RESOLVE_IP_H_1621_INCLUDED
#define RESOLVE_IP_H_1621_INCLUDED

#include <linux/fs.h>

/* Initialize the subsystem, create appropriate files in the given directory
 * in debugfs. */
int 
kedr_init_resolve_ip(struct dentry *debugfs_dir);

/* Clean up the subsystem (delete its files in debugfs, etc.). */
void
kedr_cleanup_resolve_ip(void);

#endif /* RESOLVE_IP_H_1621_INCLUDED */
