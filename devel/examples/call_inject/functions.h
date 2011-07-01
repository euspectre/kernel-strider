#ifndef FUNCTIONS_H_1337_INCLUDED
#define FUNCTIONS_H_1337_INCLUDED

/*
 * functions.h: declarations of the main operations with the functions in
 * the target module: enumeration, instrumentation, etc.
 */

#include <linux/module.h>
#include <linux/list.h>

/* Size of 'jmp rel32' machine instruction on x86 (both 32- and 64-bit).
 * This number of bytes at the beginning of each function of the target
 * module will be overwritten during the instrumentation. */
#define KEDR_REL_JMP_SIZE 5

/* This structure represents a function in the code of the loaded target
 * module. 
 * Such structures are needed only during instrumentation and can be dropped
 * after that. */
struct kedr_tmod_function
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
	
	/* The original byte sequence overwritten with a jump at the 
	 * beginning of the function. */
	u8 orig_start_bytes[KEDR_REL_JMP_SIZE];
	
	/* Start address of the instrumented version of the function. That
	 * code resides somewhere in a detour buffer. */
	void *instrumented_addr;
	
	/* Size of the instrumented version of the function. */
	unsigned long instrumented_size;
	
	/* The list of code blocks in the function */
	struct list_head blocks;
	
	/* The list of jump tables in the function */
	struct list_head jump_tables;
	
	// TODO: add other necessary fields here.
};

/* Initialize the function processing subsystem. 
 * This function should be called from 'on_module_load' handler for the 
 * target.
 * The function returns 0 on success, error code otherwise. */
int
kedr_init_function_subsystem(void);

/* Finalize the function processing subsystem. 
 * This function should be called from 'on_module_unload' handler for the 
 * target.*/
void
kedr_cleanup_function_subsystem(void);

/* Process the target module: load the list of its functions, instrument
 * them, etc. */
int 
kedr_process_target(struct module *target_module);

#endif // FUNCTIONS_H_1337_INCLUDED
