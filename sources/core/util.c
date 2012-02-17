/* util.c - convenience functions and other utility stuff */

#include <linux/kernel.h>
#include <linux/errno.h>

#include "util.h"

int 
kedr_for_each_insn(unsigned long start_addr, unsigned long end_addr,
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

int
kedr_for_each_insn_in_function(struct kedr_ifunc *func, 
	int (*proc)(struct kedr_ifunc *, struct insn *, void *), 
	void *data)
{
	unsigned long start_addr = (unsigned long)func->addr;
	struct data_for_each_insn_in_function data_container;
	
	data_container.func = func;
	data_container.data = data;
	data_container.proc = proc;
	
	return kedr_for_each_insn(start_addr, 
		start_addr + func->size, 
		proc_for_each_insn_in_function,
		&data_container);
}
/* ====================================================================== */

u8 
kedr_choose_register(unsigned int mask_choose_from, unsigned int mask_used)
{
	unsigned int mask;
	u8 rcode = 0;
	
	BUG_ON((mask_choose_from & ~X86_REG_MASK_ALL) != 0);
	BUG_ON((mask_used & ~X86_REG_MASK_ALL) != 0);
	
	/* N.B. Both masks have their higher bits zeroed => so will 'mask'*/
	mask = mask_choose_from & ~mask_used;
	if (mask == 0)
		return KEDR_REG_NONE; /* nothing found */
	
	while (mask % 2 == 0) {
		mask >>= 1;
		++rcode;
	}
	return rcode;
}
/* ====================================================================== */
