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
/* ====================================================================== */

.global kedr_test_strings
.type   kedr_test_strings,@function; 

kedr_test_strings:
	push %rsi;
	push %rdi;

	movw kedr_test_strings01, %dx;
	mov $kedr_test_strings01, %rax;
	mov $kedr_test_strings03, %rdx;
	mov %rax, %rsi;
	mov %rdx, %rdi;
	
	cld;
	mov $0x28, %rcx;
	rep movsb;
	
	/* Make sure a string insn is located at the end of the block,
	 * a special case that should also be checked (nodes will be
	 * added after the last reference node of the block during the 
	 * process). */
	call kedr_test_strings_aux;

	/* kedr_test_strings_aux() does not currently change the values
	 * of the scratch registers but the calling convention does not
	 * require that. Our handlers of the function calls also may 
	 * change scratch registers. As %rsi and %rdi are such registers
	 * on x86-64, we need to save/restore them explicitly. 
	 * PUSH/POP cannot be used because we need to keep the above call
	 * immediately after REP MOVSB for the latter to be the last insn
	 * in that block. */
	mov $kedr_test_strings01, %rax;
	mov $kedr_test_strings03, %rdx;
	lea 0x28(%rax), %rsi; /* %rsi := %rax + 0x28 */
	lea 0x28(%rdx), %rdi; /* %rdi := %rdx + 0x28 */
		
	std;
	mov $0x4, %rcx;
	/* CMPS will process the strings from the end (DF=1) */
	repe cmpsl;
	cld;
	mov %rax, %rsi;
	mov %rdx, %rdi;
	je 1f;
	xor %rcx, %rcx;

1: 	lodsw;
	stosb;
	mov kedr_test_strings02(%rip), %rax;
	xor %rax, %rax;
	scasq;

	pop %rdi;
	pop %rsi;
	ret;
.size kedr_test_strings, .-kedr_test_strings
/* ====================================================================== */

.global kedr_test_strings_aux
.type   kedr_test_strings_aux,@function; 

kedr_test_strings_aux:
	ret;
.size kedr_test_strings_aux, .-kedr_test_strings_aux
/* ====================================================================== */

.data
.align 8,0
kedr_test_strings01: .asciz "A quick brown fox jumps over a lazy dog."
kedr_test_strings02: .asciz "Pack my box with five dozen liquor jugs."
kedr_test_strings03: .asciz "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
/* ====================================================================== */
