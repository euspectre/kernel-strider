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

/* First, this function check if xFENCE instructions are available and if
 * they are, executes them among the memory access instructions. 
 * 
 * We make use of the fact that if SSE2 is supported by the processor,
 * xFENCE instructions must be supported too. Actually, some of these 
 * instructions may be supported even by earlier CPUs (without SSE2) 
 * but it does not really matter here.
 *
 * So, we check that CPUID.01H:EDX.SSE2[bit 26] == 1. */

.global kedr_test_barriers_mem
.type   kedr_test_barriers_mem,@function; 

kedr_test_barriers_mem:
	push %ebx;
	
	/* Check if CPUID is supported (it should be but - just in case) */
	pushf;
	pop %eax;
	mov %eax, %ecx;
	/* Try to toggle ID flag */
	xor $0x00200000, %eax;
	push %eax;
	popf;
	pushf;
	pop %eax;
	cmp %ecx, %eax;
	je 1f; /* no CPUID */
	
	/* Check if LFENCE, SFENCE and MFENCE are supported. */	
	mov $0x1, %eax;
	cpuid;
	test $0x04000000, %edx;
	je 1f; /* no SSE2 */
	
	/* If we have got here, xFENCE must be supported. */
	mov $kedr_test_array_bm01, %ebx;
	mov (%ebx), %eax;
	lfence;
	mov %eax, 0x10(%ebx);
	sfence;
	incl 0x4(%ebx);
	decl 0xc(%ebx);
	mfence;
	mov (%ebx), %eax;
		
1:	pop %ebx;
	ret;
.size kedr_test_barriers_mem, .-kedr_test_barriers_mem
/* ====================================================================== */

.data
.align 8,0

.global kedr_test_array_bm01
.type   kedr_test_array_bm01,@object
kedr_test_array_bm01: .int 0xbee0feed, 0x12345678, 0x0, 0xabcdefab, 0x0, 0x0
.size kedr_test_array_bm01, .-kedr_test_array_bm01
/* ====================================================================== */ 
