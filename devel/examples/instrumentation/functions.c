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
 * kedr_ifunc' instances. Do not consider st
 *
 * 3. For each created 'kedr_ifunc' instance:
 *
 *    3.1. Create the instrumented instance in a temporary buffer (allocate
 *    with kmalloc/krealloc). Result: the code that only needs relocation,
 *    nothing more. 'tbuf' and 'i_size' become defined. The value of 
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
 * the detour buffer. kfree(tbuf); set 'i_addr' to the final value. 
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
#include "ifunc.h"
#include "instrument.h"
#include "util.h"
#include "sections.h"
/* ====================================================================== */

/* Detour buffer for the target module. The instrumented code of the 
 * functions will be copied there. It is that code that will actually be
 * executed. A jump to the start of the instrumented function will be placed
 * at the beginning of the original function, so the rest of the latter 
 * should never be executed. */
static void *detour_buffer = NULL; 

/* Memory areas for fallback functions. */
static void *fallback_init_area = NULL;
static void *fallback_core_area = NULL;

/* The list of functions (struct kedr_ifunc) to be instrumented. */
static LIST_HEAD(ifuncs);

/* Number of functions in the target module */
static unsigned int num_funcs = 0;
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

/* Destroy the given 'struct kedr_ifunc' object (and release the memory it
 * occupies, among other things). */
static void
ifunc_destroy(struct kedr_ifunc *func)
{
	struct kedr_reloc *reloc;
	struct kedr_reloc *tmp;

	cleanup_jump_tables(func);
	
	/* If everything completed successfully, func->tbuf must be 
	 * NULL. If an error occurred during the instrumentation, the
	 * temporary buffer for the instrumented instance may have remained
	 * unfreed. Free it now. */
	kfree(func->tbuf);
	
	/* Free the list of relocations. */
	list_for_each_entry_safe(reloc, tmp, &func->relocs, list) {
		list_del(&reloc->list);
		kfree(reloc);
	}
	
	kfree(func);	
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
	}
	
	num_funcs = 0; /* just in case */
}

/* Remove from the list and destroy the elements with the size less than the
 * size of 'jmp near rel32'.
 * 
 * The elements with zero size may appear if there are aliases for one or 
 * more functions, that is, if there are symbols with the same start 
 * address. When doing the instrumentation, we need to process only one 
 * function of each such group, no matter which one exactly. 
 * 
 * The functions with the size less than the size of 'jmp near rel32' can 
 * not be detoured anyway (not enough space to place a jump in). We remove 
 * them from the list to avoid checking the size of the function each time 
 * later. */
static void
ifuncs_remove_aliases_and_small_funcs(void)
{
	struct kedr_ifunc *pos;
	struct kedr_ifunc *tmp;
	
	list_for_each_entry_safe(pos, tmp, &ifuncs, list) {
		if (pos->size < KEDR_SIZE_JMP_REL32) {
			list_del(&pos->list);
			kfree(pos);
			--num_funcs;
		}
	}
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
	
	BUG_ON(!list_empty(&ifuncs));
	
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

/* Prepares the structures needed to instrument the given function.
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

	INIT_LIST_HEAD(&tf->jump_tables);
	INIT_LIST_HEAD(&tf->relocs);

	/* num_jump_tables is 0 now, i_jump_tables is NULL. */
	/* i_addr and tbuf are also NULL.  */
	
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

/* Deletes all the special items (see below) from the list and frees them.*/
static void
destroy_special_items(struct list_head *items_list)
{
	struct kedr_ifunc *pos;
	struct kedr_ifunc *tmp;
	
	BUG_ON(items_list == NULL);
	
	list_for_each_entry_safe(pos, tmp, items_list, list) {
		list_del(&pos->list);
		kfree(pos);
	}
}

/* Allocates a special item and sets 'addr' field in it to the given value. 
 * Returns the pointer to the item if successful, NULL if there is not 
 * enough memory. 
 * 
 * [NB] For special items, it is enough to set only the address. */
static struct kedr_ifunc *
construct_item(unsigned long addr)
{
	struct kedr_ifunc *item;
	item = (struct kedr_ifunc *)kzalloc(sizeof(struct kedr_ifunc), 
		GFP_KERNEL);
	if (item == NULL)
		return NULL;
	
	item->addr = (void *)addr;
	return item;
}

/* Creates the list of the special items, i.e. the instances of 
 * struct kedr_ifunc representing sections and the ends of the areas of the
 * given module. 
 * The items will be added to the specified list (must be empty on entry).
 * If successful, the function returns the number of items created (>= 0).
 * A negative error code is returned in case of failure. */
static int
create_special_items(struct list_head *items_list, 
	struct module *target_module)
{
	int num = 0;
	int ret = 0;
	struct list_head *section_list = NULL;
	struct kedr_section *sec;
	struct kedr_ifunc *item;
	
	BUG_ON(target_module == NULL);
	BUG_ON(items_list == NULL);
	BUG_ON(!list_empty(items_list));
	
	/* Obtain names and addresses of the target's ELF sections */
	section_list = kedr_get_sections(target_module);
	BUG_ON(section_list == NULL);
	if (IS_ERR(section_list)) {
		pr_err("[sample] "
	"Failed to obtain names and addresses of the target's sections.\n");
		ret = (int)PTR_ERR(section_list);
		goto out;
	}
	
	list_for_each_entry(sec, section_list, list) {
		item = construct_item(sec->addr);
		if (item == NULL) {
			ret = -ENOMEM;
			goto out;
		}
		list_add_tail(&item->list, items_list);
		++num;
	}
	
	/* Here we rely on the fact that the code is placed at the beginning
	 * of "init" and "core" areas of the module by the module loader. */
	if (target_module->module_init != NULL) {
		item = construct_item(
			(unsigned long)target_module->module_init + 
			target_module->init_text_size);
		if (item == NULL) {
			ret = -ENOMEM;
			goto out;
		}
		list_add_tail(&item->list, items_list);
		++num;
	}

	if (target_module->module_core != NULL) {
		item = construct_item(
			(unsigned long)target_module->module_core + 
			target_module->core_text_size);
		if (item == NULL) {
			ret = -ENOMEM;
			goto out;
		}
		list_add_tail(&item->list, items_list);
		++num;
	}

	return num;

out:
	destroy_special_items(items_list);
	return ret;
}

/* A helper structure that is used to implement stable sorting of function
 * boundaries (to determine function size later). 
 * '*obj' may represent a function or a special item (like the end of "init"
 * or "core" area or the start of a section). 
 * 'index' is the original index of an item, i.e. the index in the array 
 * before sorting. */
struct func_boundary_item
{
	struct kedr_ifunc *obj;
	int index;
};

/* Compare pairs (addr, index) in a lexicographical order. This is 
 * to make sorting stable, that is, to preserve the order of the elements
 * with equal values of 'addr'. 
 * 'index' is the index of the element in the array before sorting. */
static int 
compare_items(const void *lhs, const void *rhs)
{
	const struct func_boundary_item *left = 
		(const struct func_boundary_item *)lhs;
	const struct func_boundary_item *right = 
		(const struct func_boundary_item *)rhs;
	
	if (left->obj->addr == right->obj->addr) {
		if (left->index == right->index)
		/* may happen only if an element is compared to itself */
			return 0; 
		else if (left->index < right->index)
			return -1;
		else 
			return 1;
	}
	else if (left->obj->addr < right->obj->addr)
		return -1;
	else 
		return 1;
}

static void 
swap_items(void *lhs, void *rhs, int size)
{
	struct func_boundary_item *left = 
		(struct func_boundary_item *)lhs;
	struct func_boundary_item *right = 
		(struct func_boundary_item *)rhs;
	struct func_boundary_item p;
	
	BUG_ON(size != (int)sizeof(struct func_boundary_item));
	
	p.obj = left->obj;
	p.index = left->index;
	
	left->obj = right->obj;
	left->index = right->index;
	
	right->obj = p.obj;
	right->index = p.index;
}

/* Find the functions in the original code and find the addresses of the 
 * corresponding fallback functions. Create and partially initialize 'struct
 * kedr_ifunc' instances, add them to 'ifuncs' list. */
static int
find_functions(struct module *target_module)
{
	struct func_boundary_item *func_boundaries = NULL;
	LIST_HEAD(special_items);
	struct kedr_ifunc *pos;
	int ret; 
	int i;
	int num_special;
	
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
	pr_info("[sample] Found %u functions in \"%s\"\n",
		num_funcs,
		module_name(target_module));
	
	ret = create_special_items(&special_items, target_module);
	if (ret < 0)
		return ret;
	
	num_special = ret;
	/* num_special should not be 0: at least "core" area should be 
	 * present. If it is 0, it is weird. */
	WARN_ON(num_special == 0); 
	
	/* This array is only necessary to estimate the size of each 
	 * function. */
	func_boundaries = (struct func_boundary_item *)kzalloc(
		sizeof(struct func_boundary_item) * 
			(num_funcs + num_special),
		GFP_KERNEL);
		
	if (func_boundaries == NULL) {
		destroy_special_items(&special_items);
		return -ENOMEM;
	}
	
	/* We add special items before the regular functions. Because of the 
	 * fact that the way of sorting we use is stable, the special items
	 * having the same address as a function will still appear before 
	 * the function in the sorted array. 
	 * 
	 * Note that sort() is not required to be stable by itself. */
	i = 0;
	list_for_each_entry(pos, &special_items, list) {
		func_boundaries[i].obj = pos;
		func_boundaries[i].index = i;
		++i;
	}
	BUG_ON(i != num_special);
	
	list_for_each_entry(pos, &ifuncs, list) {
		func_boundaries[i].obj = pos;
		func_boundaries[i].index = i;
		++i;
	}
	BUG_ON(i != (int)num_funcs + num_special);
	--i;
	
	sort(func_boundaries, (size_t)(num_funcs + num_special), 
		sizeof(struct func_boundary_item), 
		compare_items, swap_items);

	//<>
	/*debug_util_print_u64((u64)(func_boundaries[i].obj->addr),
		"0x%llx\n");*/
	//<>	
	while (i-- > 0) {
		func_boundaries[i].obj->size = 
			((unsigned long)(func_boundaries[i + 1].obj->addr) - 
			(unsigned long)(func_boundaries[i].obj->addr));
		//<>
		/*debug_util_print_u64((u64)(func_boundaries[i].obj->addr),
			"0x%llx\n");*/
		//<>
	}
	kfree(func_boundaries);
	destroy_special_items(&special_items);
	
	/* If there are aliases besides "init_module" and "cleanup_module",
	 * i.e. the symbols with different names and the same address, the 
	 * size of only one of the symbols in such group will be non-zero. 
	 * So we can simply skip symbols with size 0. */
	ifuncs_remove_aliases_and_small_funcs();
	if (list_empty(&ifuncs))
		pr_info("[sample] No functions found in \"%s\" "
			"that can be instrumented\n",
			module_name(target_module));
	return 0;
}
/* ====================================================================== */

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
	/* Find the new offset first. We assume that the instruction refers
	 * to something outside of the function. The instrumentation system
	 * must check this, see do_process_insn() in instrument.c. */
	new_offset = (u32)((unsigned long)func->addr + 
		X86_SIGN_EXTEND_V32(insn->displacement.value) -
		(unsigned long)func->fallback);
		
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

/* Creates an instrumented instance of function specified by 'func' and 
 * prepares the corresponding fallback function for later usage. */
static int
do_process_function(struct kedr_ifunc *func, struct module *mod)
{
	int ret = 0;
	BUG_ON(func == NULL || func->addr == NULL);
	
	/* Small functions should have been removed from the list */
	BUG_ON(func->size < KEDR_SIZE_JMP_REL32);
	
	ret = instrument_function(func, mod);
	if (ret != 0)
		return ret;
	
	/* Just in case func->i_addr was erroneously used instead of 
	 * func->tbuf. */
	BUG_ON(func->i_addr != NULL);
	
	/* The buffer must have been allocated. */
	BUG_ON(func->tbuf == NULL); 
	
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

/* The instruction to be relocated can be either call/jmp rel32 or
 * an instruction using RIP-relative addressing.
 * 'dest' is the address it should refer to. */
static int
relocate_insn_in_icode(struct insn *insn, void *dest)
{
	u32 *to_fixup;
	
	BUG_ON(insn->length == 0);
	
	if (insn->opcode.bytes[0] == KEDR_OP_CALL_REL32 ||
	    insn->opcode.bytes[0] == KEDR_OP_JMP_REL32) {
		/* For calls and jumps, the decoder stores the offset in 
		 * 'immediate' field rather than in 'displacement'.
		 * [NB] When dealing with RIP-relative addressing on x86-64,
		 * it uses 'displacement' field for that purpose. */
		
		to_fixup = (u32 *)((unsigned long)insn->kaddr + 
			insn_offset_immediate(insn));
		*to_fixup = (u32)X86_OFFSET_FROM_ADDR(
			insn->kaddr, 
			insn->length,
			dest);
		return 0;
	}

#ifdef CONFIG_X86_64
	if (!insn_rip_relative(insn))
		return 0;

	to_fixup = (u32 *)((unsigned long)insn->kaddr + 
		insn_offset_displacement(insn));
	*to_fixup = (u32)X86_OFFSET_FROM_ADDR(
		insn->kaddr, 
		insn->length,
		dest);
#endif
	return 0;
}

/* Performs fixup of call and jump addresses in the instrumented instance, 
 * as well as RIP-relative addressing, and the contents of the jump tables.
 * Note that the addressing expressions for the jump tables themselves must 
 * be already in place: the instrumentation phase takes care of that. */
static void
deploy_instrumented_function(struct kedr_ifunc *func)
{
	struct kedr_reloc *reloc;
	struct kedr_reloc *tmp;

	fixup_instrumented_jump_tables(func);
	
	/* Decode the instructions that should be relocated and perform 
	 * relocations. Free the relocation structures when done, they are 
	 * no longer needed. */
	list_for_each_entry_safe(reloc, tmp, &func->relocs, list) {
		struct insn insn;
		void *kaddr = (void *)((unsigned long)func->i_addr + 
			reloc->offset);
		
		BUG_ON(reloc->offset >= func->i_size);
		
		kernel_insn_init(&insn, kaddr);
		insn_get_length(&insn);
		
		relocate_insn_in_icode(&insn, reloc->dest);
		
		list_del(&reloc->list);
		kfree(reloc);
	}
	
	//<>
	// For debugging: output the address of the instrumented function.
	// gdb -c /proc/kcore can be used to view the code of that function,
	// use 'disas /r <start_addr>,<end_addr>' for that.
	debug_util_print_string(func->name);
	debug_util_print_u64((u64)(unsigned long)(func->i_addr), " %llx, ");
	debug_util_print_u64((u64)(func->i_size), "size: %llx\n");
	//<>
}

/* Deploys the instrumented code of each function to an appropriate place in
 * the detour buffer. Releases the temporary buffer and sets 'i_addr' to the
 * final address of the instrumented instance. */
static void
deploy_instrumented_code(void)
{
	struct kedr_ifunc *func;
	unsigned long dest_addr;
	
	BUG_ON(detour_buffer == NULL);
	
	dest_addr = KEDR_ALIGN_VALUE(detour_buffer);
	list_for_each_entry(func, &ifuncs, list) {
		/* Small functions should have been removed from the list */
		BUG_ON(func->size < KEDR_SIZE_JMP_REL32);

		BUG_ON(func->tbuf == NULL);
		BUG_ON(func->i_addr != NULL);
		
		memcpy((void *)dest_addr, func->tbuf, func->i_size);
		kfree(func->tbuf); /* free the temporary buffer */
		func->tbuf = NULL;
		func->i_addr = (void *)dest_addr;
		
		deploy_instrumented_function(func);
		dest_addr += KEDR_ALIGN_VALUE(func->i_size);
	}
}

/* For each original function, place a jump to the instrumented instance at
 * the beginning and fill the rest with '0xcc' (breakpoint) instructions. */
static void
detour_original_functions(void)
{
	u32 *pos;
	struct kedr_ifunc *func;
	
	list_for_each_entry(func, &ifuncs, list) {
		
		/* Small functions should have been removed from the list */
		BUG_ON(func->size < KEDR_SIZE_JMP_REL32);
		
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

	deploy_instrumented_code();

	/* At this point, nothing more should fail, so we can finally 
	 * fixup the jump tables to be applicable for the fallback instances
	 * rather than for the original one. */
	list_for_each_entry(f, &ifuncs, list)
		fixup_fallback_jump_tables(f);
	
	detour_original_functions();
	return 0;
}
/* ====================================================================== */
