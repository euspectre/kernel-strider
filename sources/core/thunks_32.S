/* thunks_*.S - thunks used when handling function calls.
 * The thunks are responsible for calling pre- and post- handlers and the 
 * replacement function for the target in the correct environment 
 * (registers, etc.). */

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

.text

#include "kedr_asm_offsets.h"

/* Within the thunks, %ebx is used to contain the address of the 
 * local storage, %ebp - the address of the call_info instance.
 * Both of these registers are non-scratch and are therefore preserved
 * by the handlers called from the thunks, which is convenient. */

#define save_sp_and_scratch_but_ax \
	mov %ecx, KEDR_LSTORAGE_cx(%ebx); \
	mov %edx, KEDR_LSTORAGE_dx(%ebx); \
	mov %esp, KEDR_LSTORAGE_sp(%ebx);
	
#define restore_scratch_but_ax \
	mov KEDR_LSTORAGE_dx(%ebx), %edx; \
	mov KEDR_LSTORAGE_cx(%ebx), %ecx;

/* ====================================================================== */

.global kedr_thunk_call
.type   kedr_thunk_call,@function; 

kedr_thunk_call:
	/* We cannot use the spill slots for %ebx and %ebp to save the
	 * values of these registers there. This is because if one of
	 * these is %base in the caller function, we would corrupt the
	 * original value of %base this way. So, use 'temp' and 'temp1'. */
	mov %ebx, KEDR_LSTORAGE_temp(%eax);
	mov %eax, %ebx;
	
	mov %ebp, KEDR_LSTORAGE_temp1(%ebx);
	mov KEDR_LSTORAGE_info(%ebx), %ebp;
	
	/* The thunk has been called with CALL instruction, so the return
	 * address of the thunk should now be on the top of the stack.
	 * Below it lies the stack that the target function might expect.
	 * So we remove the return address from the stack restoring the 
	 * latter and will place the return address back again later. */
	popl KEDR_LSTORAGE_ret_addr(%ebx);
	
	/* Now all the scratch registers except %eax have the needed 
	 * values, and so does %esp. Save their values in the respective
	 * spill slots in the local storage, because the handlers may need
	 * them and are not obliged to preserve the scratch registers.
	 * Note that the needed value of %eax is already in its slot. */
	save_sp_and_scratch_but_ax;
	
	/* The first argument is passed via %eax on x86-32, so we are 
	 * ready to call the pre-handler. */
	call *KEDR_CALL_INFO_pre_handler(%ebp);
	
	/* The values of the scratch registers might have been changed,
	 * restore them. */
	restore_scratch_but_ax;
	mov KEDR_LSTORAGE_ax(%ebx), %eax;
	
	/* Call the replacement function for the target, in the same 
	 * environment (registers, etc.) as the target would be called. */
	call *KEDR_CALL_INFO_repl(%ebp);
	
	/* Push the return address of the thunk back on the stack. We can
	 * do it here because the original value of %esp is already in the
	 * local storage in case the post-handler needs that. */
	pushl KEDR_LSTORAGE_ret_addr(%ebx); 
	
	/* Save %eax and %edx because the function might have returned 
	 * something in these registers. If it did not, their contents are
	 * unspecified, which is OK (we give no stronger guarantees for 
	 * the thunks than there are for the target function). */
	mov %eax, KEDR_LSTORAGE_ret_val(%ebx);
	mov %edx, KEDR_LSTORAGE_ret_val_high(%ebx);
	
	/* We do not need to restore the scratch registers again as the 
	 * post-handler will take their values from the local storage
	 * anyway. We only need to prepare the argument of the handler. */
	mov %ebx, %eax;
	call *KEDR_CALL_INFO_post_handler(%ebp);
	
	/* Restore %eax and %edx from the 'ret_val*' slots, they may 
	 * contain the return value of the replacement function. */
	mov KEDR_LSTORAGE_ret_val(%ebx), %eax;
	mov KEDR_LSTORAGE_ret_val_high(%ebx), %edx;
	
	/* Restore %ebx and %ebp. No need to restore more registers:
	 * %eax and %edx have already been restored, the non-scratch 
	 * registers did not change their values except %ebx and %ebp.
	 * As for the remaining scratch registers, the target itself 
	 * does not guarantee they will not change. */
	mov KEDR_LSTORAGE_temp1(%ebx), %ebp
	mov KEDR_LSTORAGE_temp(%ebx), %ebx

	ret;
.size kedr_thunk_call, .-kedr_thunk_call
/* ====================================================================== */

.global kedr_thunk_jmp
.type   kedr_thunk_jmp,@function; 

kedr_thunk_jmp:
	/* This thunk gets called via JMP instruction. All registers except
	 * %eax are restored before that and the original value of %eax is
	 * in its spill slot. Two important consequences:
	 * - unlike kedr_thunk_call(), the spill slots can now be used to
	 * store the values of %ebx and %ebp in the thunk, the saving and
	 * restoring instructions will be 3 bytes shorter;
	 * - unlike kedr_thunk_call(), no manipulations with stack are 
	 * necessary, it is already in the needed state. */
	mov %ebx, KEDR_LSTORAGE_bx(%eax)
	mov %eax, %ebx

	mov %ebp, KEDR_LSTORAGE_bp(%ebx)
	mov KEDR_LSTORAGE_info(%ebx), %ebp
	
	save_sp_and_scratch_but_ax;
	call *KEDR_CALL_INFO_pre_handler(%ebp);
	
	restore_scratch_but_ax;
	mov KEDR_LSTORAGE_ax(%ebx), %eax;
	call *KEDR_CALL_INFO_repl(%ebp);
	
	/* Save the possible return value of the function. */
	mov %eax, KEDR_LSTORAGE_ret_val(%ebx);
	mov %edx, KEDR_LSTORAGE_ret_val_high(%ebx);
	
	mov %ebx, %eax;
	call *KEDR_CALL_INFO_post_handler(%ebp);
	
	/* Restore the possible return value of the function. */
	mov KEDR_LSTORAGE_ret_val(%ebx), %eax;
	mov KEDR_LSTORAGE_ret_val_high(%ebx), %edx;
	
	/* Restore %ebp. */
	mov KEDR_LSTORAGE_bp(%ebx), %ebp;
	
	/* Now all the registers except %ebx have their needed values.
	 * Restore %ebx and call the exit handler for the function 
	 * passing the pointer to the local storage to it via %eax. 
	 * No need to bother saving the value of %edx before calling 
	 * kedr_on_function_exit_wrapper() because the wrappers preserve 
	 * all registers except %eax. */
	push  %eax;
	mov   %ebx, %eax;
	mov   KEDR_LSTORAGE_bx(%eax), %ebx;
	call  kedr_on_function_exit_wrapper;
	pop   %eax;

	ret;
.size kedr_thunk_jmp, .-kedr_thunk_jmp
/* ====================================================================== */
