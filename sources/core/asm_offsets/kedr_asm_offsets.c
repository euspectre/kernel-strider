#include <linux/kbuild.h>
#include <linux/kernel.h>

#include <kedr/kedr_mem/functions.h>
#include <kedr/kedr_mem/local_storage.h>
#include <kedr/asm/insn.h>
/* ====================================================================== */

/* Offset of a spill slot for the given register in 
 * struct kedr_local_storage. 
 * '_reg' - see INAT_REG_CODE_* */
#define KEDR_OFFSET_LS_REG(_reg) ({ \
	BUILD_BUG_ON((unsigned int)(_reg >= X86_REG_COUNT)); \
	(unsigned int)offsetof(struct kedr_local_storage, regs) + \
	(unsigned int)((_reg) * sizeof(unsigned long)); \
})

/* This function is not intended to be executed. It is needed to provide
 * the numeric values of the needed offsets. */
void 
kedr_offsets_holder(void)
{
	/* kedr_local_storage */
	COMMENT("Offsets of the fields in struct kedr_local_storage");
	
	/* Register spill slots */
	DEFINE(KEDR_LSTORAGE_ax, KEDR_OFFSET_LS_REG(INAT_REG_CODE_AX));
	DEFINE(KEDR_LSTORAGE_cx, KEDR_OFFSET_LS_REG(INAT_REG_CODE_CX));
	DEFINE(KEDR_LSTORAGE_dx, KEDR_OFFSET_LS_REG(INAT_REG_CODE_DX));
	DEFINE(KEDR_LSTORAGE_bx, KEDR_OFFSET_LS_REG(INAT_REG_CODE_BX));
	DEFINE(KEDR_LSTORAGE_sp, KEDR_OFFSET_LS_REG(INAT_REG_CODE_SP));
	DEFINE(KEDR_LSTORAGE_bp, KEDR_OFFSET_LS_REG(INAT_REG_CODE_BP));
	DEFINE(KEDR_LSTORAGE_si, KEDR_OFFSET_LS_REG(INAT_REG_CODE_SI));
	DEFINE(KEDR_LSTORAGE_di, KEDR_OFFSET_LS_REG(INAT_REG_CODE_DI));

#ifdef CONFIG_X86_64
	DEFINE(KEDR_LSTORAGE_r8, KEDR_OFFSET_LS_REG(INAT_REG_CODE_8));
	DEFINE(KEDR_LSTORAGE_r9, KEDR_OFFSET_LS_REG(INAT_REG_CODE_9));
	DEFINE(KEDR_LSTORAGE_r10, KEDR_OFFSET_LS_REG(INAT_REG_CODE_10));
	DEFINE(KEDR_LSTORAGE_r11, KEDR_OFFSET_LS_REG(INAT_REG_CODE_11));
	DEFINE(KEDR_LSTORAGE_r12, KEDR_OFFSET_LS_REG(INAT_REG_CODE_12));
	DEFINE(KEDR_LSTORAGE_r13, KEDR_OFFSET_LS_REG(INAT_REG_CODE_13));
	DEFINE(KEDR_LSTORAGE_r14, KEDR_OFFSET_LS_REG(INAT_REG_CODE_14));
	DEFINE(KEDR_LSTORAGE_r15, KEDR_OFFSET_LS_REG(INAT_REG_CODE_15));
#endif
	/* The array of local values */
	OFFSET(KEDR_LSTORAGE_values, kedr_local_storage, values);
	
	/* Other fields */
	OFFSET(KEDR_LSTORAGE_tid, kedr_local_storage, tid);
	OFFSET(KEDR_LSTORAGE_fi, kedr_local_storage, fi);
	OFFSET(KEDR_LSTORAGE_write_mask, kedr_local_storage, write_mask);
	OFFSET(KEDR_LSTORAGE_info, kedr_local_storage, info);
	OFFSET(KEDR_LSTORAGE_dest_addr, kedr_local_storage, dest_addr);
	OFFSET(KEDR_LSTORAGE_temp, kedr_local_storage, temp);
	OFFSET(KEDR_LSTORAGE_temp1, kedr_local_storage, temp1);
	OFFSET(KEDR_LSTORAGE_ret_val, kedr_local_storage, ret_val);
	OFFSET(KEDR_LSTORAGE_ret_val_high, kedr_local_storage, ret_val_high);
	OFFSET(KEDR_LSTORAGE_ret_addr, kedr_local_storage, ret_addr);
	BLANK();
	
	/* kedr_call_info */
	COMMENT("Offsets of the fields in struct kedr_call_info");
	OFFSET(KEDR_CALL_INFO_pc, kedr_call_info, pc);
	OFFSET(KEDR_CALL_INFO_target, kedr_call_info, target);
	OFFSET(KEDR_CALL_INFO_repl, kedr_call_info, repl);
	OFFSET(KEDR_CALL_INFO_pre_handler, kedr_call_info, pre_handler);
	OFFSET(KEDR_CALL_INFO_post_handler, kedr_call_info, post_handler);
}
/* ====================================================================== */
