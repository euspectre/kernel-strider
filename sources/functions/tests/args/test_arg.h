/* test_arg.h - definitions necessary for testing the retrieval of the 
 * argument values and the return value in the function call handlers. */

#ifndef TEST_ARG_H_1737_INCLUDED
#define TEST_ARG_H_1737_INCLUDED

#include <linux/kernel.h>

#ifdef CONFIG_X86_64
# define KEDR_TEST_ARG1 0x123456789abcdef0
# define KEDR_TEST_ARG2 0x23456789abcdef01
# define KEDR_TEST_ARG3 0x3456789abcdef012
# define KEDR_TEST_ARG4 0x456789abcdef0123
# define KEDR_TEST_ARG5 0x56789abcdef01234
# define KEDR_TEST_ARG6 0x6789abcdef012345
# define KEDR_TEST_ARG7 0x789abcdef0123456
# define KEDR_TEST_ARG8 0x89abcdef01234567

#else /* CONFIG_X86_32 */
# define KEDR_TEST_ARG1 0x12345678
# define KEDR_TEST_ARG2 0x23456781
# define KEDR_TEST_ARG3 0x34567812
# define KEDR_TEST_ARG4 0x45678123
# define KEDR_TEST_ARG5 0x56781234
# define KEDR_TEST_ARG6 0x67812345
# define KEDR_TEST_ARG7 0x78123456
# define KEDR_TEST_ARG8 0x81234567

#endif

/* A function with 8 parameters. As only the first 3 parameters are passed 
 * via registers on x86-32 and the first 6 - on x86-64, the remaining ones
 * will be passed on stack.
 * The function returns its start address. */
unsigned long 
kedr_test_arg_func(unsigned long arg1, unsigned long arg2, 
	unsigned long arg3, unsigned long arg4, unsigned long arg5,
	unsigned long arg6, unsigned long arg7, unsigned long arg8);

#endif /* TEST_ARG_H_1737_INCLUDED */
