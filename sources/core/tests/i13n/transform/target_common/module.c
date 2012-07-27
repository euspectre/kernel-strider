#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/init.h>
#include <linux/kernel.h>

/* ====================================================================== */
MODULE_AUTHOR("Eugene A. Shatokhin");
MODULE_LICENSE("GPL");
/* ====================================================================== */

/* This parameter is only needed to guard the sequence of the calls to some 
 * functions. They will never be called but we need to make GCC think they
 * can be. */
int must_be_zero = 0;
module_param(must_be_zero, int, S_IRUGO);
/* ====================================================================== */

/* The test functions to be called */
void kedr_test_base_reg(void);
void kedr_test_calls_jumps2_rel32(void);
void kedr_test_calls_jumps2_jcc(void);
void kedr_test_calls_jumps2_indirect(void);
void kedr_test_calls_jumps2_jmp_short(void);
void kedr_test_calls_jumps2_jcc_short(void);
void kedr_test_common_type_e(void);
void kedr_test_mem_special(void);
void kedr_test_mem_special_xlat_bx(void);
void kedr_test_strings(void);
void kedr_test_locked_updates2(void);
void kedr_test_barriers_mem(void);
void kedr_test_stack_access(void);

#ifndef CONFIG_X86_64
/* Additional functions to be called on x86-32. */
void kedr_test_base_reg_no_esi(void);
void kedr_test_base_reg_no_edi(void);
void kedr_test_base_reg_no_esi_edi1(void);
void kedr_test_base_reg_no_esi_edi2(void);

#else
static void 
kedr_test_base_reg_no_esi(void)
{ }
static void 
kedr_test_base_reg_no_edi(void)
{ }
static void 
kedr_test_base_reg_no_esi_edi1(void)
{ }
static void 
kedr_test_base_reg_no_esi_edi2(void)
{ }	
#endif

/* The following function is not intended to be called but we need to
 * make the compiler and linker think it is. */
void kedr_test_io_mem(void);
/* ====================================================================== */

/* [NB] It is safer to call the test functions from the cleanup function
 * of the module than from init. 
 * If some event handler is attached, uses a separate thread to report the 
 * events somehow (e.g. workqueue) and uses kallsyms subsystem there (e.g. 
 * prints the addresses with %pf, %pS or the like), it has to be very 
 * careful when dealing with the events from the init function and the 
 * functions called from it. 
 * The problem is, 'strtab', 'symtab' and some other fields of the 
 * 'struct module' for the target that are used by kallsyms are changed in 
 * "init_module" syscall after the init function completes. The race 
 * condition on these fields may lead to kallsyms subsystem returning 
 * garbage instead of a pointer to the symbol name and therefore - to a 
 * kernel oops. */
static void __exit
test_cleanup_module(void)
{
	/* Group "base_reg" */
	kedr_test_base_reg();
	kedr_test_base_reg_no_esi();
	kedr_test_base_reg_no_edi();
	kedr_test_base_reg_no_esi_edi1();
	kedr_test_base_reg_no_esi_edi2();
	
	/* Group "calls_jumps2" */
	kedr_test_calls_jumps2_rel32();
	kedr_test_calls_jumps2_jcc();
	kedr_test_calls_jumps2_indirect();
	kedr_test_calls_jumps2_jmp_short();
	kedr_test_calls_jumps2_jcc_short();
	
	/* Group "common_type_e" */
	kedr_test_common_type_e();
	
	/* Group "mem_special" */
	kedr_test_mem_special();
	kedr_test_mem_special_xlat_bx();
	
	/* Group "strings" */
	kedr_test_strings();
	
	/* Group "locked_updates2" */
	kedr_test_locked_updates2();
	
	/* Group "barriers_mem" */
	kedr_test_barriers_mem(); 
	
	/* Group "stack_access" */
	kedr_test_stack_access();
	
	/* [NB] When adding more tests with the functions that are actually
	 * executable rather than testing-only, consider calling these 
	 * functions here to make sure they do not crash the system. */
	
	if (must_be_zero != 0) {
		/* List the calls to the functions here that must never be
		 * called but the calls to which must be present in the code
		 * somewhere. */
		kedr_test_io_mem();
	}
	return;
}

static int __init
test_init_module(void)
{
	return 0;
}

module_init(test_init_module);
module_exit(test_cleanup_module);
/* ====================================================================== */
