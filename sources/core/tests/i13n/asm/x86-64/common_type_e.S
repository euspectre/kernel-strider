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

.global kedr_test_common_type_e_aux
.type   kedr_test_common_type_e_aux,@function; 

kedr_test_common_type_e_aux:
	xor %rax, %rax;
	ret;
.size kedr_test_common_type_e_aux, .-kedr_test_common_type_e_aux
/* ====================================================================== */

.global kedr_test_common_type_e
.type   kedr_test_common_type_e,@function; 

kedr_test_common_type_e:
/* Common block #1 (only inner forward jumps) */
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
		
	sub $0x10, %rsp;
	xor %rax, %rax;
	mov %rax, 0x8(%rsp);
	mov $kedr_test_array_cte01, %rdx;
	test %rax, %rax;
	/* A jump within the block. */
	jz 1f;
	pushq 0x8(%rdx,%rax,4);
	pop %rcx;

1:	mov %rdx, %rbx;
	incl 0x8(%rbx,%rax,4);
	
	call kedr_test_common_type_e_aux;

/* Common block #2 (no jumps at all) */	
	mov $kedr_test_array_cte01, %rdx;
	mov kedr_test_array_cte01, %rcx; /* Memory access! */
	lea 0x4(%rdx), %rcx;
	mov (%rcx), %rax;
	mov %rax, (%rsp);

	call kedr_test_common_type_e_aux;

/* Common block #3 (no memory operations; has jumps out) */
	test %rax, %rax;
	jnz 1f;
	
	call kedr_test_common_type_e_aux;

/* Common block #4 (has jumps out) */
	test %rax, %rax;
	jnz 1f;
	mov $kedr_test_array_cte01, %rdx;
	test %rdx, %rdx;
	jz 2f;
	mov (%rdx), %rax;
2:	test %rax, %rax;
	jmp 1f;
	decl (%rdx);
	
	call kedr_test_common_type_e_aux;

/* The blocks #5 and #6 are here to check if the block end handler
 * clears 'dest_addr' in the local storage. If it does not, after the
 * end of the block is reached, a jump will be made to label "1b" 
 * (i.e. to "test %rax, %rax") and the test will hang. */

/* Common block #5 (has jumps out) */	
1:	test %rax, %rax;
	jmp 3f;
	jz 2f;
	
	/* Reading the first two bytes of kedr_test_common_type_e_aux()
	 * using RIP-relative addressing. */
3:	movw kedr_test_common_type_e_aux(%rip), %cx; 
	
	call kedr_test_common_type_e_aux;

/* Common block #6 (no jumps at all) */
2:	incl (%rdx);
	
1:	add $0x10, %rsp;
	pop %rbx;
	pop %rbp;
	ret;
.size kedr_test_common_type_e, .-kedr_test_common_type_e
/* ====================================================================== */

.data
.align 8,0

.global kedr_test_array_cte01
.type   kedr_test_array_cte01,@object
kedr_test_array_cte01: .int 0xbee0feed, 0x12345678, 0x0, 0xabcdefab, 0x0, 0x0
.size kedr_test_array_cte01, .-kedr_test_array_cte01
/* ====================================================================== */ 
