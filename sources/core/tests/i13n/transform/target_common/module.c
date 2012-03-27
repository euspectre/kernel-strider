#include <linux/module.h>
#include <linux/init.h>
#include <linux/kernel.h>

/* ====================================================================== */
MODULE_AUTHOR("Eugene A. Shatokhin");
MODULE_LICENSE("GPL");
/* ====================================================================== */

/* The test functions to be called */
void kedr_test_base_reg(void);
void kedr_test_calls_jumps2_rel32(void);
void kedr_test_calls_jumps2_jcc(void);
void kedr_test_calls_jumps2_indirect(void);
void kedr_test_calls_jumps2_jmp_short(void);
void kedr_test_calls_jumps2_jcc_short(void);

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
/* ====================================================================== */

static void __exit
test_cleanup_module(void)
{
	return;
}

static int __init
test_init_module(void)
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
	
	/* [NB] When adding more tests with the functions that are actually
	 * executable rather than testing-only, consider calling these 
	 * functions here to make sure they do not crash the system. */
	return 0;
}

module_init(test_init_module);
module_exit(test_cleanup_module);
/* ====================================================================== */
