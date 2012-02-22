/* block_info.h - definition of the structures containing the information
 * about a block of code. */

#ifndef BLOCK_INFO_H_1607_INCLUDED
#define BLOCK_INFO_H_1607_INCLUDED

#include <linux/list.h>

/* The data record containing information (known at the instrumentation 
 * phase) about a memory access event. Note that the type of the event 
 * (read, write, update) is not stored here. It should be determined based 
 * on the masks from kedr_block_info and, in some cases, from the local 
 * storage. */
struct kedr_mem_event
{
	/* Address of the instruction in the original function that would
	 * have made this memory access operation ("PC" - "program counter",
	 * a synonym to "instruction pointer", "IP"). 
	 * Note that 'pc == 0' cannot be used now as an indicator for the 
	 * events that did not actually happen because this structure is 
	 * filled at the instrumentation phase and should be read-only in
	 * runtime. Something in the local storage should be used for that 
	 * purpose (e.g., the address of the accessed memory area). */
	unsigned long pc;
	
	/* Size of the accessed memory area, in bytes. Remains 0 for string 
	 * operations. For these, the size will be determined in runtime. */
	unsigned long size; 
};

/* Information known at the instrumentation phase about a block of code. 
 * The data known only in runtime should go to the local_storage, etc.
 * [NB] A locked operation or an I/O operation that accesses memory is
 * likely to be alone in the block (such operation a memory barrier among 
 * other things). Still, it is convenient to have the same kind of 
 * block_info structure for such blocks too. Different handlers of the end
 * of the block should be used in this case, however. */
struct kedr_block_info
{
	/* The list of such structures for a particular function. */
	struct list_head list;
	
	/* Start address of the original function. */
	unsigned long orig_func;
	
	/* The number of the elements in events[] array (see below). The 
	 * block of code may contain no more than 'max_events' operations 
	 * with memory. For most of the instructions accessing memory, each
	 * instruction counts as one such operation. Each instruction of 
	 * type XY (CMPS, MOVS) counts as two. */
	unsigned long max_events;
	
	/* The lower bits (0 .. KEDR_MAX_LOCAL_VALUES - 1) of the masks
	 * listed below specify whether the corresponding memory access
	 * events have a given property (KEDR_MAX_LOCAL_VALUES is defined
	 * in local_storage.h). 
	 * If bit #i is 1, event[i] has this property, if 0, it does not.
	 * - read_mask  - property: a read from memory occurs;
	 * - write_mask - property: a write to memory occurs; 
	 * - string_mask - property: the memory access is performed by a 
	 *   string operation. */
	u32 read_mask;
	u32 write_mask;
	u32 string_mask;
	/* [NB] 'string_mask' is needed to determine how to interpret the
	 * values in the local storage. For an ordinary memory access, the 
	 * local storage contains only the address of the accessed memory
	 * area, for a string operation, - that address and the size of the 
	 * accessed memory area. */
	
	/* [NB] An instruction of type XY (MOVS, CMPS) produces two memory
	 * events rather than just one. Therefore, 2 bits in each mask 
	 * correspond to such instruction. */
	
	/* (must be the last one in the structure) 
	 * When kedr_block_info structure is created, the allocated memory 
	 * chunk should be large enough for 'events[]' array to actually 
	 * contain 'max_events' elements. That is,
	 * alloc_size = sizeof(struct kedr_block_info) + 
	 *              (max_events - 1) * sizeof(struct kedr_mem_event).
	 * This way, 'events[]' would be able to accomodate the information
	 * about all the memory events in the block of code. */
	struct kedr_mem_event events[1];
};

#endif // BLOCK_INFO_H_1607_INCLUDED
