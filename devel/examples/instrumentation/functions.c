/* functions.c: main operations with the functions in the target module:
 * enumeration, instrumentation, etc.
 * 
 * Unless specifically stated, the function returning int returns 0 on 
 * success and a negative error code on failure. */
/* ====================================================================== */ 

/* Main stages of processing:
 * 
 * 1. Fallback function instances: copy init and core areas of the target to
 * the module mapping space.
 * 
 * 2. Find the functions in the original code + find the addresses of the 
 * corresponding fallback functions. Create and partially initialize 'struct
 * kedr_ifunc' instances.
 *
 * 3. For each created 'kedr_ifunc' instance:
 *
 *    3.1. Create the instrumented instance in a temporary buffer (allocate
 *    with kmalloc/krealloc). Result: the code that only needs relocation,
 *    nothing more. 'tbuf_addr' and 'i_size' become defined. The value of 
 *    'i_addr' will be defined at step 5.
 *
 *    3.2. Fixup the jump tables for the original function to be usable by
 *    the fallback function. Before that, record somewhere which instruction
 *    each element of each jump table refers to (this will be necessary to
 *    prepare the jump tables for the instrumented instances).
 *
 *    3.3. Perform relocations in the code of the fallback function. This 
 *    code is now ready to be used.
 *
 * 4. Compute the needed size of the detour buffer (sum the aligned values 
 * of 'i_size' fields for each function + take the start alignment into 
 * account) and allocate the buffer.
 *
 * 5. Copy the instrumented code of each function to an appropriate place in
 * the detour buffer. kfree(tbuf_addr); set 'i_addr' to the final value. 
 * 
 * 6. Allocate (from the module mapping space) and properly fill the 
 * jump tables for the instrumented functions. Set the displacement in the 
 * corresponding jumps.
 *
 * 7. Perform relocations in the code of the instrumented functions. This 
 * code is now ready to be used. Among other things, if a 'call rel32' or 
 * a 'jmp rel32' refers to a function in the target module, change this
 * instruction to refer to the corresponding instrumented function.
 * 
 * 8. Overwrite the beginning of each original function with a jump to the 
 * corresponding instrumented function.
 *
 * After these steps are done, the instrumentation is complete. */
/* ====================================================================== */ 

#include <linux/kernel.h>
#include <linux/kallsyms.h>
#include <linux/errno.h>
#include <linux/sort.h>
#include <linux/slab.h>
#include <linux/string.h>
#include <linux/list.h>

/* [NB] We use a modified version of the instruction decoder and hence  
 * our header is included here instead of <asm/insn.h> provided by the 
 * kernel. */
#include <kedr/asm/insn.h> /* instruction analysis facilities */

#include "functions.h"
#include "debug_util.h"
#include "detour_buffer.h"
/* ====================================================================== */

/* Opcodes for 'jmp rel32' and 'call rel32'. */
#define KEDR_OP_JMP_REL32	0xe9
#define KEDR_OP_CALL_REL32	0xe8

/* Size of 'call near rel 32' instruction, in bytes. */
#define KEDR_SIZE_CALL_REL32	5

/* Size of 'jmp rel32' machine instruction on x86 (both 32- and 64-bit).
 * This number of bytes at the beginning of each function of the target
 * module will be overwritten during the instrumentation. */
#define KEDR_SIZE_JMP_REL32 	5

/* This structure represents a function in the code of the loaded target
 * module. 
 * Such structures are needed only during instrumentation and can be dropped
 * after that. */
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
	
	/* The list of code blocks in the function */
	//struct list_head blocks;
	
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
void *detour_buffer = NULL; 

/* Memory areas for fallback functions. */
void *fallback_init_area = NULL;
void *fallback_core_area = NULL;

/* The list of functions (struct kedr_ifunc) to be instrumented. */
LIST_HEAD(ifuncs);

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
 *   int <name>(struct kedr_ifunc *, struct insn *, void *)
 * That is, it will also get access to 'func' without the need for any
 * special wrapper structures (for_each_insn_in_function() handles wrapping
 * stuff itself). */

struct data_for_each_insn_in_function
{
	struct kedr_ifunc *func;
	void *data;
	int (*proc)(struct kedr_ifunc *, struct insn *, void *);
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
for_each_insn_in_function(struct kedr_ifunc *func, 
	int (*proc)(struct kedr_ifunc *, struct insn *, void *), 
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

/* For a given function, free the structures related to the jump tables for
 * the corresponding instrumented instance.
 * Deallocate and remove all members from func->jump_tables as well. */
static void 
cleanup_jump_tables(struct kedr_ifunc *func)
{
	unsigned int i;
	struct kedr_jtable *jtable;
	struct kedr_jtable *jtmp;
	
	list_for_each_entry_safe(jtable, jtmp, &func->jump_tables, list) {
		list_del(&jtable->list);
		kfree(jtable);
	}
	
	if (func->i_jump_tables == NULL)
		return;

	/* The first non-NULL element of func->i_jump_tables points to the 
	 * beginning of the whole allocated memory area. Find it and call
	 * kedr_free_detour_buffer() for it to release all the tables at 
	 * once. */
	for (i = 0; i < func->num_jump_tables; ++i) {
		if (func->i_jump_tables[i] != NULL) {
			kedr_free_detour_buffer(func->i_jump_tables[i]);
			break;
		}
	}
	
	kfree(func->i_jump_tables);
	func->i_jump_tables = NULL;
}

/* Destructor for 'struct kedr_ifunc' objects. */
static void
ifunc_destroy(struct kedr_ifunc *func)
{
	cleanup_jump_tables(func);
	
	/* If everything completed successfully, func->tbuf_addr must be 
	 * NULL. If an error occurred during the instrumentation, the
	 * temporary buffer for the instrumented instance may have remained
	 * unfreed. Free it now. */
	kfree(func->tbuf_addr);
	func->tbuf_addr = NULL;
	
	// TODO: release all other resources this function has acquired
}

/* Destroy all the structures contained in 'ifuncs' list and remove them
 * from the list, leaving it empty. */
static void
ifuncs_destroy_all(void)
{
	struct kedr_ifunc *pos;
	struct kedr_ifunc *tmp;
	
	list_for_each_entry_safe(pos, tmp, &ifuncs, list) {
		list_del(&pos->list);
		ifunc_destroy(pos);
		kfree(pos);
	}
}

/* Remove from the list and destroy the elements with zero size. 
 * Such elements may appear if there are aliases for one or more functions,
 * that is, if there are symbols with the same start address. When doing the
 * instrumentation, we need to process only one function of each such group,
 * no matter which one exactly. */
static void
ifuncs_remove_aliases(void)
{
	struct kedr_ifunc *pos;
	struct kedr_ifunc *tmp;
	
	list_for_each_entry_safe(pos, tmp, &ifuncs, list) {
		if (pos->size == 0) {
			list_del(&pos->list);
			kfree(pos);
		}
	}
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
register_usage_mask(struct insn *insn, struct kedr_ifunc *func)
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

static void
cleanup_fallback_areas(void)
{
	/* kedr_free_detour_buffer(NULL) is a no-op anyway */
	kedr_free_detour_buffer(fallback_init_area);
	fallback_init_area = NULL;
	kedr_free_detour_buffer(fallback_core_area);
	fallback_core_area = NULL;
}

static int
init_fallback_areas(const struct module *mod)
{
	/* Here we copy the code of the target module to some areas in the
	 * module mapping space. The functions contained there will be fixed
	 * up later and will serve as fallback functions in case something
	 * bad is detected by the instrumented code in runtime. For example,
	 * If the function call allocating the primary storage fails, it is
	 * not an option to let the instrumented function continue. Calling
	 * BUG() is not quite user-friendly. So, in such situations, control
	 * will be transferred to a fallback instance of the original 
	 * function and it should execute as usual. 
	 * The original function itself will be modified, a jump to the 
	 * instrumented code will be placed at its beginning, so we cannot 
	 * let the control to pass to it. That's why we need these fallback
	 * instances.
	 * Note that after module loading notifications are handled, the
	 * module loader may make the code of the module read only, so we 
	 * cannot uninstrument it and pass control there in runtime either.
	 */
	if (mod->module_init != NULL) {
		fallback_init_area = kedr_alloc_detour_buffer(
			mod->init_text_size);
		if (fallback_init_area == NULL)
			goto no_mem;
		
		memcpy(fallback_init_area, mod->module_init, 
			mod->init_text_size);
	}
	
	if (mod->module_core != NULL) {
		fallback_core_area = kedr_alloc_detour_buffer(
			mod->core_text_size);
		if (fallback_core_area == NULL)
			goto no_mem;
		
		memcpy(fallback_core_area, mod->module_core,
			mod->core_text_size);
	}
	return 0; /* success */

no_mem:
	cleanup_fallback_areas();	
	return -ENOMEM;
}

int
kedr_init_function_subsystem(struct module *mod)
{
	int ret = 0;
	
	num_funcs = 0;
	ret = init_fallback_areas(mod);
	
	// TODO: more initialization tasks here if necessary
	return ret;
}

void
kedr_cleanup_function_subsystem(void)
{
	// TODO: more cleanup tasks here if necessary
	ifuncs_destroy_all();
	
	kedr_free_detour_buffer(detour_buffer);
	detour_buffer = NULL;
	
	cleanup_fallback_areas();
}
/* ====================================================================== */

/* Nonzero if 'addr' is the address of some location in the "init" area of 
 * the module (may be code or data), 0 otherwise. */
static int
is_init_address(unsigned long addr, struct module *mod)
{
	BUG_ON(mod == NULL);
	if ((mod->module_init != NULL) &&
	    (addr >= (unsigned long)(mod->module_init)) &&
	    (addr < (unsigned long)(mod->module_init) + mod->init_size))
		return 1;
	
	return 0;
}

/* Nonzero if 'addr' is the address of some location in the "core" area of 
 * the module (may be code or data), 0 otherwise. */
static int
is_core_address(unsigned long addr, struct module *mod)
{
	BUG_ON(mod == NULL);

	if ((mod->module_core != NULL) &&
	    (addr >= (unsigned long)(mod->module_core)) &&
	    (addr < (unsigned long)(mod->module_core) + mod->core_size))
		return 1;
	
	return 0;
}

/* Nonzero if 'addr' is the address of some location in the code of the 
 * given module in the "init" area, 0 otherwise. */
static int
is_init_text_address(unsigned long addr, struct module *mod)
{
	BUG_ON(mod == NULL);
	if ((mod->module_init != NULL) &&
	    (addr >= (unsigned long)(mod->module_init)) &&
	    (addr < (unsigned long)(mod->module_init) + mod->init_text_size))
		return 1;
	
	return 0;
}

/* Nonzero if 'addr' is the address of some location in the code of the 
 * given module in the "core" area, 0 otherwise. */
static int
is_core_text_address(unsigned long addr, struct module *mod)
{
	BUG_ON(mod == NULL);

	if ((mod->module_core != NULL) &&
	    (addr >= (unsigned long)(mod->module_core)) &&
	    (addr < (unsigned long)(mod->module_core) + mod->core_text_size))
		return 1;
	
	return 0;
}

/* Nonzero if 'addr' is the address of some location in the code of the 
 * given module (*.text sections), 0 otherwise. */
static int
is_text_address(unsigned long addr, struct module *mod)
{
	return (is_core_text_address(addr, mod) || 
		is_init_text_address(addr, mod));
}

/* Nonzero if 'addr' is an address of some location within the given 
 * function, 0 otherwise. */
static int
is_address_in_function(unsigned long addr, struct kedr_ifunc *func)
{
	return (addr >= (unsigned long)func->addr && 
		addr < (unsigned long)func->addr + func->size);
}

/* Prepares the structires needed to instrument the given function.
 * Called for each function found in the target module.
 * 
 * Returns 0 if the processing succeeds, error otherwise.
 * This error will be propagated to the return value of 
 * kallsyms_on_each_symbol() */
static int
do_prepare_function(const char *name, struct module *mod, 
	unsigned long addr)
{
	struct kedr_ifunc *tf;
	tf = (struct kedr_ifunc *)kzalloc(
		sizeof(struct kedr_ifunc),
		GFP_KERNEL);
	if (tf == NULL)
		return -ENOMEM;
	
	tf->addr = (void *)addr; /* [NB] tf->size is 0 now */
	tf->name = name;
	//INIT_LIST_HEAD(&tf->blocks);
	INIT_LIST_HEAD(&tf->jump_tables);
	/* num_jump_tables is 0 now, i_jump_tables is NULL. */
	/* i_addr and tbuf_addr are also NULL.  */
	
	/* Find the corresponding fallback function, it's at the same offset 
	 * from the beginning of fallback_init_area or fallback_core_area as
	 * the original function is from the beginning of init or core area
	 * in the module, respectively. */
	if (is_core_text_address(addr, mod)) {
		tf->fallback = (void *)((unsigned long)fallback_core_area +
			(addr - (unsigned long)mod->module_core));
	} 
	else if (is_init_text_address(addr, mod)) {
		tf->fallback = (void *)((unsigned long)fallback_init_area +
			(addr - (unsigned long)mod->module_init));
	}
	else	/* Must not get here */
		BUG();
	
	list_add(&tf->list, &ifuncs);
	++num_funcs;

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
	 	int ret = do_prepare_function(name, mod, addr);
	 	if (ret)
			return ret;
	}
	return 0;
}

static int 
function_compare_by_address(const void *lhs, const void *rhs)
{
	const struct kedr_ifunc *left = 
		*(const struct kedr_ifunc **)(lhs);
	const struct kedr_ifunc *right = 
		*(const struct kedr_ifunc **)(rhs);
	
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
	struct kedr_ifunc **left = 
		(struct kedr_ifunc **)(lhs);
	struct kedr_ifunc **right = 
		(struct kedr_ifunc **)(rhs);
	struct kedr_ifunc *p;
	
	p = *left;
	*left = *right;
	*right = p;
}

/* Find the functions in the original code + find the addresses of the 
 * corresponding fallback functions. Create and partially initialize 'struct
 * kedr_ifunc' instances, add them to 'ifuncs' list. */
static int
find_functions(struct module *target_module)
{
	struct kedr_ifunc **pfuncs = NULL;
	struct kedr_ifunc init_text_end;
	struct kedr_ifunc core_text_end;
	struct kedr_ifunc *pos;
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
	pfuncs = (struct kedr_ifunc **)kzalloc(
		sizeof(struct kedr_ifunc *) * (num_funcs + 2), 
		GFP_KERNEL);
		
	if (pfuncs == NULL)
		return -ENOMEM;
	
	i = 0;
	list_for_each_entry(pos, &ifuncs, list) {
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
	sort(pfuncs, (size_t)i, sizeof(struct kedr_ifunc *), 
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
	
	ifuncs_remove_aliases();
	
	WARN_ON(list_empty(&ifuncs));
	return 0;
}
/* ====================================================================== */

/* The structure used to pass the required data to the instruction 
 * processing facilities (invoked by for_each_insn_in_function() in 
 * instrument_function() - hence "if_" in the name). 
 * The structure should be kept reasonably small in size so that it could be 
 * placed on the stack. */
struct kedr_if_data
{
	struct module *mod; /* target module */
	// TODO: add more fields here if necessary.
};

static int
skip_trailing_zeros(struct kedr_ifunc *func)
{
	/* Skip trailing zeros. If these are a part of an instruction,
	 * it will be handled automatically. If it just a padding sequence,
	 * we will avoid reading past the end of the function.
	 * Anyway, it is unlikely that a function ends with something like
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

/* Return nonzero if the given tables overlap, 0 otherwise. */
static int 
jtables_overlap(struct kedr_jtable *jtable1, struct kedr_jtable *jtable2)
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
resolve_jtables_overlaps(struct kedr_jtable *jtable, 
	struct kedr_ifunc *func)
{
	struct kedr_jtable *pos;
	list_for_each_entry(pos, &func->jump_tables, list) {
		if (!jtables_overlap(jtable, pos))
			continue;
		
		/* Due to the way the tables are searched for, they must end
		 * at the same address if they overlap. 
		 * 
		 * [NB] When adding, we take into account that *->addr is 
		 * a pointer to unsigned long. */
		WARN_ON(jtable->addr + jtable->num != pos->addr + pos->num);
		
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
handle_jmp_near_indirect(struct kedr_ifunc *func, struct insn *insn, 
	struct kedr_if_data *if_data)
{
	unsigned long jtable_addr;
	unsigned long end_addr = 0;
	int in_init = 0;
	int in_core = 0;
	struct module *mod = if_data->mod;
	unsigned long pos;
	unsigned int num_elems;
	/*unsigned int i;*/
	struct kedr_jtable *jtable;
	/*int ret = 0;*/
	
	jtable_addr = 
		X86_SIGN_EXTEND_V32(insn->displacement.value);
	
	/* [NB] Do not use is_*_text_address() here, because the jump tables
	 * are usually stored in one of the data sections rather than code
	 * sections. */
	if (is_core_address(jtable_addr, mod)) {
		in_core = 1;
		end_addr = (unsigned long)mod->module_core + 
			(unsigned long)mod->core_size - 
			sizeof(unsigned long);
	}
	else if (is_init_address(jtable_addr, mod)) {
		in_init = 1;
		end_addr = (unsigned long)mod->module_init + 
			(unsigned long)mod->init_size - 
			sizeof(unsigned long);
	}
	
	/* Sanity check: jtable_addr should point to some location within
	 * the module. */
	if (!in_core && !in_init) {
		pr_warning("[sample] Spurious jump table (?) at %p "
			"referred to by jmp at %pS, leaving it as is.\n",
			(void *)jtable_addr,
			insn->kaddr);
		WARN_ON_ONCE(1);
		return 0;
	}
	
	/* A rather crude (and probably not always reliable) way to find
	 * the number of elements in the jump table. */
	num_elems = 0;
	for (pos = jtable_addr; pos <= end_addr; 
		pos += sizeof(unsigned long)) {
		unsigned long jaddr = *(unsigned long *)pos;
		if (!is_address_in_function(jaddr, func))
			break;
		++num_elems;
	}
	
	/* Local near indirect jumps may only jump to the beginning of a 
	 * block, so we need to add the contents of the jump table to the 
	 * array of block boundaries. */
	/*ret = block_offsets_reserve(if_data, num_elems);
	if (ret != 0)
		return ret;
	
	for (i = 0; i < num_elems; ++i) {
		unsigned long jaddr = *((unsigned long *)jtable_addr + i);
		if_data->block_offsets[if_data->num++] = 
			(u32)(jaddr - func_start);
	}*/
	
	/* Store the information about this jump table in 'func'. It may be
	 * needed during instrumentation to properly fixup the contents of
	 * the table. */
	jtable = (struct kedr_jtable *)kzalloc(
		sizeof(struct kedr_jtable), GFP_KERNEL);
	if (jtable == NULL)
		return -ENOMEM;
	
	jtable->addr = (unsigned long *)jtable_addr;
	jtable->num  = num_elems;
	
	resolve_jtables_overlaps(jtable, func);
	
	/* We add the new item at the tail of the list to make sure the 
	 * order of the items is the same as the order of the corresponding
	 * indirect jumps. This simplifies creation of the jump tables for 
	 * the instrumented instance of the function. */
	list_add_tail(&jtable->list, &func->jump_tables);
	++func->num_jump_tables;
	
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
do_process_insn(struct kedr_ifunc *func, struct insn *insn, void *data)
{
	int ret = 0;
	struct kedr_if_data *if_data = (struct kedr_if_data *)data;
	unsigned long start_addr;
	unsigned long offset_after_insn;
	u8 opcode;
	
	BUG_ON(if_data == NULL);
	
	start_addr = (unsigned long)func->addr;
	offset_after_insn = (unsigned long)insn->kaddr + 
		(unsigned long)insn->length - start_addr;
		
	/* If we've got too far, probably there is a bug in our system. It 
	 * is impossible for an instruction to be located at 64M distance
	 * or further from the beginning of the corresponding function. */
	BUG_ON(offset_after_insn >= 0x04000000UL);
	
	/* If we have skipped too many zeros at the end of the function, 
	 * that is, if we have cut off a part of the last instruction, fix
	 * it now. */
	if (offset_after_insn > func->size)
		func->size = offset_after_insn;
	
	//<>
	// For now, just process indirect near jumps that can use jump 
	// tables. In the future - build the IR.
	opcode = insn->opcode.bytes[0];
	/* Some indirect near jumps need additional processing, namely those 
	 * that have the following form: 
	 * jmp near [<jump_table> + reg * <scale>]. 
	 * [NB] We don't need to do anything about other kinds of indirect 
	 * jumps, like jmp near [reg]. 
	 * 
	 * jmp near indirect has code FF/4. 'mod' and 'R/M' fields are used 
	 * here to determine if SIB byte is present. */
	if (opcode == 0xff && 
		X86_MODRM_REG(insn->modrm.value) == 4 && 
		X86_MODRM_MOD(insn->modrm.value) != 3 &&
		X86_MODRM_RM(insn->modrm.value) == 4) {
		ret = handle_jmp_near_indirect(func, insn, if_data);
		if (ret != 0)
			return ret;
	}
	//<>
	
	// TODO: record in IR which instruction each element in each
	// jump table refers to.
	
	// TODO
	return 0; 
}

/* Fix up the jump tables for the given function so that the fallback 
 * instance could use them. */
static void
fixup_fallback_jump_tables(struct kedr_ifunc *func)
{
	struct kedr_jtable *jtable;
	unsigned long func_start = (unsigned long)func->addr;
	unsigned long fallback_start = (unsigned long)func->fallback;
	
	list_for_each_entry(jtable, &func->jump_tables, list) {
		unsigned long *table = jtable->addr;
		unsigned int i;
		/* If the code refers to a "table" without elements (e.g. a 
		 * table filled with the addresses of other functons, etc.),
		 * nothing will be done. */
		for (i = 0; i < jtable->num; ++i)
			table[i] = table[i] - func_start + fallback_start;
	}
}

/* Creates the jump tables for the instrumented instance of the function 
 * 'func' based on the jump tables for the original function. The jump 
 * tables will be filled with meaningful data during the instrumentation. 
 * For now, they will be just allocated, and the pointers to them will be 
 * stored in func->i_jump_tables[]. If an item of jump_table list has 0 
 * elements, the corresponding item in func->i_jump_tables[] will be NULL.
 *
 * [NB] The order of the corresponding indirect jumps and the order of the 
 * elements in func->jump_tables list must be the same. 
 *
 * [NB] In case of error, func->i_jump_tables will be freed in 
 * ifunc_destroy(), so it is not necessary to free it here. */
static int 
create_jump_tables(struct kedr_ifunc *func)
{
	struct kedr_jtable *jtable;
	unsigned int total = 0;
	unsigned int i = 0;
	void *buf;
	
	if (func->num_jump_tables == 0)
		return 0;
		
	func->i_jump_tables = kzalloc(
		func->num_jump_tables * sizeof(unsigned long *),
		GFP_KERNEL);
	if (func->i_jump_tables == NULL)
		return -ENOMEM;
	
	/* Find the total number of elements in all jump tables for this 
	 * function. */
	list_for_each_entry(jtable, &func->jump_tables, list) {
		total += jtable->num;
	}
	
	/* If there are jump tables but each of these jump tables has no
	 * elements (i.e. the jumps are not within the function), nothing to
	 * do. */
	if (total == 0)
		return 0;
	
	buf = kedr_alloc_detour_buffer(total * sizeof(unsigned long));
	if (buf == NULL)
		return -ENOMEM;
	
	list_for_each_entry(jtable, &func->jump_tables, list) {
		if (jtable->num == 0) {
			func->i_jump_tables[i] = NULL;
		} 
		else {
			func->i_jump_tables[i] = (unsigned long *)buf;
			buf = (void *)((unsigned long)buf + 
				jtable->num * sizeof(unsigned long));
		}
		++i;
	}
	BUG_ON(i != func->num_jump_tables);
	return 0;
}

/* Creates the instrumented instance in a temporary buffer. The resulting 
 * code will only need relocation before it can be used. 'tbuf_addr' and 
 * 'i_size' become defined, 'tbuf_addr' pointing to that buffer. 
 * [NB] The value of 'i_addr' will be defined later, when the function is 
 * copied to its final location. 
 *
 * [NB] Here, we can assume that the size of the function is not less than
 * the size of 'jmp near rel32'.
 * 
 * The data offsets (used with RIP-relative addressing) and call/jump 
 * offsets will be relocated here as if each corresponding instruction was
 * at address 0. */
static int
instrument_function(struct kedr_ifunc *func, struct module *mod)
{
	int ret = 0;
	
	//<>
	u32 *poffset;
	//<>
	struct kedr_if_data if_data;
	
	ret = skip_trailing_zeros(func);
	if (ret != 0)
		return ret;
	
	/* First, decode and process the machine instructions one by one and
	 * build the IR (TODO: IR). 
	 * 
	 * do_process_insn() will also adjust the length of the function if
	 * we have skipped too many zeros. */
	if_data.mod = mod;
	ret = for_each_insn_in_function(func, do_process_insn, 
		(void *)&if_data);
	if (ret != 0)
		return ret;
	
	/* Allocate and partially initialize the jump tables for the 
	 * instrumented instance. */
	ret = create_jump_tables(func);
	if (ret != 0)
		return ret;
	
	// TODO: do_instrumentation() - perform the instrumentation.
	// Among other things, fill the jump tables (if any) with the 
	// "pointers" to the appropriate positions in the instrumented 
	// function. The quotation marks are here to imply that these values
	// are not actually pointers at this stage. They are computed as if
	// the instrumented function had the start address of 0. They will
	// be fixed up during the deployment phase.
	
	fixup_fallback_jump_tables(func);
	
	//<>
	// For now, the instrumented instance will contain only a jump to 
	// the fallback function (relocated to the instruction address of 0)
	// to check the mechanism.
	func->tbuf_addr = kzalloc(KEDR_SIZE_JMP_REL32, GFP_KERNEL);
	if (func->tbuf_addr == NULL)
		return -ENOMEM;
	
	func->i_size = KEDR_SIZE_JMP_REL32;
	
	*(u8 *)func->tbuf_addr = KEDR_OP_JMP_REL32;
	poffset = (u32 *)((unsigned long)func->tbuf_addr + 1);
	*poffset = X86_OFFSET_FROM_ADDR(0, 
		KEDR_SIZE_JMP_REL32, 
		(unsigned long)func->fallback);
	//<>
	
	return 0;
}

/* Relocate the given instruction in the fallback function in place. The 
 * code was "moved" from base address func->addr to func->fallback. 
 * [NB] No need to process short jumps outside of the function, they are 
 * already usable. This is because the positions of the functions relative 
 * to each other are the same as for the original functions. */
static int
relocate_insn_in_fallback(struct insn *insn, void *data)
{
	struct kedr_ifunc *func = (struct kedr_ifunc *)data;
	u32 *to_fixup;
	unsigned long addr;
	u32 new_offset;
	BUG_ON(insn->length == 0);
	
	if (insn->opcode.bytes[0] == KEDR_OP_CALL_REL32 ||
	    insn->opcode.bytes[0] == KEDR_OP_JMP_REL32) {
		/* For calls and jumps, the decoder stores the offset in 
		 * 'immediate' field rather than in 'displacement'.
		 * [NB] When dealing with RIP-relative addressing on x86-64,
		 * it uses 'displacement' field for that purpose. */
		 
		/* Find the new offset corresponding to the same address */
		new_offset = (u32)((unsigned long)func->addr + 
			X86_SIGN_EXTEND_V32(insn->immediate.value) -
			(unsigned long)func->fallback);
		
		/* Then calculate the address the instruction refers to.
		 * The original instruction referred to this address too. */
		addr = (unsigned long)X86_ADDR_FROM_OFFSET(
			insn->kaddr,
			insn->length, 
			new_offset);
		
		if (is_address_in_function(addr, func))
		/* no fixup needed, the offset may remain the same */
			return 0; 
		
		/* Call or jump outside of the function. Set the new offset
		 * so that the instruction referred to the same address as 
		 * the original one. */
		to_fixup = (u32 *)((unsigned long)insn->kaddr + 
			insn_offset_immediate(insn));
		*to_fixup = new_offset;
		return 0;
	}

#ifdef CONFIG_X86_64
	if (!insn_rip_relative(insn))
		return 0;

	/* Handle RIP-relative addressing */
	/* Find the new offset first */
	new_offset = (u32)((unsigned long)func->addr + 
		X86_SIGN_EXTEND_V32(insn->displacement.value) -
		(unsigned long)func->fallback);
		
	/* Then calculate the address the instruction refers to.
	 * The original instruction referred to this address too. */
	addr = (unsigned long)X86_ADDR_FROM_OFFSET(
		insn->kaddr,
		insn->length, 
		new_offset);
	
	/* Check if the instruction addresses something inside the original
	 * function. If so, no fixup is necessary because the copied 
	 * instruction already refers to the corresponding part of the 
	 * fallback function (the offset is the same). */
	if (is_address_in_function(addr, func))
		return 0;
	
	to_fixup = (u32 *)((unsigned long)insn->kaddr + 
		insn_offset_displacement(insn));
	*to_fixup = new_offset;
#endif
	return 0;
}

/* Performs relocations in the code of the fallback instance of a function. 
 * After that, the instance is ready to be used. */
static int 
relocate_fallback_function(struct kedr_ifunc *func)
{
	return for_each_insn((unsigned long)func->fallback, 
		(unsigned long)func->fallback + func->size, 
		relocate_insn_in_fallback, 
		(void *)func); 
}

/* Creates an instrumented instance of function specified by 'func' and 
 * prepares the corresponding fallback function for later usage. */
static int
do_process_function(struct kedr_ifunc *func, struct module *mod)
{
	int ret = 0;
	BUG_ON(func == NULL || func->addr == NULL);
		
	/* If the function is too short (shorter than a single 'jmp rel32' 
	 * instruction), do not instrument it. Just report success and do 
	 * nothing more. 
	 * func->i_size will remain 0, func->tbuf_addr and func->i_addr - 
	 * NULL. */
	if (func->size < KEDR_SIZE_JMP_REL32)
		return 0;
	
	ret = instrument_function(func, mod);
	if (ret != 0)
		return ret;
	
	/* Just in case func->i_addr was erroneously used instead of 
	 * func->tbuf_addr. */
	BUG_ON(func->i_addr != NULL);
	
	/* The buffer must have been allocated. */
	BUG_ON(func->tbuf_addr == NULL); 
	
	ret = relocate_fallback_function(func);
	if (ret != 0)
		return ret;

	return 0; /* success */
}
/* ====================================================================== */

/* Computes the needed size of the detour buffer (the instrumented instances
 * of the functions must have been prepared by this time) and allocates the 
 * buffer. */
static int 
create_detour_buffer(void)
{
	/* Spare bytes to align the start of the buffer, just in case. */
	unsigned long size = KEDR_FUNC_ALIGN; 
	struct kedr_ifunc *f;
	
	list_for_each_entry(f, &ifuncs, list) {
		size += KEDR_ALIGN_VALUE(f->i_size); 
		/* OK even if f->i_size == 0 (small function left 
		 * uninstrumented). */
	}
	
	BUG_ON(detour_buffer != NULL);
	detour_buffer = kedr_alloc_detour_buffer(size);
	if (detour_buffer == NULL)
		return -ENOMEM;

	return 0;
}

/* The elements in the jump tables have been calculated based on the base 
 * address (address of the instrumented instance) of 0. This function fixes 
 * them up for the real base address (func->i_addr). */
static void
fixup_instrumented_jump_tables(struct kedr_ifunc *func)
{
	unsigned int i = 0;
	struct kedr_jtable *jtable;
	
	list_for_each_entry(jtable, &func->jump_tables, list) {
		unsigned int k;
		unsigned long *table;
		
		if (func->i_jump_tables[i] == NULL) {
			BUG_ON(jtable->num != 0);
			++i;
			continue;
		}
		
		table = func->i_jump_tables[i];
		for (k = 0; k < jtable->num; ++k)
			table[k] += (unsigned long)func->i_addr;
		++i;
	}
	BUG_ON(i != func->num_jump_tables);
}

/* Fixup call/jump addresses in the instrumented code if necessary. 
 * On entry, the call/jump offsets are as if the address of the call/jump 
 * instruction was 0. */
static int
relocate_insn_in_icode(struct insn *insn, void *data)
{
	struct kedr_ifunc *func = (struct kedr_ifunc *)data;
	u32 *to_fixup;
	unsigned long addr;
	u32 new_offset;
	BUG_ON(insn->length == 0);
	
	if (insn->opcode.bytes[0] == KEDR_OP_CALL_REL32 ||
	    insn->opcode.bytes[0] == KEDR_OP_JMP_REL32) {
		/* For calls and jumps, the decoder stores the offset in 
		 * 'immediate' field rather than in 'displacement'.
		 * [NB] When dealing with RIP-relative addressing on x86-64,
		 * it uses 'displacement' field for that purpose. */
		
		/* Find out if the instruction should refer to somewhere 
		 * within the instrumented function: calculate the address
		 * it would refer to if it had the same destination offset
		 * as before. */
		addr = (unsigned long)X86_ADDR_FROM_OFFSET(
			insn->kaddr,
			insn->length, 
			insn->immediate.value);
		
		/* If the destination address is inside the instrumented 
		 * function, then OK, nothing more to do. Otherwise, patch
		 * the offset for the instruction to refer to the same 
		 * address as before (call/jump outside). */
		if (addr >= (unsigned long)func->i_addr && 
		    addr < (unsigned long)func->i_addr + func->i_size)
			return 0;
		
		/* Call/jump outside. Calculate and set a new offset
		 * so that the instruction referred to the same address as 
		 * the original one. */
		new_offset = (u32)( 
			X86_SIGN_EXTEND_V32(insn->immediate.value) -
			(unsigned long)insn->kaddr);
		
		to_fixup = (u32 *)((unsigned long)insn->kaddr + 
			insn_offset_immediate(insn));
		*to_fixup = new_offset;
		return 0;
	}

#ifdef CONFIG_X86_64
	if (!insn_rip_relative(insn))
		return 0;

	/* Handle RIP-relative addressing. Same as with call/jump but using
	 * displacement.value rather than immediate.value. */
	addr = (unsigned long)X86_ADDR_FROM_OFFSET(
		insn->kaddr,
		insn->length,
		insn->displacement.value);

	if (addr >= (unsigned long)func->i_addr && 
	    addr < (unsigned long)func->i_addr + func->i_size)
		return 0;
	
	new_offset = (u32)( 
		X86_SIGN_EXTEND_V32(insn->displacement.value) -
		(unsigned long)insn->kaddr);
		
	to_fixup = (u32 *)((unsigned long)insn->kaddr + 
		insn_offset_displacement(insn));
	*to_fixup = new_offset;
#endif
	return 0;
}

/* Performs fixup of call and jump addresses in the instrumented instance, 
 * as well as RIP-relative addressing, and the contents of the jump tables.
 * Note that the addressing expressions for the jump tables themselves must 
 * be already in place: the instrumentation phase takes care of that. */
static int 
deploy_instrumented_function(struct kedr_ifunc *func)
{
	int ret = 0;
	fixup_instrumented_jump_tables(func);
	
	/* Decode the instructions from the instrumented function again
	 * after they have been placed at their final location and fix them 
	 * up if necessary. */
	ret = for_each_insn((unsigned long)func->i_addr, 
		(unsigned long)func->i_addr + func->i_size, 
		relocate_insn_in_icode,
		(void *)func);
	if (ret != 0)
		return ret;
	
	//<>
	// For debugging: output the address of the instrumented function.
	// gdb -c /proc/kcore can be used to view the code of that function,
	// use 'disas /r <start_addr>,<end_addr>' for that.
	debug_util_print_string(func->name);
	debug_util_print_u64((u64)(unsigned long)(func->i_addr), " %llx, ");
	debug_util_print_u64((u64)(func->i_size), "size: %llx\n");
	//<>
	
	return 0;
}

/* Deploys the instrumented code of each function to an appropriate place in
 * the detour buffer. Releases the temporary buffer and sets 'i_addr' to the
 * final address of the instrumented instance. */
static int
deploy_instrumented_code(void)
{
	struct kedr_ifunc *func;
	unsigned long dest_addr;
	int ret = 0;
	
	BUG_ON(detour_buffer == NULL);
	
	dest_addr = KEDR_ALIGN_VALUE(detour_buffer);
	list_for_each_entry(func, &ifuncs, list) {
		if (func->i_size == 0) 
			/* the function was too small to be instrumented */
			continue;
		
		BUG_ON(func->tbuf_addr == NULL);
		BUG_ON(func->i_addr != NULL);
		
		memcpy((void *)dest_addr, func->tbuf_addr, func->i_size);
		kfree(func->tbuf_addr); /* free the temporary buffer */
		func->tbuf_addr = NULL;
		func->i_addr = (void *)dest_addr;
		
		ret = deploy_instrumented_function(func);
		if (ret != 0)
			return ret;
		/* Should the deployment fail, the destructors for struct
		 * kedr_ifunc instances will free the remaining temporary 
		 * buffers. So we don't need to worry about these here. */
		
		dest_addr += KEDR_ALIGN_VALUE(func->i_size);
	}
	return 0;
}

/* For each original function, place a jump to the instrumented instance at
 * the beginning and fill the rest with '0xcc' (breakpoint) instructions. */
static void
detour_original_functions(void)
{
	u32 *pos;
	struct kedr_ifunc *func;
	
	list_for_each_entry(func, &ifuncs, list) {
		/* Place the jump to the instrumented instance at the 
		 * beginning of the original instance.
		 * [NB] We allocate memory for the detour buffer in a 
		 * special way, so that it is "not very far" from where the 
		 * code of the target module resides. A near relative jump 
		 * is enough in this case. */
		*(u8 *)func->addr = KEDR_OP_JMP_REL32;
		pos = (u32 *)((unsigned long)func->addr + 1);
		*pos = X86_OFFSET_FROM_ADDR((unsigned long)func->addr, 
			KEDR_SIZE_JMP_REL32, (unsigned long)func->i_addr);

		/* Fill the rest of the original function's code with 
		 * 'int 3' (0xcc) instructions to detect if control still 
		 * transfers there despite all our efforts. 
		 * If we do not handle some situation where the control 
		 * transfers somewhere within an original function rather 
		 * than to its beginning, we better know this early. */
		if (func->size > KEDR_SIZE_JMP_REL32) {
			memset((void *)((unsigned long)func->addr + 
					KEDR_SIZE_JMP_REL32), 
				0xcc, 
				func->size - KEDR_SIZE_JMP_REL32);
		}
	}
}

int
kedr_process_target(struct module *mod)
{
	struct kedr_ifunc *f;
	int ret = 0;
	
	BUG_ON(mod == NULL);
	
	ret = find_functions(mod);
	if (ret != 0)
		return ret;
	/* [NB] For each function, the address of its fallback instance is 
	 * now known (if the function is not too small). */
	
	list_for_each_entry(f, &ifuncs, list) {
		//<>
		pr_info("[sample] "
"module: \"%s\", processing function \"%s\" "
"(address is %p, size is %lu; fallback is at %p)\n",
		module_name(mod),
		f->name,
		f->addr,
		f->size,
		f->fallback);
		//<>
	
		ret = do_process_function(f, mod);
		if (ret != 0)
			return ret;
	}
	
	ret = create_detour_buffer();
	if (ret != 0)
		return ret;

	ret = deploy_instrumented_code();
	if (ret != 0)
		return ret;
	
	detour_original_functions();
	return 0;
}
/* ====================================================================== */
