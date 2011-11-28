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
	 * between the start addresses of the next function and of this one
	 * most of the time. So the trailing bytes may actually be padding 
	 * area rather than belong to the function's body. */
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
	void *tbuf;
	
	/* Size of the instrumented version of the function. */
	unsigned long i_size;
	
	/* The list of jump tables for the original function (one element 
	 * per each indirect near jump of the appropriate kind). Some jump
	 * tables may have 0 elements, this can happen if the elements are 
	 * not the addresses within the function or if two jumps use the 
	 * same jump table. */
	struct list_head jump_tables;
	
	/* A buffer in the module mapping memory space containing all the 
	 * jump tables for the instrumented code. */
	void *jt_buf;
	
	/* The start address of the fallback instance of the original 
	 * function. That instance should be used if the instrumented code 
	 * detects in runtime that something bad has happened. 
	 * [NB] The fallback instance uses the fixed up jump tables for the
	 * original function (if the latter uses jump tables). */
	void *fallback;
	
	/* The list of relocations to be made when deploying the 
	 * instrumented instance of the function. */
	struct list_head relocs;
};

struct kedr_ir_node;

/* Jump tables used for near relative jumps within the function 
 * (optimized 'switch' constructs). */
struct kedr_jtable
{
	/* The tables for a given function are stored in a list */
	struct list_head list; 
	
	/* Start address; the elements will be treated as unsigned long
	 * values. */
	unsigned long *addr; 
	
	/* Number of elements. */
	unsigned int num;
	
	/* Start address of the jump table for the instrumented code (the 
	 * number of elements is the same). */
	unsigned long *i_table;
	
	/* The IR node containing the instruction that refers to this
	 * jump table. */
	struct kedr_ir_node *referrer;
};

/* 'kedr_reloc' represents an instruction in the instrumented code that 
 * should be relocated during deployment phase. */
struct kedr_reloc
{
	/* The relocations are stored in a list. */
	struct list_head list; 
	
	/* Type of the relocation. */
	enum
	{
		/* The original value (imm32 or disp32) in the instruction 
		 * does not matter. The correct value will be calculated
		 * during relocation: the displacement of the memory byte 
		 * pointed to by 'dest' from the end of the instruction. 
		 * This type of relocation is useful for the instructions
		 * that refer to something outside of the current function
		 * at a known address but contain only the 32-bit offset to
		 * that location (function calls, RIP-relative addressing). 
		 */
		KEDR_RELOC_IPREL = 0,
		
		/* The actual address of the byte following the instruction
		 * will be added to the value (imm32) in the instruction
		 * during relocation. The result will replace that previous
		 * value in the instruction.
		 * On x86-64, the process is similar. The original 'imm32' 
		 * is sign-extended before addition and the lower 32 bits
		 * of the result constitute the new 'imm32'. 
		 * This special type of relocation is useful for the 
		 * instructions that already contain the 32-bit displacement
		 * of a memory location they refer to in their 'imm32' but
		 * need to contain lower 32 bits of the actual address of
		 * that location instead. */
		KEDR_RELOC_ADDR32
	} rtype;
	
	/* The offset of the instruction in the temporary buffer (it will be
	 * the same in the final memory area too). */
	unsigned long offset;
	
	/* (Used only for type KEDR_RELOC_EXT_IPREL.)
	 * The address the instruction should refer to. 'displacement' or
	 * 'immediate' field of the instruction will be calculated based on
	 * that (whichever is applicable). */
	void *dest;
};

#endif // IFUNC_H_1626_INCLUDED
