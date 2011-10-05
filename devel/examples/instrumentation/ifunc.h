/* ifunc.h - struct kedr_ifunc and related definitions */

#ifndef IFUNC_H_1626_INCLUDED
#define IFUNC_H_1626_INCLUDED

#include <linux/list.h>

/* This structure represents a function in the code of the loaded target
 * module. */
struct kedr_ifunc
{
	struct list_head list; 
	
	/* Start address */
	void *addr; 
	
	/* Size of the code. Note that it is determined as the difference 
	 * between the start addresses of the next function and of this one.
	 * So the trailing bytes may actually be padding area rather than 
	 * belong to the function's body. */
	unsigned long size;
	
	/* Name of the function */
	/* [NB] Is it safe to keep only a pointer? The string itself is in
	 * the string table of the module and that table is unlikely to go 
	 * away before the module is unloaded. 
	 * See module_kallsyms_on_each_symbol().*/ 
	const char *name;
	
	/* The start address of the instrumented version of the function 
	 * in a detour buffer. */
	void *i_addr;
	
	/* The start address of a temporary buffer for the instrumented 
	 * instance of a function. */
	void *tbuf_addr;
	
	/* Size of the instrumented version of the function. */
	unsigned long i_size;
	
	/* The list of jump tables for the original function (one element 
	 * per each indirect near jump of the appropriate kind). Some jump
	 * tables may have 0 elements, this can happen if the elements are 
	 * not the addresses within the function or if two jumps use the 
	 * same jump table. */
	struct list_head jump_tables;
	
	/* The number of elements in 'jump_tables' list. */
	unsigned int num_jump_tables; 
	
	/* The array of pointers to the jump tables for the instrumented
	 * function instance. 
	 * Number of tables: 'num_jump_tables'. */
	unsigned long **i_jump_tables;
	
	/* The start address of the fallback instance of the original 
	 * function. That instance should be used if the instrumented code 
	 * detects in runtime that something bad has happened. 
	 * [NB] The fallback instance uses the fixed up jump tables for the
	 * original function (if the latter uses jump tables). */
	void *fallback;
};

/* Jump tables used for near relative jumps within the function 
 * (optimized 'switch' constructs, etc.) */
struct kedr_jtable
{
	/* The list of tables for a given function */
	struct list_head list; 
	
	/* Start address; the elements will be treated as unsigned long
	 * values. */
	unsigned long *addr; 
	
	/* Number of elements */
	unsigned int num;
};

#endif // IFUNC_H_1626_INCLUDED
