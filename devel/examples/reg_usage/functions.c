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
#include <linux/hardirq.h>  /* in_interrupt() */
#include <linux/smp.h>      /* smp_processor_id() */
#include <linux/sched.h>    /* current, etc. */

/* [NB] "kedr/" is here to make sure the build system does not pull the 
 * "insn.h" file provided by the kernel. */
#include <kedr/asm/insn.h> /* instruction decoder machinery */

#include "functions.h"
#include "debug_util.h"
#include "detour_buffer.h"
/* ====================================================================== */

extern char* target_function; /* name of the function to debug */
/* ====================================================================== */

/* ====================================================================== */
/* Some opcodes. */
#define KEDR_OP_JMP_REL32	0xe9
#define KEDR_OP_CALL_REL32	0xe8

/* Size of 'call near rel 32' instruction, in bytes. */
#define KEDR_SIZE_CALL_REL32	5

/* entry_call_size - 
 * the size in bytes of the instruction sequence that performs a call on 
 * entry to a function (%Xax is %eax on x86-32 and %rax on x86-64). 
 *
 * 	push   %Xax
 * 	mov    <some_32-bit_value>, %Xax // with sign extension on x86-64
 * 	call   kedr_ps_get_wrapper
 * 	pop    %Xax
 *
 * entry_call_pattern -
 * the instructions (machine code) that perform the above operations. 
 * Placeholders for <some_32-bit_value> and the displacement of 
 * kedr_ps_get_wrapper are left in the pattern. 
 * 
 * entry_call_pos_val and entry_call_pos_func - positions in the pattern
 * where the 32-bit value and the 32-bit displacement of kedr_ps_get_wrapper
 * function should be placed. */
#ifdef CONFIG_X86_64
static u8 entry_call_pattern[] = {
	0x50, 				/* push  %rax */
	0x48, 0xc7, 0xc0, 0x00, 0x00, 0x00, 0x00,	
					/* mov   <some_32-bit_value>,%rax */
	0xe8, 0x00, 0x00, 0x00, 0x00, 	/* call  <something> */
	0x58};				/* pop   %rax */
static u8 entry_call_pos_val = 4; 
static u8 entry_call_pos_func = 9; 

#else /* CONFIG_X86_32 */
static u8 entry_call_pattern[] = {
	0x50, 				/* push  %eax */
	0xb8, 0x00, 0x00, 0x00, 0x00,	/* mov   <some_32-bit_value>,%eax */
	0xe8, 0x00, 0x00, 0x00, 0x00, 	/* call  <something> */
	0x58};				/* pop   %eax */
static u8 entry_call_pos_val = 2; 
static u8 entry_call_pos_func = 7; 
#endif /* #ifdef CONFIG_X86_64 */

static unsigned int entry_call_size = ARRAY_SIZE(entry_call_pattern);

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
/* For each instruction in [start_addr; end_addr), the function decodes it
 * and calls proc() callback for it. 'data' is passed to proc() as the 
 * last argument, it can be a pointer to the custom data needed by the 
 * particular callback. 
 *
 * [NB] The address of the instruction can be obtained in proc() via
 * insn->kaddr field.
 *
 * proc() is expected to return 0 on success and a negative error code on 
 * failure. for_each_insn() continues as long as there are instructions 
 * left and proc() returns 0. If proc() returns nonzero, for_each_insn()
 * stops and returns this value.
 * 
 * Use this function instead of explicit walking, decoding and processing 
 * the areas of code (you remember C++ and STL best practices, right?). */
static int 
for_each_insn(unsigned long start_addr, unsigned long end_addr,
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

/* for_each_insn_in_function() - similar to for_each_insn() but operates 
 * only on the given function 'func' (on its original code). 
 * 
 * Note that 'proc' callback must have a different prototype here:
 *   int <name>(struct kedr_tmod_function *, struct insn *, void *)
 * That is, it will also get access to 'func' without the need for any
 * special wrapper structures (for_each_insn_in_function() handles wrapping
 * stuff itself). */

struct data_for_each_insn_in_function
{
	struct kedr_tmod_function *func;
	void *data;
	int (*proc)(struct kedr_tmod_function *, struct insn *, void *);
};

static int
proc_for_each_insn_in_function(struct insn *insn, void *data)
{
	struct data_for_each_insn_in_function *data_container = 
		(struct data_for_each_insn_in_function *)data;
	
	return data_container->proc(
		data_container->func, 
		insn, 
		data_container->data);
}

static int
for_each_insn_in_function(struct kedr_tmod_function *func, 
	int (*proc)(struct kedr_tmod_function *, struct insn *, void *), 
	void *data)
{
	unsigned long start_addr = (unsigned long)func->addr;
	struct data_for_each_insn_in_function data_container;
	
	data_container.func = func;
	data_container.data = data;
	data_container.proc = proc;
	
	return for_each_insn(start_addr, 
		start_addr + func->size, 
		proc_for_each_insn_in_function,
		&data_container);
}

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
/* Estimate the size of the instrumented instruction and add it to
 * *(unsigned long *)data. Returns 0. 
 * Note that short jumps leading outside of the current function are 
 * expected to be converted to near jumps or something like that. 
 * It is also assumed that we ignore the prefixes for short jumps, except 
 * for branch prediction hints, they are probably of little use anyway. */
static int
add_insn_size(struct kedr_tmod_function *func, struct insn *insn, 
	void *data)
{
	unsigned long *size = (unsigned long *)data;
	u8 opcode = insn->opcode.bytes[0];
	s32 offset;
	unsigned long dest_addr;
	unsigned long start_addr = (unsigned long)func->addr;
	unsigned long end_addr = (unsigned long)func->addr + func->size;
	
	if (opcode >= 0x70 && opcode <= 0x7f)  { /* jcc short */
		offset = (s32)(s8)insn->immediate.bytes[0];
		dest_addr = (unsigned long)X86_ADDR_FROM_OFFSET(
			insn->kaddr, insn->length, offset); 
		
		if (dest_addr < start_addr || dest_addr >= end_addr) {
			*size += 6; /* jcc short => jcc near */
		}
		else {
			*size += (unsigned long)insn->length;
		}
	} 
	else if (opcode == 0xeb) { /* jmp short */
		offset = (s32)(s8)insn->immediate.bytes[0];
		dest_addr = (unsigned long)X86_ADDR_FROM_OFFSET(
			insn->kaddr, insn->length, offset); 
		
		if (dest_addr < start_addr || dest_addr >= end_addr) {
			*size += 5; /* jmp short => jmp near */
		}
		else {
			*size += (unsigned long)insn->length;
		}
	} 
	else if (opcode == 0xe3) { /* j*cxz */
		offset = (s32)(s8)insn->immediate.bytes[0];
		dest_addr = (unsigned long)X86_ADDR_FROM_OFFSET(
			insn->kaddr, insn->length, offset); 
		
		if (dest_addr < start_addr || dest_addr >= end_addr) {
			*size += 9; /* j*cxz => jmp near ++ */
	/* j*cxz 02 - 2 bytes
	 * jmp short 05  - 2 bytes
	 * jmp near rel32 (=> where j*cxz would have jumped) - 5 bytes
	 * ... <the code that was after j*cxz is now here> 
	 * So the length is 9 bytes total. */
		}
		else {
			*size += (unsigned long)insn->length;
		}
	}
	else { /* not a short jump or a jump that can not lead outside */
		*size += (unsigned long)insn->length;
	}
	return 0;
}

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
	unsigned long size = 0;
	BUG_ON(func == NULL || func->addr == NULL);
	
	/* Should not happen because we should have skipped aliases at the 
	 * upper level. Just a bit of extra self-control. */
	WARN_ON(func->size == 0); 
	
	// TODO: rewrite according to what the size of the instrumented 
	// function will be.
	
	//start_addr = (unsigned long)func->addr;
	//start_addr + func->size;
	
	if (for_each_insn_in_function(func, add_insn_size, &size) != 0)
		return 0;
	
	return size + entry_call_size;
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
/* Similar to insn_register_usage_mask() but also takes function calls into
 * account. If 'insn' transfers control outside of the function
 * 'func', the register_usage_mask() considers all the scratch general
 * purpose registers used and updates the mask accordingly. 
 * 
 * It is possible that the instruction does not actually use this many
 * registers. For now, we take a safer, simpler but less optimal route 
 * in such cases. */
static unsigned int 
register_usage_mask(struct insn *insn, struct kedr_tmod_function *func)
{
	unsigned int reg_mask;
	unsigned long dest;
	unsigned long start_addr = (unsigned long)func->addr;
	u8 opcode;
	
	BUG_ON(insn == NULL);
	BUG_ON(func == NULL);
	
	/* Decode at least the opcode because we need to handle some 
	 * instructions separately ('ret' group). */
	insn_get_opcode(insn);
	opcode = insn->opcode.bytes[0];
	
	/* Handle 'ret' group to avoid marking scratch registers used for 
	 * these instructions. */
	if (opcode == 0xc3 || opcode == 0xc2 || 
	    opcode == 0xca || opcode == 0xcb)
		return X86_REG_MASK(INAT_REG_CODE_SP);
	
	reg_mask = insn_register_usage_mask(insn);
	dest = insn_jumps_to(insn);
	
	if (dest != 0 && 
	    (dest < start_addr || dest >= start_addr + func->size))
	    	reg_mask |= X86_REG_MASK_SCRATCH;
		
	return reg_mask;
}

/* ====================================================================== */
int
kedr_init_function_subsystem(void)
{
	num_funcs = 0;
	
	//<>
//	{
//		static unsigned char insn_buffer[16] = {
//			/* ds:lea 0x00(%rsi,%riz,1), %rsi */
//			/*0x3e, 0x48, 0x8d, 0x74, 0x26, 0x00 */
//			
//			/*0x0f, 0xa2*/ /* cpuid */
//			0x0f, 0xb0, 0x0a /* cmpxchg %cl, (%rdx) */
//			/*0x83, 0x00, 0x00*/ /*addl $0x0, (%rax)*/
//			/*0x83, 0x38, 0x00*/ /*cmpl $0x0, (%rax)*/
//		};
//		struct insn insn;
//		
//		kernel_insn_init(&insn, (void *)&insn_buffer[0]);
//		pr_info("[DBG] reg usage mask: %08x\n",
//			insn_register_usage_mask(&insn));
//		pr_info("[DBG] op1: {am=%d, ot=%d}; op2: {am=%d, ot=%d}",
//			insn.attr.addr_method1,
//			insn.attr.opnd_type1,
//			insn.attr.addr_method2,
//			insn.attr.opnd_type2
//		);
//		
//		pr_info("[DBG] reads from memory: %d, writes to memory: %d",
//			insn_is_mem_read(&insn),
//			insn_is_mem_write(&insn));
//	}
	//<>
	
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
		addr = (unsigned long)X86_ADDR_FROM_OFFSET(
			src_insn->kaddr,
			src_insn->length, 
			src_insn->immediate.value);
		
		if (addr >= (unsigned long)func->addr && 
		    addr < (unsigned long)func->addr + func->size)
			return; /* no fixup necessary */
		
		/* Call or jump outside of the function, fix it up. */
		to_fixup = (u32 *)((unsigned long)dest + 
			insn_offset_immediate(src_insn));
		*to_fixup = X86_OFFSET_FROM_ADDR(dest, src_insn->length,
			(void *)addr);
		return;
	}

#ifdef CONFIG_X86_64
	if (!insn_rip_relative(src_insn))
		return;
		
	/* Handle RIP-relative addressing */
	addr = (unsigned long)X86_ADDR_FROM_OFFSET(
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
	*to_fixup = X86_OFFSET_FROM_ADDR(dest, src_insn->length,
		(void *)addr);
#endif
	return;
}

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

/*static int 
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
*/

/*static int
debug_print_insn(struct insn *insn, void *data)
{
	debug_util_print_hex_bytes(insn->kaddr, (unsigned int)insn->length);
	debug_util_print_string("\n");
	return 0;
}*/

/* Print the original and the instrumented code (hex bytes) of the function
 * (instruction-by-instruction) to the debug output file. */
/*static void
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
	for_each_insn(start_addr, end_addr, debug_print_insn, NULL);
			
	debug_util_print_string("\n");
	debug_util_print_string("Instrumented function, ");
	debug_util_print_u64((u64)(unsigned long)func->instrumented_addr, 
		"address: 0x%llx\n");
	
	start_addr = (unsigned long)func->instrumented_addr;
	end_addr = start_addr + func->instrumented_size;
	for_each_insn(start_addr, end_addr, debug_print_insn, NULL);
}*/
/* ====================================================================== */

/* KEDR_SAVE_SCRATCH_REGS_BUT_AX
 * Save scratch registers (except %eax/%rax) and flags on the stack.
 * 
 * KEDR_RESTORE_SCRATCH_REGS_BUT_AX
 * Restore scratch registers (except %eax/%rax) and flags from the stack.
 * 
 * The above macros can be used when injecting a function call into the 
 * code. The function to be called is a C function, so, according to x86 
 * ABI, it is responsible for preserving the values of the non-scratch 
 * registers. %eax/%rax should be saved and restored separately by the 
 * caller of the wrapper code. This register is used to pass the argument 
 * to the function to be called and to contain its return value.
 */
#ifdef CONFIG_X86_64
# define KEDR_SAVE_SCRATCH_REGS_BUT_AX \
	"pushfq\n\t"		\
	"pushq %rcx\n\t"	\
	"pushq %rdx\n\t"	\
	"pushq %rsi\n\t"	\
	"pushq %rdi\n\t"	\
	"pushq %r8\n\t"	\
	"pushq %r9\n\t"	\
	"pushq %r10\n\t"	\
	"pushq %r11\n\t"

# define KEDR_RESTORE_SCRATCH_REGS_BUT_AX \
	"popq %r11\n\t"	\
	"popq %r10\n\t"	\
	"popq %r9\n\t"		\
	"popq %r8\n\t"		\
	"popq %rdi\n\t"	\
	"popq %rsi\n\t"	\
	"popq %rdx\n\t"	\
	"popq %rcx\n\t"	\
	"popfq\n\t"
	
#else /* CONFIG_X86_32 */
# define KEDR_SAVE_SCRATCH_REGS_BUT_AX \
	"pushf\n\t"		\
	"pushl %ecx\n\t"	\
	"pushl %edx\n\t"

# define KEDR_RESTORE_SCRATCH_REGS_BUT_AX \
	"popl %edx\n\t"		\
	"popl %ecx\n\t"		\
	"popf\n\t"

#endif /* #ifdef CONFIG_X86_64 */

/* The "holder-wrapper" technique is inspired by the implementation of 
 * KProbes (kretprobe, actually) on x86. 
 *
 * The wrappers below are used to inject the following function calls:
 * - typeA *kedr_get_primary_storage(unsigned long orig_func_id);
 * - void kedr_put_primary_storage(typeA *storage);
 * - void kedr_flush_primary_storage(typeA *storage);
 * 
 * The parameter to be passed to the function is expected to be in 
 * %eax/%rax. The return value of the function will also be stored in this 
 * register.
 *
 * These wrappers allow to reduce code bloat. If it were not for them, we
 * would need to insert the code for saving and restoring registers directly
 * to the instrumented function.
 *
 * [NB] "__used" makes the compiler think the function is used even if it is
 * not clearly visible where and how. */

/* We need to declare the wrappers somewhere although their definitions will
 * be inside the holders. */
void kedr_ps_get_wrapper(void);

static __used void kedr_ps_get_wrapper_holder(void)
{
	asm volatile (
		".global kedr_ps_get_wrapper\n"
		"kedr_ps_get_wrapper: \n\t"
		KEDR_SAVE_SCRATCH_REGS_BUT_AX
#ifdef CONFIG_X86_64
		/* On x86-64, the first parameter of the function is 
		 * expected to be passed in %rdi. On x86-32 with 'regparm'
		 * compiler option used, it is expected to be in %eax. */
		"movq %rax, %rdi\n\t"
#endif
		"call kedr_get_primary_storage\n\t"
		KEDR_RESTORE_SCRATCH_REGS_BUT_AX
		"ret\n");
}

static __used void * 
kedr_get_primary_storage(unsigned long orig_func_addr)
{
	//<>
	static unsigned int call_no = 0;

	/* A race condition is possible here between the threads executing
	 * 'call_no < 256' and '++call_no'. In this case (debug output 
	 * only), it does not make much harm. */
	if (call_no < 256) {
		pr_info("[DBG] [%3u] cpu: %u, func: %p (%pf), "
			"current: %p (%s)\n", 
			call_no,
			smp_processor_id(),
			(void *)orig_func_addr,
			(void *)orig_func_addr,
			(void *)current,
			current->comm);
		++call_no;
	}
	//<>
	
	// TODO
	return /*<>*/ NULL; 
}
/* ====================================================================== */

/* Process 'jmp rel8' and adjust the destination address so that it points 
 * where the next instruction should be placed. 
 * If the jump leads outside of the function, place jmp rel32 (jmp near) 
 * instead of jmp rel8. Otherwise, copy the instriction as is. */
static void
process_jmp_short(struct kedr_tmod_function *func, struct insn *insn,
	unsigned long *pdest_addr)
{
	s32 offset = (s32)(s8)insn->immediate.bytes[0];
	unsigned long start_addr = (unsigned long)func->addr;
	unsigned long end_addr = (unsigned long)func->addr + func->size;
	unsigned long jump_addr;
	
	jump_addr = (unsigned long)X86_ADDR_FROM_OFFSET(
			insn->kaddr, insn->length, offset); 
		
	if (jump_addr < start_addr || jump_addr >= end_addr) {
		/* jmp short => jmp near */
		//<>
		pr_info("[DBG] "
	"Found jmp short at %p (%pS) to another function (%p, %pf)\n",
			(void *)insn->kaddr, (void *)insn->kaddr, 
			(void *)jump_addr, (void *)jump_addr);
		//<>
		*(u8 *)(*pdest_addr) = KEDR_OP_JMP_REL32;
		*(u32 *)(*pdest_addr + 1) = (u32)X86_OFFSET_FROM_ADDR(
			*pdest_addr, KEDR_REL_JMP_SIZE, jump_addr);
		*pdest_addr += KEDR_REL_JMP_SIZE;
	}
	else {
		memcpy((void *)(*pdest_addr), (const void *)insn->kaddr, 
			insn->length);
		*pdest_addr += insn->length;
	}
	return;
}

/* Similar to process_jmp_short() but for conditional jumps (except j*cxz)*/
static void
process_jcc_short(struct kedr_tmod_function *func, struct insn *insn,
	unsigned long *pdest_addr)
{
	s32 offset = (s32)(s8)insn->immediate.bytes[0];
	unsigned long start_addr = (unsigned long)func->addr;
	unsigned long end_addr = (unsigned long)func->addr + func->size;
	unsigned long jump_addr;
	
	jump_addr = (unsigned long)X86_ADDR_FROM_OFFSET(
			insn->kaddr, insn->length, offset); 
		
	if (jump_addr < start_addr || jump_addr >= end_addr) {
		/* jcc short => jcc near */
		//<>
		pr_info("[DBG] "
	"Found jcc short at %p (%pS) to another function (%p, %pf)\n",
			(void *)insn->kaddr, (void *)insn->kaddr, 
			(void *)jump_addr, (void *)jump_addr);
		//<>
		
		/* Here we take advantage of the fact that the opcodes 
		 * for short and near conditional jumps go in the same
		 * order with the last opcode byte being 0x10 greater for 
		 * jcc rel32, e.g.:
		 *   77 (ja rel8) => 0F 87 (ja rel32) 
		 *   78 (js rel8) => 0F 88 (js rel32), etc. */
		*(u8 *)(*pdest_addr) = 0x0F;
		*(u8 *)(*pdest_addr + 1) = (u8)insn->opcode.bytes[0] + 0x10;
		*(u32 *)(*pdest_addr + 2) = (u32)X86_OFFSET_FROM_ADDR(
			*pdest_addr, 6, jump_addr);
		*pdest_addr += 6;
		/* Length of 'jcc rel32' is 6 bytes. */
	}
	else {
		memcpy((void *)(*pdest_addr), (const void *)insn->kaddr, 
			insn->length);
		*pdest_addr += insn->length;
	}
	return;
}

/* Similar to process_jmp_short() but for j*cxz. There is no 'j*cxz near', 
 * so 'j*cxz short' + 'jmp near' are used. */
static void
process_jcxz_short(struct kedr_tmod_function *func, struct insn *insn,
	unsigned long *pdest_addr)
{
	s32 offset = (s32)(s8)insn->immediate.bytes[0];
	unsigned long start_addr = (unsigned long)func->addr;
	unsigned long end_addr = (unsigned long)func->addr + func->size;
	unsigned long jump_addr;
	
	jump_addr = (unsigned long)X86_ADDR_FROM_OFFSET(
			insn->kaddr, insn->length, offset); 
		
	if (jump_addr < start_addr || jump_addr >= end_addr) {
	/* j*cxz => 
	 *     j*cxz 02 (to label_jump, insn length: 2 bytes)
	 *     jmp short 05 (to label_continue, insn length: 2 bytes) 
	 * label_jump:
	 *     jmp near <where j*cxz would jump> (insn length: 5 bytes)
	 * label_continue:
	 *     ...
	 */
		//<>
		pr_info("[DBG] "
	"Found j*cxz at %p (%pS) to another function (%p, %pf)\n",
			(void *)insn->kaddr, (void *)insn->kaddr, 
			(void *)jump_addr, (void *)jump_addr);
		//<>
		
		/* j*cxz 02 */
		*(u8 *)(*pdest_addr) = 0xE3;
		*(u8 *)(*pdest_addr + 1) = 0x02;
		
		/* jmp short 05 */
		*(u8 *)(*pdest_addr + 2) = 0xEB;
		*(u8 *)(*pdest_addr + 3) = 0x05;
		
		/* jmp near <where j*cxz would jump> */
		*(u8 *)(*pdest_addr + 4) = KEDR_OP_JMP_REL32;
		*(u32 *)(*pdest_addr + 5) = (u32)X86_OFFSET_FROM_ADDR(
			*pdest_addr, KEDR_REL_JMP_SIZE, jump_addr);
		
		*pdest_addr += 9;
		/* Total length is 2+2+5=9 bytes. */
	}
	else {
		memcpy((void *)(*pdest_addr), (const void *)insn->kaddr, 
			insn->length);
		*pdest_addr += insn->length;
	}
	return;
}

/* Process the instruction specified, that is, copy it to the address 
 * *(unsigned long *)data, fixing up the code if necessary. */
static int
do_process_insn(struct kedr_tmod_function *func, struct insn *insn, 
	void *data)
{
	unsigned long offset_after_insn;
	unsigned long *pdest_addr = (unsigned long *)data;
	u8 opcode = insn->opcode.bytes[0];
	
	offset_after_insn = (unsigned long)insn->kaddr + 
		(unsigned long)insn->length - 
		(unsigned long)func->addr;
	
	/* If we've got too far, probably there is a bug in our system. It 
	 * is impossible for an instruction to be located at 64M distance
	 * or further from the beginning of the corresponding function. */
	WARN_ON(offset_after_insn >= 0x04000000UL);
	
	/* If we have skipped too many zeros at the end of the function, 
	 * that is, if we have cut off a part of the last instruction, fix 
	 * it now. */
	if (offset_after_insn > func->size)
		func->size = offset_after_insn;
	
	if (opcode == 0xeb) { /* jmp short */
		process_jmp_short(func, insn, pdest_addr);
	} 
	else if (opcode >= 0x70 && opcode <= 0x7f)  { /* jcc short */
		process_jcc_short(func, insn, pdest_addr);
	}
	else if (opcode == 0xeb) { /* j*cxz short */
		process_jcxz_short(func, insn, pdest_addr);
	} 
	else {
		copy_and_fixup_insn(insn, (void *)(*pdest_addr), func);
		*pdest_addr += insn->length;
	}
	
	// TODO: If this function does not just copy and fixup the 
	// instruction but rather emits something of different length to the
	// destination address, update '*pdest_addr' appropriately.
	
	return 0;
}

#ifdef CONFIG_X86_64
#define NUM_REGS 16 /* Number of general-purpose registers */
static const char *reg_name[NUM_REGS] = {
	"RAX", "RCX", "RDX", "RBX", "RSP", "RBP", "RSI", "RDI",
	"R8",  "R9",  "R10", "R11", "R12", "R13", "R14", "R15"
};

#else /* CONFIG_X86_32 */
#define NUM_REGS 8 /* Number of general-purpose registers */
static const char *reg_name[NUM_REGS] = {
	"EAX", "ECX", "EDX", "EBX", "ESP", "EBP", "ESI", "EDI"
};
	
#endif

static unsigned int reg_usage[NUM_REGS]; /* Usage count for each register */

static int 
process_reg_usage_proc(struct kedr_tmod_function *func, struct insn *insn, 
	void *data) 
{
	unsigned int mask;
	int i;
	char str[NUM_REGS * 4 + 1]; /* for the strings like "EAX EBP ESI" */
	
	str[0] = '\0';
	mask = register_usage_mask(insn, func);
	for (i = 0; i < NUM_REGS; ++i) {
		if (mask & X86_REG_MASK(i)) {
			reg_usage[i] += 1;
			strcat(str, reg_name[i]);
			strcat(str, " ");
		}
	}
	
	pr_info("[DBG] %3lx: %s\n", 
		(unsigned long)insn->kaddr - (unsigned long)func->addr,
		(mask != X86_REG_MASK_ALL ? str : "All registers are used"));	
	return 0;
}

/* Create an instrumented variant of function specified by 'func'. 
 * The function returns 0 if successful, an error code otherwise. 
 * 
 * The function also adjusts 'func->instrumented_size' if necessary (it is 
 * an estimate on entry). */
static int
instrument_function(struct kedr_tmod_function *func, struct module *mod)
{
	u32 *poffset;
	int ret = 0;
	unsigned long dest_addr;
	
	BUG_ON(func == NULL || func->addr == NULL);
	BUG_ON(	func->instrumented_addr == NULL || 
		func->instrumented_size == 0);
	
	/* If the function is too short (shorter than a single 'jmp rel32' 
	 * instruction), do not instrument it. */
	if (func->size < KEDR_REL_JMP_SIZE)
		return 0;
	
	/* Process instructions one by one, fixing them up if necessary. */
	ret = skip_trailing_zeros(func);
	if (ret != 0)
		return ret;
	
	dest_addr = (unsigned long)func->instrumented_addr;
	
	/* Place an "entry call" to kedr_ps_get_wrapper() at the beginning 
	 * of the function passing the address of the original function as 
	 * the argument. */
	memcpy(func->instrumented_addr, &entry_call_pattern[0], 
		entry_call_size);
	*(u32 *)(dest_addr + entry_call_pos_val) = 
		(u32)((unsigned long)(func->addr));
	*(u32 *)(dest_addr + entry_call_pos_func) = 
		X86_OFFSET_FROM_ADDR(
				/* -1 byte for opcode */
			(dest_addr + entry_call_pos_func - 1), 
			KEDR_SIZE_CALL_REL32, 
			(unsigned long)&kedr_ps_get_wrapper);

	dest_addr += entry_call_size;
	
	ret = for_each_insn_in_function(func, do_process_insn, &dest_addr);
	if (ret != 0)
		return ret;

	/* Adjust the length of the instrumented function */
	func->instrumented_size = dest_addr - 
		(unsigned long)func->instrumented_addr;
	
	//<>
	// For debugging: output the address of the instrumented function.
	// gdb -c /proc/kcore can be used to view the code of that function,
	// use 'disas /r <start_addr>,<end_addr>' for that.
	debug_util_print_string(func->name);
	debug_util_print_u64((u64)(unsigned long)(func->instrumented_addr), 
		" %llx\n");
	//<>
	
	/* Output register usage information for each instruction in this 
	 * function to the system log along with a summary. */
	if (0 == strcmp(func->name, target_function)) {
		int result;
		int i;
		pr_info("[DBG] Gathering register usage info for %s()\n",
			func->name);
			
		/* just in case */
		memset(&reg_usage[0], 0, sizeof(reg_usage)); 
		
		result = for_each_insn_in_function(
			func, process_reg_usage_proc, NULL);
		pr_info("[DBG] for_each_insn_in_function() returned %d\n", 
			result);
		
		pr_info ("[DBG] Register usage totals:\n");
		for (i = 0; i < NUM_REGS; ++i) {
			pr_info ("[DBG]   %s: %u\n", 
				reg_name[i], 
				reg_usage[i]);
		}
	}
	
	/* Save the bytes to be overwritten by the jump instruction and
	 * place the jump to the instrumented function at the beginning 
	 * of the original function. */
	memcpy(&func->orig_start_bytes[0], func->addr, KEDR_REL_JMP_SIZE);
	
	/* We allocate memory for the detour buffer in a special way, so 
	 * that it is "not very far" from where the code of the target 
	 * module resides. A near relative jump is enough in this case. */
	*(u8 *)func->addr = KEDR_OP_JMP_REL32;
	poffset = (u32 *)((unsigned long)func->addr + 1);
	*poffset = X86_OFFSET_FROM_ADDR((unsigned long)func->addr, 
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
		pr_info("[sample] "
"module: \"%s\", processing function \"%s\" (address is %p, size is %lu)\n",
		module_name(mod),
		f->name,
		f->addr,
		f->size);
		//<>
	
		ret = instrument_function(f, mod);
		if (ret != 0)
			return ret;
	}
	return 0;
}
/* ====================================================================== */
