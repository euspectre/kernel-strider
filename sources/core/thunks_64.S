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

/* Within the thunks, %rbx is used to contain the address of the 
 * local storage, %rbp - the address of the call_info instance.
 * Both of these registers are non-scratch and are therefore preserved
 * by the handlers called from the thunks, which is convenient. */

#define save_sp_and_scratch_but_ax \
	mov %rcx, KEDR_LSTORAGE_cx(%rbx); \
	mov %rdx, KEDR_LSTORAGE_dx(%rbx); \
	mov %rsi, KEDR_LSTORAGE_si(%rbx); \
	mov %rdi, KEDR_LSTORAGE_di(%rbx); \
	mov %r8, KEDR_LSTORAGE_r8(%rbx); \
	mov %r9, KEDR_LSTORAGE_r9(%rbx); \
	mov %r10, KEDR_LSTORAGE_r10(%rbx); \
	mov %r11, KEDR_LSTORAGE_r11(%rbx); \
	mov %rsp, KEDR_LSTORAGE_sp(%rbx);
	
#define restore_scratch_but_ax \
	mov KEDR_LSTORAGE_r11(%rbx), %r11; \
	mov KEDR_LSTORAGE_r10(%rbx), %r10; \
	mov KEDR_LSTORAGE_r9(%rbx), %r9; \
	mov KEDR_LSTORAGE_r8(%rbx), %r8; \
	mov KEDR_LSTORAGE_di(%rbx), %rdi; \
	mov KEDR_LSTORAGE_si(%rbx), %rsi; \
	mov KEDR_LSTORAGE_dx(%rbx), %rdx; \
	mov KEDR_LSTORAGE_cx(%rbx), %rcx;

/* ====================================================================== */

.global kedr_thunk_call
.type   kedr_thunk_call,@function; 

kedr_thunk_call:
	/* We cannot use the spill slots for %rbx and %rbp to save the
	 * values of these registers there. This is because if one of
	 * these is %base in the caller function, we would corrupt the
	 * original value of %base this way. So, use 'temp' and 'temp1'. */
	mov %rbx, KEDR_LSTORAGE_temp(%rax);
	mov %rax, %rbx;
	
	mov %rbp, KEDR_LSTORAGE_temp1(%rbx);
	mov KEDR_LSTORAGE_info(%rbx), %rbp;
	
	/* The thunk has been called with CALL instruction, so the return
	 * address of the thunk should now be on the top of the stack.
	 * Below it lies the stack that the target function might expect.
	 * So we remove the return address from the stack restoring the 
	 * latter and will place the return address back again later. */
	popq KEDR_LSTORAGE_ret_addr(%rbx);
	
	/* Now all the scratch registers except %rax have the needed 
	 * values, and so does %rsp. Save their values in the respective
	 * spill slots in the local storage, because the handlers may need
	 * them and are not obliged to preserve the scratch registers.
	 * Note that the needed value of %rax is already in its slot. */
	save_sp_and_scratch_but_ax;
	
	/* The first argument is passed via %rdi on x86-64. */
	mov %rax, %rdi;
	call *KEDR_CALL_INFO_pre_handler(%rbp);
	
	/* The values of the scratch registers might have been changed,
	 * restore them. */
	restore_scratch_but_ax;
	mov KEDR_LSTORAGE_ax(%rbx), %rax;
	
	/* Call the replacement function for the target, in the same 
	 * environment (registers, etc.) as the target would be called. */
	call *KEDR_CALL_INFO_repl(%rbp);
	
	/* Push the return address of the thunk back on the stack. We can
	 * do it here because the original value of %rsp is already in the
	 * local storage in case the post-handler needs that. */
	pushq KEDR_LSTORAGE_ret_addr(%rbx); 
	
	/* Save %rax and %rdx because the function might have returned 
	 * something in these registers. If it did not, their contents are
	 * unspecified, which is OK (we give no stronger guarantees for 
	 * the thunks than there are for the target function). */
	mov %rax, KEDR_LSTORAGE_ret_val(%rbx);
	mov %rdx, KEDR_LSTORAGE_ret_val_high(%rbx);
	
	/* We do not need to restore the scratch registers again as the 
	 * post-handler will take their values from the local storage
	 * anyway. We only need to prepare the argument of the handler. */
	mov %rbx, %rdi;
	call *KEDR_CALL_INFO_post_handler(%rbp);
	
	/* Restore %rax and %rdx from the 'ret_val*' slots, they may 
	 * contain the return value of the replacement function. */
	mov KEDR_LSTORAGE_ret_val(%rbx), %rax;
	mov KEDR_LSTORAGE_ret_val_high(%rbx), %rdx;

	/* It seems that the calls to the special Ftrace functions
	 * like mcount(), etc., give stronger guarantees w.r.t.
	 * preserving the registers. Namely, some of these functions
	 * preserve all the registers that can be used for parameter
	 * transfer (or even all?) and the compiler takes that into
	 * account. To avoid problems, restore the values of such 
	 * scratch registers that have not been restored yet.
	 * Note that %rax and %rdx have the desired values already. */
#ifdef CONFIG_FUNCTION_TRACER
	mov KEDR_LSTORAGE_r11(%rbx), %r11;
	mov KEDR_LSTORAGE_r10(%rbx), %r10;
	mov KEDR_LSTORAGE_r9(%rbx), %r9;
	mov KEDR_LSTORAGE_r8(%rbx), %r8;
	mov KEDR_LSTORAGE_di(%rbx), %rdi;
	mov KEDR_LSTORAGE_si(%rbx), %rsi;
	mov KEDR_LSTORAGE_cx(%rbx), %rcx;
#endif
	
	/* Restore %rbx and %rbp. Among the remaining registers,
	 * %rax and %rdx have already been restored, the non-scratch 
	 * registers did not change their values except %rbx and %rbp.
	 * As for the remaining scratch registers, the target itself 
	 * does not guarantee they will not change except if it is
	 * mcount() or something similar from Ftrace. */
	mov KEDR_LSTORAGE_temp1(%rbx), %rbp
	mov KEDR_LSTORAGE_temp(%rbx), %rbx

	ret;
.size kedr_thunk_call, .-kedr_thunk_call
/* ====================================================================== */

.global kedr_thunk_jmp
.type   kedr_thunk_jmp,@function; 

kedr_thunk_jmp:
	/* This thunk gets called via JMP instruction. All registers except
	 * %rax are restored before that and the original value of %rax is
	 * in its spill slot. An important consequence:
	 * - unlike kedr_thunk_call(), the spill slots can now be used to
	 * store the values of %rbx and %rbp in the thunk, the saving and
	 * restoring instructions will be 3 bytes shorter. */
	mov %rbx, KEDR_LSTORAGE_bx(%rax)
	mov %rax, %rbx

	mov %rbp, KEDR_LSTORAGE_bp(%rbx)
	mov KEDR_LSTORAGE_info(%rbx), %rbp
	
	/* The thunk has been called using JMP instruction, so the return
	 * address for the function the thunk has been called from 
	 * ("caller") should now be on the top of the stack.
	 * Below it lies the stack that the target function might expect.
	 * The target/replacement function will be called with CALL rather
	 * than with JMP instruction. To make sure the callee will find
	 * the stack-based arguments exactly where it expects them, we 
	 * need to remove the return address from the stack here and 
	 * place it there back again after the callee exits. */
	popq KEDR_LSTORAGE_ret_addr(%rbx);
	
	/* [NB] If the target receives some of its arguments on stack, the
	 * saved value of %rsp will point to the first of such arguments. */
	save_sp_and_scratch_but_ax;
	mov %rax, %rdi;
	call *KEDR_CALL_INFO_pre_handler(%rbp);
	
	restore_scratch_but_ax;
	mov KEDR_LSTORAGE_ax(%rbx), %rax;
	
	call *KEDR_CALL_INFO_repl(%rbp);
	
	pushq KEDR_LSTORAGE_ret_addr(%rbx); 
	
	/* Save the possible return value of the function. */
	mov %rax, KEDR_LSTORAGE_ret_val(%rbx);
	mov %rdx, KEDR_LSTORAGE_ret_val_high(%rbx);
	
	mov %rbx, %rdi;
	call *KEDR_CALL_INFO_post_handler(%rbp);
	
	/* Restore the possible return value of the function. */
	mov KEDR_LSTORAGE_ret_val(%rbx), %rax;
	mov KEDR_LSTORAGE_ret_val_high(%rbx), %rdx;
	
	/* Restore %rbp. */
	mov KEDR_LSTORAGE_bp(%rbx), %rbp;
	
#ifdef CONFIG_FUNCTION_TRACER
	mov KEDR_LSTORAGE_r11(%rbx), %r11;
	mov KEDR_LSTORAGE_r10(%rbx), %r10;
	mov KEDR_LSTORAGE_r9(%rbx), %r9;
	mov KEDR_LSTORAGE_r8(%rbx), %r8;
	mov KEDR_LSTORAGE_di(%rbx), %rdi;
	mov KEDR_LSTORAGE_si(%rbx), %rsi;
	mov KEDR_LSTORAGE_cx(%rbx), %rcx;
#endif
	
	/* Now all the registers except %rbx have their needed values.
	 * Restore %rbx and call the exit handler for the function 
	 * passing the pointer to the local storage to it via %rax. 
	 * No need to bother saving the value of %rdx before calling 
	 * kedr_on_function_exit_wrapper() because the wrappers preserve 
	 * all registers except %rax. */
	push  %rax;
	mov   %rbx, %rax;
	mov   KEDR_LSTORAGE_bx(%rax), %rbx;
	call  kedr_on_function_exit_wrapper;
	pop   %rax;

	ret;
.size kedr_thunk_jmp, .-kedr_thunk_jmp
/* ====================================================================== */
