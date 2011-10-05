/* primary_storage.h - kedr_primary_storage structure and related stuff. */

#ifndef PRIMARY_STORAGE_H_1804_INCLUDED
#define PRIMARY_STORAGE_H_1804_INCLUDED

#include <linux/kernel.h>
#include <kedr/asm/insn.h>	/* X86_REG_COUNT */

/* The data record containing information about a memory access operation */
struct kedr_mem_record
{
	/* Address of the instruction in the original function that would
	 * have made this memory access operation ("PC" - "program counter",
	 * a synonim to "instruction pointer", "IP"). 
	 * 
	 * pc == 0 indicates that this record was not used when the code 
	 * block was executed. */
	unsigned long pc;
	
	/* Start address of the accessed memory area. */
	unsigned long addr;
	
	/* Size of the accessed memory area, in bytes. */
	unsigned long size;
};

/* Maximum number of memory access operations allowed in a code block.
 * This may actually limit the size of the code blocks. */
#define KEDR_MEM_NUM_RECORDS	32

/* "Thread-local" storage for a running instrumented function. 
 * Among other things, the information about memory reads and writes in a 
 * current code block is recorded there. 
 * The spill slots for the general-purpose registers are here too. */
struct kedr_primary_storage
{
	/* Spill slots for general-purpose registers. This is the place 
	 * where the values from a register can be temporarily stored while
	 * the register is used for some other purpose. 
	 * See arch/x86/include/kedr/asm/inat.h for the list of the register
	 * codes, these are to be used as the indexes into this array.
	 * 
	 * This array is located at the beginning of the primary storage 
	 * because this allows to address the spill slots using only 8-bit
	 * offsets from the beginning of the storage even on x86-64 systems
	 * (the offsets are signed 8-bit values).
	 * The largest offset is 
	 *     sizeof(unsigned long) * (X86_REG_COUNT - 1)
	 * On x86-64, this is 8 * 15 = 120 < 127, which is the maximum 8-bit
	 * positive offset. */
	unsigned long regs[X86_REG_COUNT];
	
	/* "Thread ID", a unique number identifying the thread this storage
	 * belongs to. As far as interrupt handlers are concerned, 'tid' can
	 * be chosen in many ways, e.g. it could be the number of the CPU
	 * the handler is running on, or may be something else. */
	unsigned long tid;
	
	/* Start address of the original function. */
	unsigned long orig_func;
	
	/* The recorded memory access information. */
	struct kedr_mem_record mem_record[KEDR_MEM_NUM_RECORDS];
	
	/* The lower bits (0 .. KEDR_MEM_NUM_RECORDS-1) of the masks listed
	 * below specify whether the corresponding memory access events have
	 * a given property. If bit #i is 1, the event mem_record[i] has 
	 * this property, if 0, it does not.
	 * - read_mask  - property: a read from memory occurs;
	 * - write_mask - property: a write to memory occurs;
	 * - lock_mask  - property: the memory access operation is locked,
	 *        that is, no other access to the given memory area can take
         *        place during this operation. */
	unsigned long read_mask;
	unsigned long write_mask;
	unsigned long lock_mask;
	
	/* Destination address of a jump, used to handle jumps out of the
	 * code block. */
	unsigned long dest_addr;
	
	/* A place for temporary data. It can be handy if using a register
	 * to store these data is not desirable. */
	unsigned long temp;
};

#endif // PRIMARY_STORAGE_H_1804_INCLUDED
