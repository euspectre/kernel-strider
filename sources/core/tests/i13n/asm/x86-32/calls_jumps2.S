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
	push %ebp;
	push %edi;
	push %esi;

	call kedr_test_calls_jumps2_aux1;
	xor %eax, %eax;
	
	call 1f;
1:	pop %eax;
	
	pop %esi;
	pop %edi;
	pop %ebp;
	
	jmp kedr_test_calls_jumps2_aux2;
.size kedr_test_calls_jumps2_rel32, .-kedr_test_calls_jumps2_rel32
/* ====================================================================== */

.global kedr_test_calls_jumps2_jcc
.type   kedr_test_calls_jumps2_jcc,@function; 

kedr_test_calls_jumps2_jcc:
	push %ebp;
	push %edi;
	push %esi;

	call kedr_test_calls_jumps2_aux1;
	
	pop %esi;
	pop %edi;
	pop %ebp;
	
	xor %eax, %eax;
	test %eax, %eax;
	jz kedr_test_calls_jumps2_aux2;
	ret;
.size kedr_test_calls_jumps2_jcc, .-kedr_test_calls_jumps2_jcc
/* ====================================================================== */

.global kedr_test_calls_jumps2_indirect
.type   kedr_test_calls_jumps2_indirect,@function; 

kedr_test_calls_jumps2_indirect:
	push %ebp;
	push %edi;
	push %esi;
	
	/* Increase usage count of other non-scratch registers to make sure
	 * %ebx is chosen as %base. */
	.rept 10
	dec %edi;
	dec %esi;
	dec %ebp;
	.endr
		
	/* [1] Indirect calls */
	mov %ebx, %ebp;

	mov $kedr_test_calls_jumps2_aux1, %ebx;
	call *%ebx;
	
	mov $kedr_test_calls_jumps2_aux1, %eax;
	call *%eax;
	xor %eax, %eax;
	
	xor %ebx, %ebx;
	call *jump_table_out(,%ebx,4);
	
	/* [2] Indirect inner jumps */
	/* Inner indirect jump, does not use %base */
	jmp *jump_table1(,%eax,4);
	
	/* Jump #1, dest #1 */
	inc %ecx;
	jmp 1f;
	
	/* Jump #1, dest #2 */
	inc %edx;
	jmp 1f;
	
	/* Jump #1, dest #3 */
	inc %eax;
1:
	xor %ebx, %ebx;
	jmp *jump_table2(,%ebx,4);

	/* Jump #2, dest #1 */
	mov %ebp, %ebx;
	pop %esi;
	pop %edi;
	pop %ebp;
	
	/* [3] Indirect jump outside */
	xor %edx, %edx;
	inc %edx;
	jmp *jump_table_out(,%edx,4);
	
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
1:	pop %eax;
	/* GAS refused to generate a short jump by default here, so we 
	 * do this manually. Hope the padding of the functions will not
	 * be longer than the range of a short jump. It is checked below. */
	.byte 0xeb, kedr_test_calls_jumps2_aux1-.-1;
.size kedr_test_calls_jumps2_jmp_short, .-kedr_test_calls_jumps2_jmp_short
/* ====================================================================== */

.global kedr_test_calls_jumps2_aux1
.type   kedr_test_calls_jumps2_aux1,@function; 

kedr_test_calls_jumps2_aux1:
	push %ebx;
	push %ebp;
	push %esi;
	pop %esi;
	pop %ebp;
	pop %ebx;
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
	xor %eax, %eax;
	test %eax, %eax;
	/* Same notes as in *_jmp_short() above. */
	.byte 0x74, kedr_test_calls_jumps2_aux2-.-1;
	ret;
.size kedr_test_calls_jumps2_jcc_short, .-kedr_test_calls_jumps2_jcc_short
/* ====================================================================== */

.global kedr_test_calls_jumps2_aux2
.type   kedr_test_calls_jumps2_aux2,@function; 

kedr_test_calls_jumps2_aux2:
	push %ebx;
	push %ebp;
	push %esi;
	pop %esi;
	pop %ebp;
	pop %ebx;
	ret;
.size kedr_test_calls_jumps2_aux2, .-kedr_test_calls_jumps2_aux2

/* Just a sanity check, a bit more restrictive than needed but simple. */
.if (kedr_test_calls_jumps2_aux2 - kedr_test_calls_jumps2_jcc_short >= 0x80)
.error "The distance between the functions is too large for Jcc short."
.endif
/* ====================================================================== */

.data
.align 8,0
jump_table1: .long \
	kedr_test_calls_jumps2_indirect+0x43, \
	kedr_test_calls_jumps2_indirect+0x46, \
	kedr_test_calls_jumps2_indirect+0x49, \
	kedr_test_calls_jumps2_indirect+0x49

jump_table2: .long \
	kedr_test_calls_jumps2_indirect+0x53, \
	kedr_test_calls_jumps2_indirect+0x53, \
	kedr_test_calls_jumps2_indirect+0x53, \
	kedr_test_calls_jumps2_indirect+0x53
	
jump_table_out: .long \
	kedr_test_calls_jumps2_aux1, \
	kedr_test_calls_jumps2_aux2, \
	kedr_test_calls_jumps2_aux1, \
	kedr_test_calls_jumps2_aux1
/* ====================================================================== */
