/* functions.c: main operations with the functions in the target module:
 * enumeration, instrumentation, etc. 
 * 
 * Unless specifically stated, the function returning int return 0 on 
 * success and a negative error code on failure. */

#include <linux/kernel.h>
#include <linux/kallsyms.h>
#include <linux/errno.h>
#include <linux/module.h>
#include <linux/sort.h>
#include <linux/slab.h>
#include <linux/string.h>

#include <asm/insn.h>       /* instruction decoder machinery */

#include "functions.h"
#include "debug_util.h"
#include "detour_buffer.h"
/* ====================================================================== */

extern char* target_function; /* the name of the function to processs */
/* ====================================================================== */

/* Initial number of elements for dynamic arrays. */
#define KEDR_BASE_ARRAY_SIZE 8
/* ====================================================================== */

/* A block of code in a function. The block contains one or more machine
 * instructions. 
 * The rules used to split the function code into such blocks: 
 * - if an instruction may transter control outside of the current function,
 *    it constitutes a separate block; note that in addition to some of the 
 *    calls and jumps, the instructions like 'ret' and 'int' fall into this 
 *    group;
 * - if an instruction transfers control to a location before it within the 
 *    function (a "backward jump" in case of 'for'/'while'/'do' constructs, 
 *    etc.), it constitutes a separate block;
 *    note that rep-prefixed instructions do not fall into this group;
 * - each 'jmp near r/m32' instruction constitutes a separate block, same
 *    for 'jmp near r/m64';
 * - near indirect jumps must always transfer control to the beginning of
 *    a block;
 * - if an instruction transfers control to a location before it within the 
 *    function, it is allowed to transfer control only to the beginning of 
 *    a block; 
 * - it is allowed for a block to contain the instructions that transfer 
 *    control forward within the function, not necessarily within the block
 *    such instructions need not be placed in separate blocks. */
struct kedr_code_block
{
	/* The code blocks for a given function are linked in the list */
	struct list_head list; 
	
	/* Start address */
	void *addr; 
	
	/* Size of the code */
	unsigned int size;
};

/* Jump tables used for near relative jumps within the function 
 * (optimized 'switch' constructs, etc.) */
struct kedr_jump_table
{
	/* The list of tables for a given function */
	struct list_head list; 
	
	/* Start address; the elements will be treated as unsigned long
	 * values. */
	unsigned long *addr; 
	
	/* Number of elements */
	unsigned int num;
};

/* ====================================================================== */
/* Maximum size of a machine instruction on x86, in bytes. Actually, 15
 * would be enough. From Intel Software Developer's Manual Vol2A, section 
 * 2.2.1: "The instruction-size limit of 15 bytes still applies <...>".
 * We just follow the implementation of kernel probes in this case. */
/*#define KEDR_MAX_INSN_SIZE 16*/

/* Some opcodes */
#define KEDR_OP_JMP_REL32	0xe9
#define KEDR_OP_CALL_REL32	0xe8

/* KEDR_ADDR_FROM_OFFSET()
 * 
 * Calculate the memory address being the operand of a given instruction 
 * that uses IP-relative addressing ('call near', 'jmp near', ...). 
 *   'insn_addr' is the address of the instruction itself,
 *   'insn_len' is length of the instruction in bytes,
 *   'offset' is the offset of the destination address from the first byte
 *   past the instruction.
 * 
 * For x86-64 architecture, the offset value is sign-extended here first.
 * 
 * "Intel x86 Instruction Set Reference" states the following 
 * concerning 'call rel32':
 * 
 * "Call near, relative, displacement relative to next instruction.
 * 32-bit displacement sign extended to 64 bits in 64-bit mode." */
#ifdef CONFIG_X86_64
# define KEDR_ADDR_FROM_OFFSET(insn_addr, insn_len, offset) \
	(void *)((s64)(insn_addr) + (s64)(insn_len) + (s64)(s32)(offset))

#else /* CONFIG_X86_32 */
# define KEDR_ADDR_FROM_OFFSET(insn_addr, insn_len, offset) \
	(void *)((u32)(insn_addr) + (u32)(insn_len) + (u32)(offset))
#endif

/* KEDR_OFFSET_FROM_ADDR()
 * 
 * The reverse of KEDR_ADDR_FROM_OFFSET: calculates the offset value
 * to be used in an instruction given the address and length of the
 * instruction and the destination address it must refer to. */
#define KEDR_OFFSET_FROM_ADDR(insn_addr, insn_len, dest_addr) \
	(u32)(dest_addr - (insn_addr + (u32)insn_len))

/* KEDR_SIGN_EXTEND_V32_TO_ULONG()
 *
 * Just a cast to unsigned long on x86-32. 
 * On x86-64, sign-extends a 32-bit value to and casts the result to 
 * unsigned long */
#define KEDR_SIGN_EXTEND_V32_TO_ULONG(val) ((unsigned long)(long)(s32)(val))

/* ====================================================================== */
/* Alignment of the start addresses of the instrumented functions (in 
 * bytes). The start address of the detour buffer will usually be 
 * page-aligned but it may also be desirable to align the start address of
 * each function. 
 *
 * KEDR_FUNC_ALIGN must be a power of 2. */
#define KEDR_FUNC_ALIGN 0x10UL

/* Align the value '_val', that is, round it up to the multiple of 
 * KEDR_FUNC_ALIGN. */
#define KEDR_ALIGN_VALUE(_val) \
  (((unsigned long)(_val) + KEDR_FUNC_ALIGN - 1) & ~(KEDR_FUNC_ALIGN - 1))

/* Detour buffer for the target module. The instrumented code of the 
 * functions will be copied there. It is that code that will actually be
 * executed. A jump to the start of the instrumented function will be placed
 * at the beginning of the original function, so the rest of the latter 
 * should never be executed. */
void *dbuf = NULL; 

/* The list of functions (struct kedr_tmod_function) found in the target 
 * module. */
LIST_HEAD(tmod_funcs);

/* Number of functions in the target module */
unsigned int num_funcs = 0;

/* ====================================================================== */
/* Destroy all the structures contained in 'tmod_funcs' list and remove them
 * from the list, leaving it empty. */
static void
tmod_funcs_destroy_all(void)
{
	struct kedr_tmod_function *pos;
	struct kedr_tmod_function *tmp;
	
	list_for_each_entry_safe(pos, tmp, &tmod_funcs, list) {
		list_del(&pos->list);
		kfree(pos);
	}
}

/* Remove from the list and destroy the elements with zero size. 
 * Such elements may appear if there are aliases for one or more functions,
 * that is, if there are symbols with the same start address. When doing the
 * instrumentation, we need to process only one function of each such group,
 * no matter which one exactly. */
static void
tmod_funcs_remove_aliases(void)
{
	struct kedr_tmod_function *pos;
	struct kedr_tmod_function *tmp;
	
	list_for_each_entry_safe(pos, tmp, &tmod_funcs, list) {
		if (pos->size == 0) {
			list_del(&pos->list);
			kfree(pos);
		}
	}
}

/* ====================================================================== */
/* Estimate the size of the buffer (in bytes) needed to contain the 
 * instrumented variant of the function specified by 'func'. The returned 
 * size must be greater than or equal to the size of the instrumented 
 * function.
 * 
 * Alignment of the start address of the function is handled at the upper
 * level, no need to take it into account here.
 * 
 * The function returns nonzero size if successful, 0 if an error occurs.
 * The only thing that might fail here is the instruction decoder if it 
 * does not process some byte sequence properly. Ideally, this should not
 * happen. */
static unsigned long
estimate_func_buf_size(struct kedr_tmod_function *func)
{
	BUG_ON(func == NULL || func->addr == NULL);
	
	/* Should not happen because we should have skipped aliases at the 
	 * upper level. Just a bit of extra self-control. */
	WARN_ON(func->size == 0); 
	
	// TODO
	// For now, just return the size of the original function. In a real
	// system we need to estimate the size of the instrumented function
	// instead.
	return func->size;
}

/* Estimate the size of the detour buffer to contain all the instrumented 
 * functions and determine size for each instrumented function (it will be 
 * stored in 'instrumented_size' field of the appropriate kedr_tmod_function 
 * structures). 
 * 
 * It is assumed that the target module contains at least one function. 
 * 
 * The function returns the total size estimate of the detour buffer if 
 * successful, 0 otherwise. */

// TODO: rewrite to really estimate the instrumented size.
static unsigned long
estimate_detour_buf_size(void)
{
	/* extra bytes in case the start address is not aligned */
	unsigned long total_size = KEDR_FUNC_ALIGN; 
	struct kedr_tmod_function *pos;
		
	BUG_ON(list_empty(&tmod_funcs));
	
	list_for_each_entry(pos, &tmod_funcs, list) {
		unsigned long size = estimate_func_buf_size(pos);
		if (size == 0) {
			pr_err("[sample] "
	"Failed to determine the size of the buffer for function \"%s\"\n",
				pos->name);
			return 0;
		}
		
		pos->instrumented_size = size;
		total_size += KEDR_ALIGN_VALUE(size);
	}
	return total_size;
}

/* Set the start addresses of the instrumented functions (store them in 
 * 'instrumented_addr' fields of the appropriate kedr_tmod_function 
 * structures). */
static void
set_instrumented_addrs(void)
{
	struct kedr_tmod_function *pos;
	void *addr;
	
	BUG_ON(dbuf == NULL);
	
	addr = (void *)KEDR_ALIGN_VALUE(dbuf);
	list_for_each_entry(pos, &tmod_funcs, list) {
		BUG_ON(pos->instrumented_size == 0);
		
		pos->instrumented_addr = addr;
		addr = (void *)((unsigned long)addr + 
			KEDR_ALIGN_VALUE(pos->instrumented_size));
	}
}

/* Allocate the detour buffer and prepare kedr_tmod_function structures for
 * the instrumentation in that buffer. 
 * 
 * The function returns 0 if successful, error code otherwise. */
static int
prepare_funcs_for_detour(void)
{
	unsigned long db_size = 0;
	
	db_size = estimate_detour_buf_size();
	if (db_size == 0) 
		return -EFAULT;
	
	dbuf = kedr_alloc_detour_buffer(db_size);
	if (dbuf == NULL) {
		pr_err("[sample] "
			"Failed to allocate detour buffer of size %lu\n",
			db_size);
		return -ENOMEM;
	}
	memset(dbuf, 0, (size_t)db_size);
	
	set_instrumented_addrs();
	
	//<>
	{
	//	struct kedr_tmod_function *pos;
		pr_info("[sample] "
			"Allocated detour buffer of size %lu at 0x%p\n",
			db_size, dbuf);
			
	/*	list_for_each_entry(pos, &tmod_funcs, list) {
			pr_info("[sample] "
	"function \"%s\": address is %p, size is %lu, " 
	"'detoured' address is %p, size is %lu.\n",
				pos->name,
				pos->addr, pos->size,
				pos->instrumented_addr, 
				pos->instrumented_size);
		}*/
	}
	//<>
	return 0; /* success */
}

/* ====================================================================== */
int
kedr_init_function_subsystem(void)
{
	num_funcs = 0;
	
	// TODO: more initialization tasks here if necessary
	return 0; /* success */
}

void
kedr_cleanup_function_subsystem(void)
{
	// TODO: more cleanup tasks here if necessary
	tmod_funcs_destroy_all();
	kedr_free_detour_buffer(dbuf);
}

/* ====================================================================== */
/* Called for each function found in the target module.
 * 
 * Returns 0 if the processing succeeds, error otherwise.
 * This error will be propagated to the return value of 
 * kallsyms_on_each_symbol() */
static int
do_process_function(const char *name, struct module *mod, 
	unsigned long addr)
{
	struct kedr_tmod_function *tf;
	tf = (struct kedr_tmod_function *)kzalloc(
		sizeof(struct kedr_tmod_function),
		GFP_KERNEL);
	if (tf == NULL)
		return -ENOMEM;
	
	tf->addr = (void *)addr; /* [NB] tf->size is 0 now */
	tf->name = name;
	INIT_LIST_HEAD(&tf->blocks);
	INIT_LIST_HEAD(&tf->jump_tables);
	
	list_add(&tf->list, &tmod_funcs);
	++num_funcs;

	return 0;
}

/* Nonzero if 'addr' is the address of some location in the code of the 
 * given module (*.text sections), 0 otherwise.
 */
static int
is_text_address(unsigned long addr, struct module *mod)
{
	BUG_ON(mod == NULL);

	if ((mod->module_core != NULL) &&
	    (addr >= (unsigned long)(mod->module_core)) &&
	    (addr < (unsigned long)(mod->module_core) + mod->core_text_size))
		return 1;

	if ((mod->module_init != NULL) &&
	    (addr >= (unsigned long)(mod->module_init)) &&
	    (addr < (unsigned long)(mod->module_init) + mod->init_text_size))
		return 1;
	
	return 0;
}

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
	struct module *target_module = (struct module *)data;
	/* For now it seems to be enough to compare only addresses of 
	 * struct module instances for the target module and the module
	 * the current symbol belongs to. 
	 */
	 
	if (mod == target_module && 
	    name[0] != '\0' && /* skip symbols with empty name */
	    is_text_address(addr, mod) && 
	    strcmp(name, "init_module") != 0 &&  /* skip common aliases */
	    strcmp(name, "cleanup_module") != 0) {
	 	int ret = do_process_function(name, mod, addr);
	 	if (ret)
			return ret;
	}
	return 0;
}

static int 
function_compare_by_address(const void *lhs, const void *rhs)
{
	const struct kedr_tmod_function *left = 
		*(const struct kedr_tmod_function **)(lhs);
	const struct kedr_tmod_function *right = 
		*(const struct kedr_tmod_function **)(rhs);
	
	if (left->addr == right->addr)
		return 0;
	else if (left->addr < right->addr)
		return -1;
	else 
		return 1;
}

static void 
ptr_swap(void *lhs, void *rhs, int size)
{
	struct kedr_tmod_function **left = 
		(struct kedr_tmod_function **)(lhs);
	struct kedr_tmod_function **right = 
		(struct kedr_tmod_function **)(rhs);
	struct kedr_tmod_function *p;
	
	p = *left;
	*left = *right;
	*right = p;
}

/* Loads the list of functions from the given module to the internal 
 * structures for future processing. */
static int
kedr_load_function_list(struct module *target_module)
{
	struct kedr_tmod_function **pfuncs = NULL;
	struct kedr_tmod_function init_text_end;
	struct kedr_tmod_function core_text_end;
	struct kedr_tmod_function *pos;
	int ret; 
	int i;
	
	BUG_ON(target_module == NULL);
	
	ret = kallsyms_on_each_symbol(symbol_walk_callback, 
		(void *)target_module);
	if (ret)
		return ret;
	
	if (num_funcs == 0) {
		pr_info("[sample] "
			"No functions found in \"%s\", nothing to do\n",
			module_name(target_module));
		return 0;
	} 
	
	pr_info("[sample] "
		"Found %u functions in \"%s\"\n",
		num_funcs,
		module_name(target_module));
	
	/* This array is only necessary to estimate the size of each 
	 * function.
	 * The 2 extra elements are for the address bounds, namely for the 
	 * addresses immediately following "init" and "core" areas of 
	 * code.
	 * 
	 * [NB] If there are aliases (except "init_module" and 
	 * "cleanup_module"), i.e. the symbols with different names and 
	 * the same addresses, the size of only one of the symbols in such 
	 * group will be non-zero. We can just skip symbols with size 0.
	 */
	pfuncs = (struct kedr_tmod_function **)kzalloc(
		sizeof(struct kedr_tmod_function *) * (num_funcs + 2), 
		GFP_KERNEL);
		
	if (pfuncs == NULL)
		return -ENOMEM;
	
	i = 0;
	list_for_each_entry(pos, &tmod_funcs, list) {
		pfuncs[i++] = pos;
	}

	/* We only need to initialize the addresses for these fake 
	 * "functions" */
	if (target_module->module_init) {
		init_text_end.addr = target_module->module_init + 
			target_module->init_text_size;
		pfuncs[i++] = &init_text_end;
	}
	if (target_module->module_core) {
		core_text_end.addr = target_module->module_core + 
			target_module->core_text_size;
		pfuncs[i++] = &core_text_end;
	}
	
	/* NB: sort 'i' elements, not 'num_funcs' */
	sort(pfuncs, (size_t)i, sizeof(struct kedr_tmod_function *), 
		function_compare_by_address, ptr_swap);
	
	/* The last element should now be the end of init or core area. */
	--i;
	WARN_ON(pfuncs[i] != &core_text_end && 
		pfuncs[i] != &init_text_end);
	
	while (i-- > 0) {
		pfuncs[i]->size = 
			((unsigned long)(pfuncs[i + 1]->addr) - 
			(unsigned long)(pfuncs[i]->addr));
	}
	kfree(pfuncs);
	
	tmod_funcs_remove_aliases();
	
	BUG_ON(list_empty(&tmod_funcs));
	return 0;
}

/* Copy the (already decoded) instruction to 'dest' and check if the 
 * instruction references memory relative to the next byte (like near 
 * relative calls and jumps and instructions with RIP-relative addressing
 * mode). 
 * If so, fixup the copied instruction if it addresses memory outside 
 * of the current function. 
 * [NB] If it is a call to some other function in this module, the copied 
 * instruction will point to the original function. If it is a recursive
 * call to the same function, no fixup is necessary. */
static void
copy_and_fixup_insn(struct insn *src_insn, void *dest, 
	const struct kedr_tmod_function *func)
{
	u32 *to_fixup;
	unsigned long addr;
	BUG_ON(src_insn->length == 0);
	
	memcpy((void *)dest, (const void *)src_insn->kaddr, 
		src_insn->length);
	
	if (src_insn->opcode.bytes[0] == KEDR_OP_CALL_REL32 ||
	    src_insn->opcode.bytes[0] == KEDR_OP_JMP_REL32) {
			
		/* For some obscure reason, the decoder stores the offset
		 * in 'immediate' field rather than in 'displacement'.
		 * [NB] When dealing with RIP-relative addressing on x86-64,
		 * it uses 'displacement' field as it should. */
		addr = (unsigned long)KEDR_ADDR_FROM_OFFSET(
			src_insn->kaddr,
			src_insn->length, 
			src_insn->immediate.value);
		
		if (addr >= (unsigned long)func->addr && 
		    addr < (unsigned long)func->addr + func->size)
			return; /* no fixup necessary */
		
		/* Call or jump outside of the function, fix it up. */
		to_fixup = (u32 *)((unsigned long)dest + 
			insn_offset_immediate(src_insn));
		*to_fixup = KEDR_OFFSET_FROM_ADDR(dest, src_insn->length,
			(void *)addr);
		return;
	}

#ifdef CONFIG_X86_64
	if (!insn_rip_relative(src_insn))
		return;
		
	/* Handle RIP-relative addressing */
	addr = (unsigned long)KEDR_ADDR_FROM_OFFSET(
		src_insn->kaddr,
		src_insn->length, 
		src_insn->displacement.value);
	
	/* Check if the instruction addresses something inside this 
	 * function. If so, no fixup is necessary. */
	if (addr >= (unsigned long)func->addr && 
	    addr < (unsigned long)func->addr + func->size)
		return;
	
	to_fixup = (u32 *)((unsigned long)dest + 
		insn_offset_displacement(src_insn));
	*to_fixup = KEDR_OFFSET_FROM_ADDR(dest, src_insn->length,
		(void *)addr);
#endif
	return;
}

/* Return 0 for the instructions that do not alter control flow (that is, do
 * not jump). 
 * For near relative calls as well as short and near relative jumps, the 
 * function returns the destination address. 
 * For other kinds of calls and jumps as well as for 'int' and 'ret' 
 * instruction families, the function returns (unsigned long)(-1).
 * 
 * The value returned by this function can be used to determine whether an
 * instruction transfers control inside or outside of a given function
 * (except for indirect jumps that should be handled separately; the 
 * function returns (unsigned long)(-1) for them). */
static unsigned long
insn_jumps_to(struct insn *insn)
{
	u8 opcode = insn->opcode.bytes[0];
	
	/* jcc short, jmp short */
	if ((opcode >= 0x70 && opcode <= 0x7f) || (opcode == 0xe3) || 
	    opcode == 0xeb) {
		s32 offset = (s32)(s8)insn->immediate.bytes[0];
		return (unsigned long)KEDR_ADDR_FROM_OFFSET(insn->kaddr, 
			insn->length, offset); 
	}
	
	/* call/jmp/jcc near relative */
	if (opcode == 0xe8 || opcode == 0xe9 || 
	    (opcode == 0x0f && (insn->opcode.bytes[1] & 0xf0) == 0x80)) {
		return (unsigned long)KEDR_ADDR_FROM_OFFSET(insn->kaddr, 
			insn->length, insn->immediate.value); 
	}
	
	/* int*, ret* */
	if ((opcode >= 0xca && opcode <= 0xce) || 
	    opcode == 0xc2 || opcode == 0xc3)
		return (unsigned long)(-1); 
	
	/* loop* */
	if (opcode >= 0xe0 && opcode <= 0xe2) {
		s32 offset = (s32)(s8)insn->immediate.bytes[0];
		return (unsigned long)KEDR_ADDR_FROM_OFFSET(insn->kaddr, 
			insn->length, offset); 
	}
	
	/* indirect calls and jumps, near and far */
	if (opcode == 0xff) {
		int aux_code = X86_MODRM_REG(insn->modrm.value);
		if (aux_code >= 2 && aux_code <= 5)
			return (unsigned long)(-1); 
		else /* flavours of inc, dec and push */
			return 0;
	}
	
	/* call/jump far absolute ptr16:32;  */
	if (opcode == 0x9a || opcode == 0xea)
		return (unsigned long)(-1); 
	
	return 0; /* no jump */
}
/* ====================================================================== */

/* For each instruction in [start_addr; end_addr), the function decodes it
 * and calls proc() callback for it. 'data' is passed to proc() as the 
 * last argument, it can be a pointer to the custom data needed by the 
 * particular callback. 
 *
 * [NB] The address of the instruction can be obtained in proc() via
 * insn->kaddr field.
 *
 * proc() is expected to return 0 on success and a negative error code on 
 * failure. do_for_each_insn() continues as long as there are instructions 
 * left and proc() returns 0. If proc() returns nonzero, do_for_each_insn()
 * stops and returns this value.
 * 
 * Use this function instead of explicit walking, decoding and processing 
 * the areas of code (you remember C++ and STL best practices, right?). */
static int 
do_for_each_insn(unsigned long start_addr, unsigned long end_addr,
	int (*proc)(struct insn *, void *), void *data) 
{
	struct insn insn;
	int ret;
	
	while (start_addr < end_addr) {
		kernel_insn_init(&insn, (void *)start_addr);
		insn_get_length(&insn);  /* Decode the instruction */
		if (insn.length == 0) {
			pr_err("[sample] "
		"Failed to decode instruction at %p\n",
				(const void *)start_addr);
			return -EILSEQ;
		}
		
		ret = proc(&insn, data); /* Process the instruction */
		if (ret != 0)
			return ret;
		
		start_addr += insn.length;
	}
	return 0;
}
/* ====================================================================== */

struct kedr_data_detect_block_bounds 
{
	struct kedr_tmod_function *func;
	struct module *mod;
	
	/* Dynamic array to store the offsets of block boundaries, possibly
	 * with duplicates. */
	u32 *block_offsets; 
	
	/* Maximum number of elements the memory is reserved for in 
	 * 'block_offsets' array.*/
	unsigned int capacity; 
	
	/* Actual number of elements in 'block_offsets' */
	unsigned int num; 
};

/* If necessary, enlarge the memory space reserved for 'block_offsets' to 
 * accomodate 'num_new_elem' additional elements. */
static int
block_offsets_reserve(struct kedr_data_detect_block_bounds *ddbb, 
	unsigned int num_add_elem) 
{
	u32 *p;
	unsigned int needed_elems = ddbb->num + num_add_elem;
	if (needed_elems <= ddbb->capacity)
		return 0; /* OK, already enough, nothing to do */
	
	/* We reserve memory for at least KEDR_BASE_ARRAY_SIZE elements. */
	ddbb->capacity += KEDR_BASE_ARRAY_SIZE;
	if (needed_elems > ddbb->capacity)
		ddbb->capacity = needed_elems;
	
	p = (u32 *)krealloc(ddbb->block_offsets, 
		ddbb->capacity * sizeof(u32), 
		GFP_KERNEL | __GFP_ZERO);
	if (p == NULL)
		return -ENOMEM;
	
	ddbb->block_offsets = p;
	return 0;
}

/* Return nonzero if the given tables overlap, 0 otherwise. */
static int jtables_overlap(struct kedr_jump_table *jtable1,
	struct kedr_jump_table *jtable2)
{
	if (jtable2->addr <= jtable1->addr) {
		unsigned long jtable2_end = (unsigned long)jtable2->addr + 
			jtable2->num * sizeof(unsigned long);
		return (jtable2_end > (unsigned long)jtable1->addr);
	} 
	else { /* jtable2->addr > jtable1->addr */
		unsigned long jtable1_end = (unsigned long)jtable1->addr + 
			jtable1->num * sizeof(unsigned long);
		return (jtable1_end > (unsigned long)jtable2->addr);
	}
}

/* Check if this jump table and some jump tables processed earlier overlap,
 * and if so, adjust numbers of elements as necessary to eliminate this. 
 * 
 * Call this function before adding jtable to the list of jump tables 
 * in 'func'. */
static void
resolve_jtables_overlaps(struct kedr_jump_table *jtable, 
	struct kedr_tmod_function *func)
{
	struct kedr_jump_table *pos;
	list_for_each_entry(pos, &func->jump_tables, list) {
		if (!jtables_overlap(jtable, pos))
			continue;
		
		/* Due to the way the tables are searched for, they must end
		 * at the same address if they overlap. 
		 * 
		 * [NB] Wnen adding we take into account that *->addr is 
		 * a pointer to unsigned long. */
		WARN_ON(jtable->addr + jtable->num != pos->addr + pos->num);
		//<>
		/*if (jtable->addr + jtable->num != pos->addr + pos->num) {
			pr_info("[DBG] jtable at %p with %u elems; "
				"pos at %p with %u elems\n",
				jtable->addr, jtable->num,
				pos->addr, pos->num);
		}*/
		//<>
		
		if (jtable->addr == pos->addr) {
			jtable->num = 0;
		} 
		else if (pos->addr < jtable->addr) {
			pos->num = (unsigned int)((int)pos->num - 
				(int)jtable->num);
		}
		else { /* jtable->addr < pos->addr */
			jtable->num = (unsigned int)((int)jtable->num - 
				(int)pos->num);
		}
	}
}

static int 
handle_jmp_near_indirect(struct insn *insn, 
	struct kedr_data_detect_block_bounds *ddbb)
{
	unsigned long jtable_addr;
	unsigned long init_start; 
	unsigned long init_end; 
	unsigned long core_start; 
	unsigned long core_end; 
	unsigned long func_start;
	unsigned long func_end;
	unsigned long end_addr = 0;
	int in_init = 0;
	int in_core = 0;
	struct kedr_tmod_function *func = ddbb->func;
	struct module *mod = ddbb->mod;
	unsigned long pos;
	unsigned int num_elems;
	unsigned int i;
	struct kedr_jump_table *jtable;
	int ret = 0;
	
	func_start = (unsigned long)func->addr;
	func_end = func_start + func->size;
	
	init_start = (unsigned long)mod->module_init;
	init_end = init_start + (unsigned long)mod->init_size;
	core_start = (unsigned long)mod->module_core;
	core_end = core_start + (unsigned long)mod->core_size;
	
	jtable_addr = 
		KEDR_SIGN_EXTEND_V32_TO_ULONG(insn->displacement.value);
	
	if (jtable_addr >= core_start && jtable_addr < core_end) {
		in_core = 1;
		end_addr = core_end - sizeof(unsigned long);
	}
	else if (jtable_addr >= init_start && jtable_addr < init_end) {
		in_init = 1;
		end_addr = init_end - sizeof(unsigned long);
	}
	
	/* Sanity check: jtable_addr should point to some location within
	 * the module. */
	if (!in_core && !in_init) {
		pr_warning("[sample] Spurious jump table (?) at %p "
			"referred to by jmp at %pS, leaving it as is.\n",
			(void *)jtable_addr,
			insn->kaddr);
		return 0;
	}
	
	/* A rather crude (and probably not always reliable) way to find
	 * the number of elements in the jump table. */
	for (pos = jtable_addr; pos <= end_addr; 
		pos += sizeof(unsigned long)) {
		unsigned long jaddr = *(unsigned long *)pos;
		if (jaddr < func_start || jaddr >= func_end)
			break;
	}
	
	num_elems = (unsigned int)(
		(pos - jtable_addr) / sizeof(unsigned long));
	
	/* Near indirect jumps may only jump to the beginning of a block, 
	 * so we need to add the contents of the jump table to the array
	 * of block boundaries. */
	ret = block_offsets_reserve(ddbb, num_elems);
	if (ret != 0)
		return ret;
	
	for (i = 0; i < num_elems; ++i) {
		unsigned long jaddr = *((unsigned long *)jtable_addr + i);
		ddbb->block_offsets[ddbb->num++] = 
			(u32)(jaddr - func_start);
	}
	
	/* Store the information about this jump table in 'func'. It may be
	 * needed during instrumentation to properly fixup the contents of
	 * the table. */
	jtable = (struct kedr_jump_table *)kzalloc(
		sizeof(struct kedr_jump_table), GFP_KERNEL);
	if (jtable == NULL)
		return -ENOMEM;
	
	jtable->addr = (unsigned long *)jtable_addr;
	jtable->num  = num_elems;
	resolve_jtables_overlaps(jtable, func);
	list_add(&jtable->list, &func->jump_tables);
	
	//<>
	pr_info("[DBG] Found jump table with %u entries at %p "
		"referred to by a jmp at %pS\n",
		jtable->num,
		(void *)jtable->addr, 
		(void *)insn->kaddr);
	//<>
	return 0;
}

static int
detect_block_bounds(struct insn *insn, void *data)
{
	unsigned long dest;
	unsigned long start_addr;
	unsigned long offset_after_insn;
	struct kedr_tmod_function *func;
	int ret = 0;
	struct kedr_data_detect_block_bounds *ddbb = 
		(struct kedr_data_detect_block_bounds *)data;
	BUG_ON(ddbb == NULL);
	
	func = ddbb->func;
	start_addr = (unsigned long)func->addr;
	offset_after_insn = (unsigned long)insn->kaddr + 
		(unsigned long)insn->length - start_addr;
		
	/* If we've got too far, probably there is a bug in our system. It 
	 * is impossible for an instruction to be located at 64M distance
	 * or further from the beginning of the corresponding function. */
	WARN_ON(offset_after_insn >= 0x04000000UL);
	
	/* If we have skipped too many zeros at the end of the function, 
	 * that is, if he cut off a part of the last instruction, fix it 
	 * now. */
	if (offset_after_insn > func->size)
		func->size = offset_after_insn;
	
	dest = insn_jumps_to(insn);
	if (dest == 0) /* no jumps - just go on */
		return 0;
	
	/* Control transfer outside of the function; indirect near jumps */
	if (dest < start_addr || dest >= start_addr + func->size) {
	    	u32 offset;
	    	u8 opcode = insn->opcode.bytes[0];
	    	
	    	ret = block_offsets_reserve(ddbb, 2);
	    	if (ret != 0)
			return ret;
		
		offset = (u32)((unsigned long)insn->kaddr - start_addr);
		ddbb->block_offsets[ddbb->num++] = offset;
		ddbb->block_offsets[ddbb->num++] = offset + 
			(u32)insn->length;
		
		/* Some indirect near jumps need additional processing,
		 * namely those that have the following form: 
		 * jmp near [<jump_table> + reg * <scale>]. 
		 * [NB] We don't need to do anything about other kinds of 
		 * indirect jumps, like jmp near [reg]. 
		 * 
		 * jmp near indirect has code FF/4. 'mod' and 'R/M' fields
		 * are used here to determine if SIB byte is present. */
		if (opcode == 0xff && 
			X86_MODRM_REG(insn->modrm.value) == 4 && 
			X86_MODRM_MOD(insn->modrm.value) != 3 &&
			X86_MODRM_RM(insn->modrm.value) == 4)
			return handle_jmp_near_indirect(insn, ddbb);
		
		return 0;
	}
	
	/* A jump backwards is a separate block. The jump target must also
	 * be a start of some other block. */
	if (dest < (unsigned long)insn->kaddr) {
		u32 offset;
		ret = block_offsets_reserve(ddbb, 3);
	    	if (ret != 0)
			return ret;
		
		ddbb->block_offsets[ddbb->num++] = (u32)(dest - start_addr);
		offset = (u32)((unsigned long)insn->kaddr - start_addr);
		ddbb->block_offsets[ddbb->num++] = offset;
		ddbb->block_offsets[ddbb->num++] = offset + 
			(u32)insn->length;
	}
	
	/* Other instructions need not be placed in separate blocks. */
	return 0;	
}
/* ====================================================================== */

static int
skip_trailing_zeros(struct kedr_tmod_function *func)
{
	/* Skip trailing zeros. If these are a part of an instruction,
	 * it will be handled automatically. If it just a padding sequence,
	 * we will avoid reading past the end of the function.
	 * It is unlikely, anyway, that a function ends with something like
	 * 'add %al, %(eax)', that is, 0x0000. */
	while (func->size != 0 && 
		*(u8 *)((unsigned long)func->addr + func->size - 1) == '\0')
		--func->size;
	
	if (func->size == 0) { /* Very unlikely. Broken module? */
		pr_err("[sample] "
"A spurious symbol \"%s\" (address: %p) seems to contain only zeros\n",
			func->name,
			func->addr);
		return -EILSEQ;
	}
	return 0;
}

static int 
offset_compare(const void *lhs, const void *rhs)
{
	u32 left  = *(const u32 *)lhs;
	u32 right = *(const u32 *)rhs;
	
	if (left == right)
		return 0;
	else if (left < right)
		return -1;
	else 
		return 1;
}

static void 
offset_swap(void *lhs, void *rhs, int size)
{
	u32 *left  = (u32 *)lhs;
	u32 *right = (u32 *)rhs;
	u32 t;
		
	t = *right;
	*right = *left;
	*left = t;
}

/* Release the memory occupied by kedr_code_block instances and other 
 * structures (necessary only for instrumentation of the function 'func')
 * created by prepare_blocks(). */
static void
cleanup_blocks(struct kedr_tmod_function *func)
{
	struct kedr_code_block *block;
	struct kedr_code_block *btmp;
	
	struct kedr_jump_table *jtable;
	struct kedr_jump_table *jtmp;
	
	list_for_each_entry_safe(block, btmp, &func->blocks, list) {
		list_del(&block->list);
		kfree(block);
	}

	list_for_each_entry_safe(jtable, jtmp, &func->jump_tables, list) {
		list_del(&jtable->list);
		//<>
		pr_info("[DBG] Deleting the info about jump table "
			"of %u entries at %p\n",
			jtable->num, (void *)jtable->addr);
		//<>
		kfree(jtable);
	}
}

/* Split the function into code blocks (see the description of struct
 * kedr_code_block) and populate func->blocks and func->jump_tables lists.
 *
 * A side effect: adjusts func->size as to skip trailing zeros. */
static int
prepare_blocks(struct kedr_tmod_function *func, struct module *mod)
{
	int ret = 0;
	unsigned int i;
	u32 offset_start;
	u32 offset_end;
	struct kedr_data_detect_block_bounds ddbb;
//<>
	unsigned int max_block_size = 0;
	
	debug_util_print_string("Function ");
	debug_util_print_string(func->name);
	debug_util_print_string("()\n");
//<>
	
	/* If the last instruction in the function (e.g. a jump) ends with
	 * one or more zeros, detect_block_bounds() will later adjust 
	 * func->size. So we can just skip all trailing zeros here. */
	ret = skip_trailing_zeros(func);
	if (ret != 0)
		return ret;
	
	ddbb.func = func;
	ddbb.mod = mod;
	ddbb.num = 0;
	ddbb.capacity = KEDR_BASE_ARRAY_SIZE;
	ddbb.block_offsets = 
		(u32 *)kzalloc(KEDR_BASE_ARRAY_SIZE * sizeof(u32), 
		GFP_KERNEL);
	if (ddbb.block_offsets == NULL) {
		pr_err("[sample] prepare_blocks(): "
			"Not enough memory for block_offsets[].");
		return -ENOMEM;
	}
	
	ret = do_for_each_insn((unsigned long)func->addr, 
		(unsigned long)func->addr + func->size, 
		detect_block_bounds,
		(void *)&ddbb);
	if (ret != 0)
		goto fail;
		
	/* Add the end offset (func->size) to the array of offsets too. */
	ret = block_offsets_reserve(&ddbb, 1);
	if (ret != 0)
		goto fail;
	
	ddbb.block_offsets[ddbb.num++] = (u32)func->size;
	
	/* Sort the array; then create blocks for non-duplicate offsets. */
	sort(ddbb.block_offsets, (size_t)ddbb.num, sizeof(u32), 
		offset_compare, offset_swap);
	WARN_ON(ddbb.block_offsets[ddbb.num - 1] != (u32)func->size);
	
	//<>
	/*if (ddbb.block_offsets[ddbb.num - 1] != (u32)func->size) {
		debug_util_print_string("Offsets: \n");
		for (i = 0; i < ddbb.num; ++i) {
			debug_util_print_u64((u64)ddbb.block_offsets[i], 
				"%llx\n");
		}
	}*/
	//<>

	i = 0;
	while (ddbb.block_offsets[i] == 0) {
		++i;
		if (i >= ddbb.num) {
			/* All offsets are 0? There is a bug in the logic 
			 * somewhere in our system if we get here. */
			WARN_ON(1);
			ret = -EFAULT;
			goto fail;
		}
	}
	
	offset_start = 0;
	while (i < ddbb.num) {
		struct kedr_code_block *block;
		offset_end = ddbb.block_offsets[i];
		
		block = (struct kedr_code_block *)kzalloc(
			sizeof(struct kedr_code_block), GFP_KERNEL);
		if (block == NULL) {
			ret = -ENOMEM;
			goto fail;
		}
		block->addr = (void *)((unsigned long)func->addr + 
			offset_start);
		block->size = (unsigned int)(offset_end - offset_start);
		list_add(&block->list, &func->blocks);
		
		//<>
		if (max_block_size < block->size) {
			max_block_size = block->size;
		}
		/*debug_util_print_u64((u64)(unsigned long)block->addr, 
			"[0x%llx; ");
		debug_util_print_u64((u64)((unsigned long)block->addr + 
			block->size), "0x%llx)\n");*/
		//<>
		
		offset_start = offset_end;
		while (i < ddbb.num && ddbb.block_offsets[i] == offset_end)
			++i;
	}

	//<>
	debug_util_print_u64((u64)max_block_size, 
		"Max block size: 0x%llx\n");
	//<>

	kfree(ddbb.block_offsets);
	return 0;
	
fail:
	kfree(ddbb.block_offsets);
	
	/* Call cleanup_blocks() just in case some blocks and/or jump tables 
	 * have been prepared before an error occured.*/
	cleanup_blocks(func); 
	return ret;
}
/* ====================================================================== */

static int
debug_print_insn(struct insn *insn, void *data)
{
	debug_util_print_hex_bytes(insn->kaddr, (unsigned int)insn->length);
	debug_util_print_string("\n");
	return 0;
}

/* Print the original and the instrumented code (hex bytes) of the function
 * (instruction-by-instruction) to the debug output file. */
static void
debug_print_func_code(struct kedr_tmod_function *func)
{
	unsigned long start_addr;
	unsigned long end_addr;

	debug_util_print_string("Original function: ");
	debug_util_print_string(func->name);
	debug_util_print_string(", ");
	debug_util_print_u64((u64)(unsigned long)func->addr, 
		"address: 0x%llx\n");
	
	start_addr = (unsigned long)func->addr;
	end_addr  = start_addr + func->size;
	do_for_each_insn(start_addr, end_addr, debug_print_insn, NULL);
			
	debug_util_print_string("\n");
	debug_util_print_string("Instrumented function, ");
	debug_util_print_u64((u64)(unsigned long)func->instrumented_addr, 
		"address: 0x%llx\n");
	
	start_addr = (unsigned long)func->instrumented_addr;
	end_addr = start_addr + func->instrumented_size;
	do_for_each_insn(start_addr, end_addr, debug_print_insn, NULL);
}

/* Create an instrumented variant of function specified by 'func'. 
 * The function returns 0 if successful, an error code otherwise. 
 * 
 * The function also adjusts 'func->instrumented_size' if necessary (it is 
 * an estimate on entry). */
static int
instrument_function(struct kedr_tmod_function *func, struct module *mod)
{
	unsigned long orig_addr;
	unsigned long dest_addr;
//	u32 *poffset;
	struct kedr_code_block *block;
	int ret = 0;
	
	BUG_ON(func == NULL || func->addr == NULL);
	BUG_ON(	func->instrumented_addr == NULL || 
		func->instrumented_size == 0);
	
	/* If the function is too short (shorter than a single 'jmp rel32' 
	 * instruction), do not instrument it. */
	if (func->size < KEDR_REL_JMP_SIZE)
		return 0;
		
	orig_addr = (unsigned long)func->addr;
	dest_addr = (unsigned long)func->instrumented_addr;

	ret = prepare_blocks(func, mod);
	if (ret != 0)
		return ret;
		
	list_for_each_entry(block, &func->blocks, list) {
		// TODO: process block-by-block
		// TODO: increase 'dest' as needed
		
		// if error - goto instrum_failed
	}
	cleanup_blocks(func);
	
	/* Adjust the length of the instrumented function */
	//func->instrumented_size = dest_addr - 
	//	(unsigned long)func->instrumented_addr;
	
	//<>
	if (0 == strcmp(func->name, target_function))
		debug_print_func_code(func);
	//<>
	
	// TODO: uncomment when instrumentation code is prepared
	
	/* Save the bytes to be overwritten by the jump instruction and
	 * place the jump to the instrumented function at the beginning 
	 * of the original function. */
	//memcpy(&func->orig_start_bytes[0], func->addr, KEDR_REL_JMP_SIZE);
	
	/* We allocate memory for the detour buffer in a special way, so 
	 * that it is "not very far" from where the code of the target 
	 * module resides. A near relative jump is enough in this case. */
	//*(u8 *)func->addr = KEDR_OP_JMP_REL32;
	//poffset = (u32 *)((unsigned long)func->addr + 1);
	//*poffset = KEDR_OFFSET_FROM_ADDR((unsigned long)func->addr, 
	//	KEDR_REL_JMP_SIZE, (unsigned long)func->instrumented_addr);
	
	return 0; /* success */

/*
instrum_failed:
	cleanup_blocks(func);
	return ret;*/
}

/* ====================================================================== */
int
kedr_process_target(struct module *mod)
{
	struct kedr_tmod_function *f;
	int ret = 0;
	
	BUG_ON(mod == NULL);
	
	ret = kedr_load_function_list(mod);
	if (ret != 0)
		return ret;
	
	ret = prepare_funcs_for_detour();
	if (ret != 0)
		return ret;
	
	list_for_each_entry(f, &tmod_funcs, list) {
/*		//<>
		pr_info("[sample] "
"module: \"%s\", processing function \"%s\" (address is %p, size is %lu)\n",
		module_name(mod),
		f->name,
		f->addr,
		f->size);
		//<>
*/	
		ret = instrument_function(f, mod);
		if (ret != 0)
			return ret;
	}
	return 0;
}
/* ====================================================================== */
