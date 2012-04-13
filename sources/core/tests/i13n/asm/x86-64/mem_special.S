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

.global kedr_test_mem_special_aux
.type   kedr_test_mem_special_aux,@function; 

kedr_test_mem_special_aux:
	xor %rax, %rax;
	ret;
.size kedr_test_mem_special_aux, .-kedr_test_mem_special_aux
/* ====================================================================== */

.global kedr_test_mem_special_xlat_bx
.type   kedr_test_mem_special_xlat_bx,@function; 

/* Check handling of XLAT when %rbx is %base. */
kedr_test_mem_special_xlat_bx:
	push %rbp;
	mov %rsp, %rbp;
	push %rbx;
	
	/* Make sure %rbx is chosen as %base, use other non-scratch
	 * registers somehow. */
	mov %rbp, %rax;
	mov %rax, %rbp;
	mov %rbp, %rax;
	mov %rax, %rbp;
	mov %r12, %rax;
	mov %rax, %r12;
	mov %r12, %rax;
	mov %rax, %r12;
	mov %r12, %rax;
	mov %rax, %r12;
	mov %r12, %rax;
	mov %rax, %r12;
	mov %r13, %rax;
	mov %rax, %r13;
	mov %r13, %rax;
	mov %rax, %r13;
	mov %r13, %rax;
	mov %rax, %r13;
	mov %r13, %rax;
	mov %rax, %r13;
	mov %r14, %rax;
	mov %rax, %r14;
	mov %r14, %rax;
	mov %rax, %r14;
	mov %r14, %rax;
	mov %rax, %r14;
	mov %r14, %rax;
	mov %rax, %r14;
	mov %r15, %rax;
	mov %rax, %r15;
	mov %r15, %rax;
	mov %rax, %r15;
	mov %r15, %rax;
	mov %rax, %r15;
	mov %r15, %rax;
	mov %rax, %r15;
	
	mov $kedr_test_array_ms01, %rbx;
	mov $0x2, %al;
	xlat;	

	pop %rbx;
	pop %rbp;
	ret;
.size kedr_test_mem_special_xlat_bx, .-kedr_test_mem_special_xlat_bx
/* ====================================================================== */

.global kedr_test_mem_special
.type   kedr_test_mem_special,@function; 

kedr_test_mem_special:
	push %rbx;
	
	mov $kedr_test_array_ms01, %rbx;
	mov $0x2, %al;
	xlat;
	
	xor %al, %al;
	mov $0x1, %cl;
	cmpxchg %cl, 0x8(%rbx);
	jz 1f;
	cmpxchg %cl, %al;
	cmove %rcx, %rax;
		
1:	sete 0x10(%rbx);
	cmpl $0x10, 0x4(%rbx);
	cmovge (%rbx), %rdx;
	
	mov (%rbx), %rax;
	mov 0x8(%rbx), %rcx;
	cmpxchg %rcx, 0x8(%rbx);
	jz 1f;
	mov %rax, (%rbx);

1:	xor %eax, %eax;
	movabs kedr_test_array_ms01, %al;
	movabs %al, kedr_test_array_ms01;
	movabs kedr_test_array_ms01, %rax;
	movabs %rax, kedr_test_array_ms01;

	pop %rbx;
	ret;
.size kedr_test_mem_special, .-kedr_test_mem_special
/* ====================================================================== */

.data
.align 8,0

.global kedr_test_array_ms01
.type   kedr_test_array_ms01,@object
kedr_test_array_ms01: .int 0xbee0feed, 0x12345678, 0x0, 0xabcdefab, 0x0, 0x0
.size kedr_test_array_ms01, .-kedr_test_array_ms01
/* ====================================================================== */ 
