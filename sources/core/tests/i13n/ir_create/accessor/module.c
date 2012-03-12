/* This "accessor" module is used when testing IR creation subsystem of the 
 * core. For the specified function of the target module (the name of the 
 * function is a parameter for this module), the module gets the IR for it 
 * from the core and outputs the information about it to a file in debugfs.
 *
 * [NB] This module itself does not perform any tests, it just provides data
 * for analysis in the user space. 
 *
 * The format is as follows: 
 * ("offset" for a node is (node->orig_addr - func->addr), printed as 
 * %llx)
 * -----------------------------------------------------------------------
 * 
 * <If the function has jump tables, for each jump table, in order>
 * 	<If the jump table #N is not empty>
 *		JTable <N> (referrer: <offset>): <dest. offsets>
 *		<The offsets are of the destination nodes. Separator: ", ".>
 * IR:
 * <for each node ('node')>
 *	<If the node is the starting node of a block>
 * 		Block (type: <N (as %u)>) [, has jumps out]
 *		<If there is block_info>
 *			Function difference: <N>
 *			<N is block_info::orig_func - func->addr>
 *			max_events = <...>
 *			read_mask = <...>
 *			write_mask = <...>
 *			string_mask = <...>
 *			events: 
 *			  (pc_offset1, size1) 
 *			[  (pc_offset2, size2) ...]
 *	<If the insn is a barrier>
 *		Barrier of type <N>
 *	<If dest_inner != NULL>
 *		Jump to <offset of 'dest_inner' node>
 * 	<if it is a jump out of the common block with tracked memory events>
 *		[Jump out of block]
 *	<Node offset>: <Hex repr. of the insn in the node, space-sep. bytes>
 *		
 * -----------------------------------------------------------------------
 */

/* ========================================================================
 * Copyright (C) 2012, KEDR development team
 * Authors: 
 *      Eugene A. Shatokhin <spectre@ispras.ru>
 *      Andrey V. Tsyvarev  <tsyvarev@ispras.ru>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published
 * by the Free Software Foundation.
 ======================================================================== */
 
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/init.h>
#include <linux/errno.h>
#include <linux/kernel.h>
#include <linux/string.h>
#include <linux/slab.h>
#include <linux/debugfs.h>
#include <linux/list.h>

#include <kedr/kedr_mem/block_info.h>
#include <kedr/asm/insn.h>
#include <kedr/object_types.h>

#include "config.h"
#include "core_impl.h"

#include "debug_util.h"
#include "hooks.h"
#include "i13n.h"
#include "ir.h"
#include "ifunc.h"

/* ====================================================================== */
MODULE_AUTHOR("Eugene A. Shatokhin");
MODULE_LICENSE("GPL");
/* ====================================================================== */

/* Name of the function to dump information for. */
char *target_function = "";
module_param(target_function, charp, S_IRUGO);
/* ====================================================================== */

/* A directory for the core in debugfs. */
static struct dentry *debugfs_dir_dentry = NULL;
const char *debugfs_dir_name = "test_ir_create";
/* ====================================================================== */

static unsigned long
offset_for_node(struct kedr_ifunc *func, struct kedr_ir_node *node)
{
	if (node->orig_addr == 0)
		return (unsigned long)(-1); /* just in case */
		
	return (node->orig_addr - (unsigned long)func->addr);
}

static void
print_jump_tables(struct kedr_ifunc *func)
{
	struct kedr_jtable *pos;
	unsigned int i;
	unsigned int n = 0;
	
	list_for_each_entry(pos, &func->jump_tables, list) {
		if (pos->num == 0)
			continue;
		
		debug_util_print_ulong((unsigned long)n, "JTable %lu ");
		debug_util_print_ulong(offset_for_node(func, pos->referrer),
			"(referrer at 0x%lx)");
		debug_util_print_string(": ");
		
		for (i = 0; i < pos->num; ++i) {
			struct kedr_ir_node *dest = 
				(struct kedr_ir_node *)pos->i_table[i];
			if (i > 0)
				debug_util_print_string(", ");
			debug_util_print_ulong(offset_for_node(func, dest),
				"0x%lx");
		}
		debug_util_print_string("\n");
		++n;
	}
	
	if (n > 0)
		debug_util_print_string("\n");
}

static void 
print_ir_block(struct kedr_ifunc *func, struct kedr_ir_node *node)
{
	struct kedr_block_info *bi;
	unsigned long i;
	
	debug_util_print_ulong((unsigned long)node->cb_type, 
		"Block (type: %lu)");
	if (node->block_has_jumps_out)
		debug_util_print_string(", has jumps out");
	debug_util_print_string("\n");
	
	if (node->block_info == NULL)
		return;
	
	bi = node->block_info;
	debug_util_print_string("Block info:\n");
	debug_util_print_ulong(bi->orig_func - (unsigned long)func->addr,
		"Function difference: %lu\n");
	
	debug_util_print_ulong(bi->max_events, "max_events = %lu\n");
	debug_util_print_ulong(bi->read_mask, "read_mask = 0x%lx\n");
	debug_util_print_ulong(bi->write_mask, "write_mask = 0x%lx\n");
	debug_util_print_ulong(bi->string_mask, "string_mask = 0x%lx\n");
	
	debug_util_print_string("events:\n");
	for (i = 0; i < bi->max_events; ++i) {
		debug_util_print_ulong(
			bi->events[i].pc - (unsigned long)func->addr, 
			"  (0x%lx, ");
		debug_util_print_ulong(bi->events[i].size, "%lu)\n");
	}
}

static void 
test_on_ir_created(struct kedr_core_hooks *hooks, struct kedr_i13n *i13n, 
	struct kedr_ifunc *func, struct list_head *ir)
{
	struct kedr_ir_node *node;
	u8 buf[X86_MAX_INSN_SIZE];
	struct insn *insn;
	u8 *pos;
	u8 opcode;
	
	if (strcmp(target_function, func->name) != 0)
		return;
	
	print_jump_tables(func);
	debug_util_print_string("IR:\n");
	
	list_for_each_entry(node, ir, list) {
		if (node->block_starts)
			print_ir_block(func, node);
		if (node->dest_inner != NULL)
			debug_util_print_ulong(
				offset_for_node(func, node->dest_inner), 
				"Jump to 0x%lx\n");
		if (node->jump_past_last)
			debug_util_print_string("Jump out of block\n");
		
		if (node->cb_type == KEDR_CB_LOCKED_UPDATE ||
		    node->cb_type == KEDR_CB_IO_MEM_OP ||
		    node->cb_type == KEDR_CB_BARRIER_OTHER) {
			debug_util_print_ulong(
				(unsigned long)node->barrier_type,
				"Barrier of type %lu\n");
		}
		
		debug_util_print_ulong(offset_for_node(func, node), 
			"0x%lx: ");
		memcpy(&buf[0], &node->insn_buffer[0], X86_MAX_INSN_SIZE);
		insn = &node->insn;
		opcode = insn->opcode.bytes[0];
		
		/* For the indirect near jumps using a jump table, we cannot
		 * determine the address of the table in advance to prepare 
		 * the expected dump properly. Let us just put 0 both here
		 * and in the expected dump. */
		if (opcode == 0xff && insn->modrm.bytes[0] == 0x24 && 
		    X86_SIB_BASE(insn->sib.value) == 5) {
			pos = buf + insn_offset_displacement(&node->insn);
			*(u32 *)pos = 0;
		}
		else if (opcode == 0xe8 || opcode == 0xe9 ||
		    (opcode == 0x0f && 
		    (insn->opcode.bytes[1] & 0xf0) == 0x80)) {
			/* same for the relative near calls and jumps */
			pos = buf + insn_offset_immediate(&node->insn);
			*(u32 *)pos = 0;
		}
		else if (insn->x86_64 && 
		    (insn->modrm.bytes[0] & 0xc7) == 0x5) {
			/* same for the insns with IP-rel. addressing */
			pos = buf + insn_offset_displacement(&node->insn);
			*(u32 *)pos = 0;
		}
		
		debug_util_print_hex_bytes(&buf[0], node->insn.length);
		debug_util_print_string("\n\n");
	}
}

struct kedr_core_hooks test_hooks = {
	.owner = THIS_MODULE,
	.on_ir_created = test_on_ir_created,
};
/* ====================================================================== */

static void __exit
test_cleanup_module(void)
{
	kedr_set_core_hooks(NULL);
	debug_util_fini();
	debugfs_remove(debugfs_dir_dentry);
	return;
}

static int __init
test_init_module(void)
{
	int ret = 0;
	
	debugfs_dir_dentry = debugfs_create_dir(debugfs_dir_name, NULL);
	if (debugfs_dir_dentry == NULL) {
		pr_warning(KEDR_MSG_PREFIX
			"failed to create a directory in debugfs\n");
		ret = -EINVAL;
		goto out;
	}
	if (IS_ERR(debugfs_dir_dentry)) {
		pr_warning(KEDR_MSG_PREFIX "debugfs is not supported\n");
		ret = -ENODEV;
		goto out;
	}

	ret = debug_util_init(debugfs_dir_dentry);
	if (ret != 0)
		goto out_rmdir;
	
	kedr_set_core_hooks(&test_hooks);
	return 0;

out_rmdir:
	debugfs_remove(debugfs_dir_dentry);
out:
	return ret;	
}

module_init(test_init_module);
module_exit(test_cleanup_module);
/* ====================================================================== */
