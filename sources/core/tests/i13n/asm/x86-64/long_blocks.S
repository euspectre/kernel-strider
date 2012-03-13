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

/* This code is not intended to be executed. It is only provided to test
 * the instrumentation system on it. */

.text

.global kedr_test_long_blocks
.type   kedr_test_long_blocks,@function; 

kedr_test_long_blocks:
	push %rbp;
	mov %rsp, %rbp;
	
	/* A sequence w/o string operations that should be split in 4
	 * blocks. */
	xor %rdx, %rdx;
	add (%rax), %rdx; 
	push (%rcx, %rax, 8);
	movb %cl, (%rcx);
	addw (%rcx), %ax; /* #4 */
	
	.rept 14
	xor %rcx, %rcx;
	add (%rax), %rdx;
	add (%rax), %rdx;
	add (%rax), %rdx;
	add (%rax), %rdx; 
	.endr /* #60 */
	
	xor %rcx, %rcx;
	pop (%rdx);
	add (%rax), %rdx;
	add (%rax), %rdx;
	rep lodsb; /* values #64 and #65 */

	xor %rcx, %rcx;
	add (%rax), %rdx;
	rep stosw; /* values #67 and #68 */
	
	.rept 6
	xor %rcx, %rcx;
	add (%rax), %rdx;
	add (%rax), %rdx;
	add (%rax), %rdx;
	add (%rax), %rdx; 
	.endr /* #92 */
	
	add (%rax), %rdx; /* #93 */
	
	movsq /* values #94-97 */
	add (%rax), %rdx;
	movsl
	
	pop %rbx;
	pop %rbp;
	ret;
.size kedr_test_long_blocks, .-kedr_test_long_blocks
