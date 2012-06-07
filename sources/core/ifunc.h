/* ifunc.h - the structures describing the functions in the target module
 * and the API to manage the collection of these functions. */

#ifndef IFUNC_H_1626_INCLUDED
#define IFUNC_H_1626_INCLUDED

#include <linux/list.h>
#include <kedr/kedr_mem/functions.h>

struct kedr_i13n;

/* The following "instances" of a function in the target module are
 * considered here.
 *
 * 1. "Original instance" - the function in the target module as it is, 
 * without any instrumentation of whatever. When the target is 
 * instrumented, a jump is placed at the beginning of the original 
 * instance of a function that leads to the instrumented instance of that
 * function. The remaining part of the original instance should never be 
 * executed in this case.
 *
 * 2. "Instrumented instance" - the instrumented code of the function 
 * created in a memory buffer ("detour buffer"). This instance is executed
 * instead of the original one. During the execution, it collects the
 * information about the behaviour of the target module. It depends on the
 * instrumentation, which exactly information is collected this way. 
 *
 * 3. "Fallback instance" - basically, a copy of the original instance,
 * properly relocated to be operational. On entry, the instrumented instance 
 * must perform some initialization: allocate space for local storage, etc.
 * If this fails (e.g., in low memory conditions), the instrumented instance
 * cannot be used. But the original instance cannot be used either: during
 * the instrumentation a jump was placed at its beginning. After all the 
 * notifications about the loading of the target module had been processed,
 * the module loader might have made the code of the target module read only
 * ("RO/NX" protection). Therefore, we cannot restore the original instance 
 * and pass control there if the initialization fails in the instrumented
 * instance. This is what the fallback instance is for. The control will be
 * passed there instead, and the fallback instance should operate as the 
 * original instance would. */

/* This structure represents a function in the code of the loaded target
 * module. */
struct kedr_ifunc
{
	struct list_head list; 
	
	/* The information about the function (it can be needed in runtime
	 * too). */
	struct kedr_func_info info;
	
	/* Size of the code. Actually, it is the "upper bound" on that size.
	 * The last bytes of the code may be a padding of some kind rather 
	 * than the real code of the function. */
	unsigned long size;
	
	/* Name of the function */
	/* [NB] Is it safe to keep only a pointer? The string itself is in
	 * the string table of the module and that table is unlikely to go 
	 * away before the module is unloaded. 
	 * See module_kallsyms_on_each_symbol().*/ 
	const char *name;
	
	/* The list of jump tables (one element per each indirect near jump 
	 * of the appropriate kind). Some jump tables may have 0 elements, 
	 * this can happen if the elements are not the addresses within 
	 * the function or if two jumps use the same jump table. */
	struct list_head jump_tables;
	
	/* The start address of the instrumented version of the function 
	 * in a detour buffer. */
	void *i_addr;
	
	/* The start address of a temporary buffer for the instrumented 
	 * instance of a function. */
	void *tbuf;
	
	/* Size of the instrumented version of the function. */
	unsigned long i_size;
	
	/* A buffer in the module mapping memory space containing all the 
	 * jump tables for the instrumented code. */
	void *jt_buf;
	
	/* The start address of the fallback instance of the function. 
	 * That instance should be used if the instrumented code detects 
	 * in runtime that something bad has happened. 
	 * [NB] The fallback instance uses the fixed up jump tables for the
	 * original function (if the latter uses jump tables). */
	void *fallback;
	
	/* The list of relocations to be made when deploying the 
	 * instrumented instance of the function. */
	struct list_head relocs;
	
	/* The list of kedr_block_info structures created for this function.
	 * These structures must live until this kedr_ifunc instance is
	 * destroyed (they are used when the target module is working). */
	struct list_head block_infos;
	
	/* The list of kedr_call_info structures created for this function.
	 * These structures must live until this kedr_ifunc instance is
	 * destroyed (they are used when the target module is working). */
	struct list_head call_infos;
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

/* Types of the relocations that can be performed at the deployment phase.*/
enum kedr_reloc_type
{
	/* The original value (imm32 or disp32) in the instruction does not
	 * matter. The correct value will be calculated during relocation: 
	 * the displacement of the memory byte pointed to by 'dest' from the 
	 * end of the instruction. 
	 * This type of relocation is useful for the instructions that refer 
	 * to something outside of the current function at a known address
	 * but contain only the 32-bit offset of that location (function 
	 * calls, RIP-relative addressing). */
	KEDR_RELOC_IPREL = 0,
	
	/* The actual address of the byte following the instruction will be 
	 * added to the value (imm32) in the instruction during relocation.
	 * The result will replace that previous value in the instruction.
	 * On x86-64, the process is similar. The original 'imm32' is 
	 * sign-extended before addition and the lower 32 bits of the result
	 * constitute the new 'imm32'. 
	 * This special type of relocation is useful for the instructions
	 * that already contain the 32-bit displacement of a memory location
	 * they refer to in their 'imm32' but need to contain lower 32 bits
	 * of the actual address of that location instead. */
	KEDR_RELOC_ADDR32
};

/* 'kedr_reloc' represents an instruction in the instrumented code that 
 * should be relocated during deployment phase. */
struct kedr_reloc
{
	/* The relocations are stored in a list. */
	struct list_head list; 
	
	/* Type of the relocation. */
	enum kedr_reloc_type rtype;
	
	/* The offset of the instruction in the temporary buffer (it will be
	 * the same in the final memory area too). */
	unsigned long offset;
	
	/* (Used only for type KEDR_RELOC_EXT_IPREL.)
	 * The address the instruction should refer to. 'displacement' or
	 * 'immediate' field of the instruction will be calculated based on
	 * that (whichever is applicable). */
	void *dest;
};

/* Find the functions to be instrumented in the target module, create 
 * kedr_ifunc instance for each of them and add to 'i13n->ifuncs' list. 
 * The list should be empty on entry.
 * 0 is returned on success. If an error occurs, -errno is returned and 
 * '*ifuncs' remains empty. 
 *
 * Before calling kedr_get_functions(), make sure the list of target's 
 * sections is populated in 'i13n'. It is not allowed to call this function
 * when the list of sections is empty or contains invalid data. */
int
kedr_get_functions(struct kedr_i13n *i13n);

/* Empty 'i13n->ifuncs' list and properly delete the elements it contains.*/
void
kedr_release_functions(struct kedr_i13n *i13n);

#endif // IFUNC_H_1626_INCLUDED
