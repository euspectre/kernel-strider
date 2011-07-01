/*
 * functions.c: main operations with the functions in the target module:
 * enumeration, instrumentation, etc.
 */

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
/* Maximum size of a machine instruction on x86, in bytes. Actually, 15
 * would be enough. From Intel Software Developer's Manual Vol2A, section 
 * 2.2.1: "The instruction-size limit of 15 bytes still applies <...>".
 * We just follow the implementation of kernel probes in this case. */
/*#define KEDR_MAX_INSN_SIZE 16*/

/* Some opcodes */
#define KEDR_OP_JMP_REL32	0xe9
#define KEDR_OP_CALL_REL32	0xe8

/* CODE_ADDR_FROM_OFFSET()
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
# define CODE_ADDR_FROM_OFFSET(insn_addr, insn_len, offset) \
	(void*)((s64)(insn_addr) + (s64)(insn_len) + (s64)(s32)(offset))

#else /* CONFIG_X86_32 */
# define CODE_ADDR_FROM_OFFSET(insn_addr, insn_len, offset) \
	(void*)((u32)(insn_addr) + (u32)(insn_len) + (u32)(offset))
#endif

/* CODE_OFFSET_FROM_ADDR()
 * 
 * The reverse of CODE_ADDR_FROM_OFFSET: calculates the offset value
 * to be used in an instruction given the address and length of the
 * instruction and the destination address it must refer to. */
#define CODE_OFFSET_FROM_ADDR(insn_addr, insn_len, dest_addr) \
	(u32)(dest_addr - (insn_addr + (u32)insn_len))

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

/* Remove from the list and destroy the elements with zero text_size. 
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
		if (pos->text_size == 0) {
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
	WARN_ON(func->text_size == 0); 
	
	// TODO
	// For now, just return the size of the original function. In a real
	// system we need to estimate the size of the instrumented function
	// instead.
	return func->text_size;
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
			printk(KERN_ERR "[sample] "
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
		printk(KERN_ERR "[sample] "
			"Failed to allocate detour buffer of size %lu\n",
			db_size);
		return -ENOMEM;
	}
	memset(dbuf, 0, (size_t)db_size);
	
	set_instrumented_addrs();
	
	//<>
	{
	//	struct kedr_tmod_function *pos;
		printk(KERN_INFO "[sample] "
			"Allocated detour buffer of size %lu at 0x%p\n",
			db_size, dbuf);
			
	/*	list_for_each_entry(pos, &tmod_funcs, list) {
			printk(KERN_INFO "[sample] "
	"function \"%s\": address is %p, size is %lu, " 
	"'detoured' address is %p, size is %lu.\n",
				pos->name,
				pos->addr, pos->text_size,
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
	
	tf->addr = (void *)addr; /* [NB] tf->text_size is 0 now*/
	tf->name = name;
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
		printk(KERN_INFO "[sample] "
			"No functions found in \"%s\", nothing to do\n",
			module_name(target_module));
		return 0;
	} 
	
	printk(KERN_INFO "[sample] "
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
		pfuncs[i]->text_size = 
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
		addr = (unsigned long)CODE_ADDR_FROM_OFFSET(
			src_insn->kaddr,
			src_insn->length, 
			src_insn->immediate.value);
		
		if (addr >= (unsigned long)func->addr && 
		    addr < (unsigned long)func->addr + func->text_size)
			return; /* no fixup necessary */
		
		/* Call or jump outside of the function, fix it up. */
		to_fixup = (u32 *)((unsigned long)dest + 
			insn_offset_immediate(src_insn));
		*to_fixup = CODE_OFFSET_FROM_ADDR(dest, src_insn->length,
			(void *)addr);
		return;
	}

#ifdef CONFIG_X86_64
	if (!insn_rip_relative(src_insn))
		return;
		
	/* Handle RIP-relative addressing */
	addr = (unsigned long)CODE_ADDR_FROM_OFFSET(
		src_insn->kaddr,
		src_insn->length, 
		src_insn->displacement.value);
	
	/* Check if the instruction addresses something inside this 
	 * function. If so, no fixup is necessary. */
	if (addr >= (unsigned long)func->addr && 
	    addr < (unsigned long)func->addr + func->text_size)
		return;
	
	to_fixup = (u32 *)((unsigned long)dest + 
		insn_offset_displacement(src_insn));
	*to_fixup = CODE_OFFSET_FROM_ADDR(dest, src_insn->length,
		(void *)addr);
#endif
	return;
}

/* Create an instrumented variant of function specified by 'func'. 
 * The function returns 0 if successful, an error code otherwise. 
 * 
 * The function also adjusts 'func->instrumented_size' if necessary (it is 
 * an estimate on entry). */
static int
instrument_function(struct kedr_tmod_function *func)
{
	unsigned long orig_addr;
	unsigned long dest_addr;
	unsigned long end_addr;
	u32 *poffset;
	struct insn insn;
	
	BUG_ON(func == NULL || func->addr == NULL);
	BUG_ON(	func->instrumented_addr == NULL || 
		func->instrumented_size == 0);
	
	/* If the function is too short (shorter than a single 'jmp rel32' 
	 * instruction), do not instrument it. */
	if (func->text_size < KEDR_REL_JMP_SIZE)
		return 0;
	
	orig_addr = (unsigned long)func->addr;
	dest_addr = (unsigned long)func->instrumented_addr;
	
	/* Process instructions one by one, fixing them up if necessary. */
	
	/* Skip trailing zeros first. If these are a part of an instruction,
	 * it will be handled automatically. If it just a padding sequence,
	 * we will avoid reading past the end of the function.
	 * It is unlikely that a function ends with something like
	 *  'add %al, %(eax)', i.e. 0x0000, anyway. */
	end_addr = orig_addr + func->text_size;
	while (end_addr > orig_addr && *(u8 *)(end_addr - 1) == '\0')
		--end_addr;
	
	if (orig_addr == end_addr) { /* Very unlikely. Broken module? */
		printk(KERN_ERR "[sample] "
"A spurious symbol \"%s\" (address: %p) seems to contain only zeros\n",
			func->name,
			func->addr);
		return -EILSEQ;
	}
	
	while (orig_addr < end_addr) {
		kernel_insn_init(&insn, (void *)orig_addr);
		insn_get_length(&insn); /* Decode the instruction */
		if (insn.length == 0) {
			printk(KERN_ERR "[sample] "
		"Failed to decode instruction at %p (%s+0x%lx)\n",
				(const void *)orig_addr,
				func->name,
				orig_addr - (unsigned long)func->addr);
			return -EILSEQ;
		}
		
		copy_and_fixup_insn(&insn, (void *)dest_addr, func);
		
		orig_addr += insn.length;
		dest_addr += insn.length;
	}
	
	/* Adjust the length of the instrumented function */
	func->instrumented_size = dest_addr - 
		(unsigned long)func->instrumented_addr;
	
	// For debugging: output the code of the original function
	// and of the copied one instruction-by-instruction.
	//<>
	/*orig_addr = (unsigned long)func->addr;
	dest_addr = (unsigned long)func->instrumented_addr;
	
	debug_util_print_string("Original function: ");
	debug_util_print_string(func->name);
	debug_util_print_string(", ");
	debug_util_print_u64((u64)func->addr, "address: 0x%llx\n");
	
	while (orig_addr < end_addr) {
		kernel_insn_init(&insn, (void *)orig_addr);
		insn_get_length(&insn); 
		debug_util_print_hex_bytes(insn.kaddr, 
			(unsigned int)insn.length);
		debug_util_print_string("\n");
		orig_addr += insn.length;
	}
	
	debug_util_print_string("\n");
	debug_util_print_string("Instrumented function, ");
	debug_util_print_u64((u64)func->instrumented_addr, 
		"address: 0x%llx\n");
	
	end_addr = dest_addr + func->instrumented_size;
	while (dest_addr < end_addr) {
		kernel_insn_init(&insn, (void *)dest_addr);
		insn_get_length(&insn); 
		debug_util_print_hex_bytes(insn.kaddr, 
			(unsigned int)insn.length);
		debug_util_print_string("\n");
		dest_addr += insn.length;
	}*/
	//<>
	
	/* Save the bytes to be overwritten by the jump instruction and
	 * place the jump to the instrumented function at the beginning 
	 * of the original function. */
	memcpy(&func->orig_start_bytes[0], func->addr, KEDR_REL_JMP_SIZE);
	
	/* We allocate memory for the detour buffer in a special way, so 
	 * that it is "not very far" from where the code of the target 
	 * module resides. A near relative jump is enough in this case. */
	*(u8 *)func->addr = KEDR_OP_JMP_REL32;
	poffset = (u32 *)((unsigned long)func->addr + 1);
	*poffset = CODE_OFFSET_FROM_ADDR((unsigned long)func->addr, 
		KEDR_REL_JMP_SIZE, (unsigned long)func->instrumented_addr);
	
	return 0; /* success */
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
		//<>
		printk(KERN_INFO "[sample] "
"module: \"%s\", processing function \"%s\" (address is %p, size is %lu)\n",
		module_name(mod),
		f->name,
		f->addr,
		f->text_size);
		//<>
	
		ret = instrument_function(f);
		if (ret != 0)
			return ret;
	}
	
	// TODO: more processing
	return 0;
}
/* ====================================================================== */
