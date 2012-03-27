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

/* The code below can be used to check handling of the following constructs:
 * - direct near relative call, unconditional or conditional jump out of the 
 * function;
 * - call/jump near indirect out of the function; 
 * - jump near indirect within the function (using %base and not using it).
 * 
 * [NB] The *_aux* functions should only be called from other functions 
 * defined here and are not expected to be used alone. */

.text
/* ====================================================================== */

.global kedr_test_calls_jumps2_rel32
.type   kedr_test_calls_jumps2_rel32,@function; 

kedr_test_calls_jumps2_rel32:
	push %rbp;
	push %rdi;
	push %rsi;

	call kedr_test_calls_jumps2_aux1;
	xor %rax, %rax;
	
	call 1f;
1:	pop %rax;
	
	pop %rsi;
	pop %rdi;
	pop %rbp;
	
	jmp kedr_test_calls_jumps2_aux2;
.size kedr_test_calls_jumps2_rel32, .-kedr_test_calls_jumps2_rel32
/* ====================================================================== */

.global kedr_test_calls_jumps2_jcc
.type   kedr_test_calls_jumps2_jcc,@function; 

kedr_test_calls_jumps2_jcc:
	push %rbp;
	push %rdi;
	push %rsi;

	call kedr_test_calls_jumps2_aux1;
	
	pop %rsi;
	pop %rdi;
	pop %rbp;
	
	xor %rax, %rax;
	test %rax, %rax;
	jz kedr_test_calls_jumps2_aux2;
	ret;
.size kedr_test_calls_jumps2_jcc, .-kedr_test_calls_jumps2_jcc
/* ====================================================================== */

.global kedr_test_calls_jumps2_indirect
.type   kedr_test_calls_jumps2_indirect,@function; 

kedr_test_calls_jumps2_indirect:
	push %rbp;
	push %rdi;
	push %rsi;
	push %r12;
	push %r13;
	push %r14;
	push %r15;
	
	/* Increase usage count of other non-scratch registers to make sure
	 * %rbx is chosen as %base. */
	.rept 10
	dec %rdi;
	dec %rsi;
	dec %rbp;
	dec %r12;
	dec %r13;
	dec %r14;
	dec %r15;
	.endr
		
	/* [1] Indirect calls */
	mov %rbx, %rbp;

	mov $kedr_test_calls_jumps2_aux1, %rbx;
	call *%rbx;
	
	mov $kedr_test_calls_jumps2_aux1, %rax;
	call *%rax;
	xor %rax, %rax;
	
	xor %rbx, %rbx;
	call *jump_table_out(,%rbx,8);
	
	/* [2] Indirect inner jumps */
	/* Inner indirect jump, does not use %base */
	jmp *jump_table1(,%rax,8);
	
	/* Jump #1, dest #1 */
	inc %rcx;
	jmp 1f;
	
	/* Jump #1, dest #2 */
	inc %rdx;
	jmp 1f;
	
	/* Jump #1, dest #3 */
	inc %rax;
1:
	xor %rbx, %rbx;
	jmp *jump_table2(,%rbx,8);

	/* Jump #2, dest #1 */
	mov %rbp, %rbx;
	pop %r15;
	pop %r14;
	pop %r13;
	pop %r12;
	pop %rsi;
	pop %rdi;
	pop %rbp;
	
	/* [3] Indirect jump outside */
	xor %rdx, %rdx;
	inc %rdx;
	jmp *jump_table_out(,%rdx,8);
	
	/* Each "normal" function must preserve the non-scratch registers.
	 * This way, the indirect jumps using %base must be rather
	 * uncommon (who will restore that register, anyway?). We do not 
	 * check such jumps here. 
	 * Note that the calls using %base ARE checked above. */
.size kedr_test_calls_jumps2_indirect, .-kedr_test_calls_jumps2_indirect
/* ====================================================================== */

/* This function is located right before kedr_test_calls_jumps2_aux1() to 
 * make it possible for the assembler to use a short jump here. We need to 
 * check such jumps too. The assembler, of course, is not obliged to use
 * a short jump there. */
.global kedr_test_calls_jumps2_jmp_short
.type   kedr_test_calls_jumps2_jmp_short,@function; 

kedr_test_calls_jumps2_jmp_short:
	call 1f;
1:	pop %rax;
	/* GAS refused to generate a short jump by default here, so we 
	 * do this manually. Hope the padding of the functions will not
	 * be longer than the range of a short jump. It is checked below. */
	.byte 0xeb, kedr_test_calls_jumps2_aux1-.-1;
.size kedr_test_calls_jumps2_jmp_short, .-kedr_test_calls_jumps2_jmp_short
/* ====================================================================== */

.global kedr_test_calls_jumps2_aux1
.type   kedr_test_calls_jumps2_aux1,@function; 

kedr_test_calls_jumps2_aux1:
	push %rbx;
	push %rbp;
	push %rsi;
	pop %rsi;
	pop %rbp;
	pop %rbx;
	ret;
.size kedr_test_calls_jumps2_aux1, .-kedr_test_calls_jumps2_aux1

/* Just a sanity check, a bit more restrictive than needed but simple. */
.if (kedr_test_calls_jumps2_aux1 - kedr_test_calls_jumps2_jmp_short >= 0x80)
.error "The distance between the functions is too large for JMP short."
.endif
/* ====================================================================== */

/* This function is located right before kedr_test_calls_jumps2_aux2() to 
 * make it possible for the assembler to use a short jump here. We need to 
 * check such jumps too. The assembler, of course, is not obliged to use
 * a short jump there. */
.global kedr_test_calls_jumps2_jcc_short
.type   kedr_test_calls_jumps2_jcc_short,@function; 

kedr_test_calls_jumps2_jcc_short:
	xor %rax, %rax;
	test %rax, %rax;
	/* Same notes as in *_jmp_short() above. */
	.byte 0x74, kedr_test_calls_jumps2_aux2-.-1;
	ret;
.size kedr_test_calls_jumps2_jcc_short, .-kedr_test_calls_jumps2_jcc_short
/* ====================================================================== */

.global kedr_test_calls_jumps2_aux2
.type   kedr_test_calls_jumps2_aux2,@function; 

kedr_test_calls_jumps2_aux2:
	push %rbx;
	push %rbp;
	push %rsi;
	pop %rsi;
	pop %rbp;
	pop %rbx;
	ret;
.size kedr_test_calls_jumps2_aux2, .-kedr_test_calls_jumps2_aux2

/* Just a sanity check, a bit more restrictive than needed but simple. */
.if (kedr_test_calls_jumps2_aux2 - kedr_test_calls_jumps2_jcc_short >= 0x80)
.error "The distance between the functions is too large for Jcc short."
.endif
/* ====================================================================== */

.data
.align 8,0
jump_table1: .quad \
	kedr_test_calls_jumps2_indirect+0x106, \
	kedr_test_calls_jumps2_indirect+0x10b, \
	kedr_test_calls_jumps2_indirect+0x110, \
	kedr_test_calls_jumps2_indirect+0x110

jump_table2: .quad \
	kedr_test_calls_jumps2_indirect+0x11d, \
	kedr_test_calls_jumps2_indirect+0x11d, \
	kedr_test_calls_jumps2_indirect+0x11d, \
	kedr_test_calls_jumps2_indirect+0x11d
	
jump_table_out: .quad \
	kedr_test_calls_jumps2_aux1, \
	kedr_test_calls_jumps2_aux2, \
	kedr_test_calls_jumps2_aux1, \
	kedr_test_calls_jumps2_aux1
/* ====================================================================== */
