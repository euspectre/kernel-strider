/* This "accessor" module is used when testing IR transformation subsystem.
 * For the specified function of the target module (the name of the 
 * function is a parameter for this module), the module gets the IR for it 
 * from the core and outputs the information about it to a file in debugfs.
 *
 * [NB] This module itself does not perform any tests, it just provides data
 * for analysis in the user space. 
 *
 * The format is as follows: 
 * ("offset" for a node is (node->orig_addr - func->addr), printed as 
 * %llx, '0xadded' for the nodes added during the instrumentation)
 * -----------------------------------------------------------------------
 * 
 * IR:
 * <for each node ('node')>
 *	<If the node is the starting node of a block>
 * 		Block (type: <N (as %u)>)
 *	<If dest_inner != NULL>
 *		Jump to <offset of 'dest_inner' node>
 *	<If the instruction refers to a 'call_info' instance>
 *		Ref. to call_info for the node at <off of the node>
 *	<If the instruction refers to a 'block_info' instance>
 *		Ref. to block_info for the block at \
 *			<offset of the first reference node of the block>
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

/* A directory for the module in debugfs. */
static struct dentry *debugfs_dir_dentry = NULL;
const char *debugfs_dir_name = "test_accessor2";
/* ====================================================================== */

static unsigned long
offset_for_node(struct kedr_ifunc *func, struct kedr_ir_node *node)
{
	if (node->orig_addr == 0)
		return 0xadded; /* just in case */
		
	return (node->orig_addr - (unsigned long)func->addr);
}

static void 
print_ir_node(struct kedr_ifunc *func, struct kedr_ir_node *node, 
	struct kedr_ir_node *start)
{
	u8 buf[X86_MAX_INSN_SIZE];
	struct insn *insn = &node->insn;
	u8 *pos;
	u8 opcode;
	u8 modrm;
	int is_mov_imm_to_reg;
	
	if (node->dest_inner != NULL)
		debug_util_print_ulong(
			offset_for_node(func, node->dest_inner), 
			"Jump to 0x%lx\n");
	
	memcpy(&buf[0], &node->insn_buffer[0], X86_MAX_INSN_SIZE);
	opcode = insn->opcode.bytes[0];
	modrm = insn->modrm.bytes[0];
	
	/* Non-zero for MOV imm32/64, %reg. */
	is_mov_imm_to_reg = 
		((opcode == 0xc7 && X86_MODRM_REG(modrm) == 0) ||
		(opcode >= 0xb8 && opcode <= 0xbf));
	
	/* For the indirect near jumps using a jump table, as well as 
	 * for other instructions using similar addressing expressions
	 * we cannot determine the address of the table in advance to  
	 * prepare the expected dump properly. Let us just put 0 here. */
	if (X86_MODRM_RM(modrm) == 4 && insn->displacement.nbytes == 4) {
		/* SIB and disp32 are used. 
		 * [NB] If mod == 3, displacement.nbytes is 0. */ 
		pos = buf + insn_offset_displacement(&node->insn);
		*(u32 *)pos = 0;
	}
	else if (opcode == 0xe8 || opcode == 0xe9 ||
	    (opcode == 0x0f && 
	    (insn->opcode.bytes[1] & 0xf0) == 0x80)) {
		/* same for the relative near calls and jumps */
		pos = buf + insn_offset_immediate(insn);
		*(u32 *)pos = 0;
	}
	else if ((insn->modrm.bytes[0] & 0xc7) == 0x5) {
		/* same for the insns with IP-relative addressing (x86-64)
		 * and with plain disp32 addressing (x86-32). */
		pos = buf + insn_offset_displacement(insn);
		*(u32 *)pos = 0;
	}
#ifdef CONFIG_X86_64
	else if (start != NULL && is_mov_imm_to_reg &&
		X86_REX_W(insn->rex_prefix.value)) {
		/* MOV imm64, %reg, check if imm64 is the address of 
		 * a call_info or a block_info instance */
		u64 imm64 = ((u64)insn->immediate2.value << 32) | 
			(u64)(u32)insn->immediate1.value;
		/* [NB] insn->immediate*.value is signed by default, so we
		 * cast it to u32 here first to avoid sign extension which
		 * would lead to incorrectly calculated value of 'imm64'. */
		
		if (imm64 == (u64)(unsigned long)start->block_info) {
			debug_util_print_ulong(offset_for_node(func, start),
			"Ref. to block_info for the block at 0x%lx\n");
		}
		if (imm64 == (u64)(unsigned long)start->call_info) {
			/* 'start' should be the only reference node of the
			 * block in this case. */
			debug_util_print_ulong(offset_for_node(func, start),
			"Ref. to call_info for the node at 0x%lx\n");
		}
		
		/* Zero the immediate value anyway */
		pos = buf + insn_offset_immediate(insn);
		*(u64 *)pos = 0;
	}
#else /* x86-32 */
	else if (start != NULL && is_mov_imm_to_reg) {
		/* "MOV imm32, r/m32", check if imm32 is the address of 
		 * a call_info or a block_info instance */
		u32 imm32 = (u32)insn->immediate.value;
		if (imm32 == (u32)(unsigned long)start->block_info) {
			pos = buf + insn_offset_immediate(insn);
			*(u32 *)pos = 0;
			debug_util_print_ulong(offset_for_node(func, start),
			"Ref. to block_info for the block at 0x%lx\n");
		}
		if (imm32 == (u32)(unsigned long)start->call_info) {
			pos = buf + insn_offset_immediate(insn);
			*(u32 *)pos = 0;
			/* 'start' should be the only reference node of the
			 * block in this case. */
			debug_util_print_ulong(offset_for_node(func, start),
			"Ref. to call_info for the node at 0x%lx\n");
		}
		
		/* Zero the immediate value anyway */	
		pos = buf + insn_offset_immediate(insn);
		*(u32 *)pos = 0;
	}
#endif
	else if (start == NULL && is_mov_imm_to_reg) {
		/* MOV imm32, %rax in the entry handler. */
		pos = buf + insn_offset_immediate(insn);
		*(u32 *)pos = 0;
	}
	else if (opcode >= 0xa0 && opcode <= 0xa3) {
		/* direct offset MOV, zero the address */
		pos = buf + insn_offset_immediate(insn);
		*(unsigned long *)pos = 0;
	}
	
	debug_util_print_ulong(offset_for_node(func, node), "0x%lx: ");
	debug_util_print_hex_bytes(&buf[0], insn->length);
	debug_util_print_string("\n\n");
}

/* Prints the group of nodes for a given reference node, i.e the nodes 
 * from ref_node->first to ref_node->last. 
 * Updates the pointer to the start of the current block ('*pstart'). */
static void 
print_ir_node_group(struct kedr_ifunc *func, struct kedr_ir_node *ref_node, 
	struct kedr_ir_node **pstart)
{
	struct list_head *pos;
	struct kedr_ir_node *node;
	
	if (ref_node->block_starts) {
		*pstart = ref_node;
		debug_util_print_ulong((unsigned long)ref_node->cb_type, 
			"Block (type: %lu)");
		debug_util_print_string("\n");
	}
	
	for (pos = &ref_node->first->list; pos != ref_node->last->list.next;
		pos = pos->next) {
		node = list_entry(pos, struct kedr_ir_node, list);
		print_ir_node(func, node, *pstart);
	}
}

static void 
test_on_ir_transformed(struct kedr_core_hooks *hooks, 
	struct kedr_i13n *i13n, struct kedr_ifunc *func, 
	struct list_head *ir)
{
	struct kedr_ir_node *node;
	struct kedr_ir_node *start = NULL;
		
	if (strcmp(target_function, func->name) != 0)
		return;
	
	debug_util_print_string("IR:\n");
	
	/* Print the entry nodes first (they do not belong to any group). */
	list_for_each_entry(node, ir, list) {
		if (node->orig_addr != 0)
			break;
		print_ir_node(func, node, NULL);
	}
	
	/* Print the groups of nodes. */
	list_for_each_entry(node, ir, list) {
		if (node->orig_addr == 0)
			continue;
		print_ir_node_group(func, node, &start);
	}
}

struct kedr_core_hooks test_hooks = {
	.owner = THIS_MODULE,
	.on_ir_transformed = test_on_ir_transformed,
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
