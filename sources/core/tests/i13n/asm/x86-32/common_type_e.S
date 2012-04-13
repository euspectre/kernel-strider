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

.global kedr_test_common_type_e
.type   kedr_test_common_type_e,@function; 

kedr_test_common_type_e:
/* Common block #1 (only inner forward jumps) */
	push %ebp;
	mov %esp, %ebp;
	push %ebx;
	
	/* Make sure %ebx is chosen as %base, use other non-scratch
	 * registers somehow. */
	mov %ebp, %eax;
	mov %eax, %ebp;
	mov %ebp, %eax;
	mov %eax, %ebp;
	mov %esi, %eax;
	mov %eax, %esi;
	mov %esi, %eax;
	mov %eax, %esi;
	mov %esi, %eax;
	mov %eax, %esi;
	mov %esi, %eax;
	mov %eax, %esi;
	mov %edi, %eax;
	mov %eax, %edi;
	mov %edi, %eax;
	mov %eax, %edi;
	mov %edi, %eax;
	mov %eax, %edi;
	mov %edi, %eax;
	mov %eax, %edi;
	
	sub $0x10, %esp;
	xor %eax, %eax;
	mov %eax, 0x8(%esp);
	mov $kedr_test_array_cte01, %edx;
	test %eax, %eax;
	/* A jump within the block. */
	jz 1f;
	pushl 0x8(%edx,%eax,4);
	pop %ecx;

1:	mov %edx, %ebx;
	incl 0x8(%ebx,%eax,4);
	
	call kedr_test_common_type_e_aux;

/* Common block #2 (no jumps at all) */	
	mov $kedr_test_array_cte01, %edx;
	mov kedr_test_array_cte01, %ecx; /* Memory access! */
	lea 0x4(%edx), %ecx;
	mov (%ecx), %eax;
	mov %eax, (%esp);

	call kedr_test_common_type_e_aux;

/* Common block #3 (no memory operations; has jumps out) */
	test %eax, %eax;
	jnz 1f;
	
	call kedr_test_common_type_e_aux;

/* Common block #4 (has jumps out) */
	test %eax, %eax;
	jnz 1f;
	mov $kedr_test_array_cte01, %edx;
	test %edx, %edx;
	jz 2f;
	mov (%edx), %eax;
2:	test %eax, %eax;
	jmp 1f;
	decl (%edx);
	
	call kedr_test_common_type_e_aux;

/* The blocks #5 and #6 are here to check if the block end handler
 * clears 'dest_addr' in the local storage. If it does not, after the
 * end of the block is reached, a jump will be made to label "1b" 
 * (i.e. to "test %eax, %eax") and the test will hang. */

/* Common block #5 (has jumps out) */	
1:	test %eax, %eax;
	jmp 3f;
	jz 2f;
3:	mov kedr_test_array_cte01, %ecx; /* Memory access! */
	
	call kedr_test_common_type_e_aux;

/* Common block #6 (no jumps at all) */
2:	incl (%edx);
	
1:	add $0x10, %esp;
	pop %ebx;
	pop %ebp;
	ret;
.size kedr_test_common_type_e, .-kedr_test_common_type_e
/* ====================================================================== */

.global kedr_test_common_type_e_aux
.type   kedr_test_common_type_e_aux,@function; 

kedr_test_common_type_e_aux:
	xor %eax, %eax;
	ret;
.size kedr_test_common_type_e_aux, .-kedr_test_common_type_e_aux
/* ====================================================================== */

.data
.align 8,0

.global kedr_test_array_cte01
.type   kedr_test_array_cte01,@object
kedr_test_array_cte01: .int 0xbee0feed, 0x12345678, 0x0, 0xabcdefab, 0x0, 0x0
.size kedr_test_array_cte01, .-kedr_test_array_cte01
/* ====================================================================== */ 
