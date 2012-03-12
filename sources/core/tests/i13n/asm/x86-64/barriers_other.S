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

.global kedr_test_barriers_other
.type   kedr_test_barriers_other,@function; 

/* [NB] Handling of CPUID is checked in a separate test - as an example 
 * of how such code snippets can be written. */
kedr_test_barriers_other:
	push %rbp;
	mov %rsp, %rbp;
	
	/* *FENCE */
	lfence;
	xor %rax, %rax;
	xor %rcx, %rcx;
	sfence;
	xor %rax, %rax;
	xor %rcx, %rcx;
	mfence;
	xor %rax, %rax;
	xor %rcx, %rcx;
	
	/* IN and OUT */
	in $0x37, %al;
	xor %rax, %rax;
	xor %rcx, %rcx;
	in $0x37, %eax;
	xor %rax, %rax;
	xor %rcx, %rcx;
	in (%dx), %al;
	xor %rax, %rax;
	xor %rcx, %rcx;
	in (%dx), %eax;
	xor %rax, %rax;
	xor %rcx, %rcx;
	
	out %al, $0x28
	xor %rax, %rax;
	xor %rcx, %rcx;
	out %eax, $0x28;
	xor %rax, %rax;
	xor %rcx, %rcx;
	out %al, (%dx);
	xor %rax, %rax;
	xor %rcx, %rcx;
	out %eax, (%dx);
	xor %rax, %rax;
	xor %rcx, %rcx;
	
	/* INVD, WBINVD, INVLPG */
	invd;
	xor %rax, %rax;
	xor %rcx, %rcx;
	
	invlpg (%rdx);
	xor %rax, %rax;
	xor %rcx, %rcx;
	
	wbinvd;
	xor %rax, %rax;
	xor %rcx, %rcx;
	
	/* MOV to CRn */
	mov %rbx, %cr2;
	mov %cr2, %rax;
	xor %rax, %rax;
	xor %rcx, %rcx;
	mov %rbx, %cr0;
	mov %cr0, %rax;
	xor %rax, %rax;
	xor %rcx, %rcx;
	
	/* x86-64 only */
	mov %rax, %cr8;
	mov %cr8, %rax;
	
	/* MOV to DRn */
	mov %rax, %dr0;
	mov %dr0, %rcx;
	
	pop %rbx;
	pop %rbp;
	ret;
.size kedr_test_barriers_other, .-kedr_test_barriers_other
