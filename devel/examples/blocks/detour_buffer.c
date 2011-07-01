/* detour_buffer.c - operations with detour buffers (the buffers where the
 * code of kernel modules is instrumented and then executed).
 *
 * API for allocation and deallocation of such buffers is provided here. */

#include <linux/kernel.h>
#include <linux/kallsyms.h>
#include <linux/errno.h>
#include <linux/string.h>

/* ====================================================================== */
/* It is needed to allocate memory close enough to the areas occupied by the 
 * kernel modules (within +/- 2Gb). Otherwise, RIP-relative addressing could
 * be a problem on x86-64. It is used, for example, when the module accesses
 * its global data. 
 *
 * For now, I cannot see a good way to ensure the memory is allocated 
 * properly. 
 * It seems from the memory layout (Documentation/x86/x86_64/mm.txt) that 
 * the only way is to use memory mapped to exactly the same region of 
 * addresses where the modules reside. The most clear way I currently see is
 * to use module_alloc() like the module loader and kernel probes do. 
 * 
 * Of course, that function is not exported and was never meant to. I look
 * for its address via kallsyms subsystem and use this address then. This
 * is an "ugly hack" and will definitely be frowned upon by kernel 
 * developers. I hope I will find a better way in the future. For example,
 * inclusion of the core parts of our instrumentation engine in the kernel
 * could mitigate the problem. */

void *(*module_alloc_func)(unsigned long) = NULL;
void (*module_free_func)(struct module *, void *) = NULL;

/* ====================================================================== */
/* This function will be called for each symbol known to the system.
 * We need to find only functions and only from the target module.
 *
 * If this function returns 0, kallsyms_on_each_symbol() will continue
 * walking the symbols. If non-zero - it will stop.
 */
static int
symbol_walk_callback(void *data, const char *name, struct module *mod, 
	unsigned long addr)
{
	/* Skip the symbol if it belongs to a module rather than to 
	 * the kernel proper. */
	if (mod != NULL) 
		return 0;
	
	if (strcmp(name, "module_alloc") == 0) {
		if (module_alloc_func != NULL) {
			pr_err("[sample] "
"Found two \"module_alloc\" symbols in the kernel, unable to continue\n");
			return -EFAULT;
		}
		module_alloc_func = (void *(*)(unsigned long))addr;
	} else if (strcmp(name, "module_free") == 0) {
		if (module_free_func != NULL) {
			pr_err("[sample] "
"Found two \"module_free\" symbols in the kernel, unable to continue\n");
			return -EFAULT;
		}
		module_free_func = (void (*)(struct module *, void *))addr;
	}
	return 0;
}

/* ====================================================================== */
int
kedr_init_detour_subsystem(void)
{
	int ret = kallsyms_on_each_symbol(symbol_walk_callback, NULL);
	if (ret)
		return ret;
	
	if (module_alloc_func == NULL) {
		pr_err("[sample] "
		"Unable to find \"module_alloc\" function\n");
		return -EFAULT;
	}
	
	if (module_free_func == NULL) {
		pr_err("[sample] "
		"Unable to find \"module_free\" function\n");
		return -EFAULT;
	}
		
	return 0; /* success */
}

void
kedr_cleanup_detour_subsystem(void)
{
	module_alloc_func = NULL;
	module_free_func = NULL;
}

void *
kedr_alloc_detour_buffer(unsigned long size)
{
	BUG_ON(module_alloc_func == NULL);
	return module_alloc_func(size);
}

void 
kedr_free_detour_buffer(void *buf)
{
	BUG_ON(module_free_func == NULL);
	if (buf != NULL)
		module_free_func(NULL, buf);
}
/* ====================================================================== */
