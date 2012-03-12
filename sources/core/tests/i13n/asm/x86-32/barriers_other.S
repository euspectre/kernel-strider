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
	push %ebp;
	mov %esp, %ebp;
	
	/* *FENCE */
	lfence;
	xor %eax, %eax;
	xor %ecx, %ecx;
	sfence;
	xor %eax, %eax;
	xor %ecx, %ecx;
	mfence;
	xor %eax, %eax;
	xor %ecx, %ecx;
	
	/* IN and OUT */
	in $0x37, %al;
	xor %eax, %eax;
	xor %ecx, %ecx;
	in $0x37, %eax;
	xor %eax, %eax;
	xor %ecx, %ecx;
	in (%dx), %al;
	xor %eax, %eax;
	xor %ecx, %ecx;
	in (%dx), %eax;
	xor %eax, %eax;
	xor %ecx, %ecx;
	
	out %al, $0x28
	xor %eax, %eax;
	xor %ecx, %ecx;
	out %eax, $0x28;
	xor %eax, %eax;
	xor %ecx, %ecx;
	out %al, (%dx);
	xor %eax, %eax;
	xor %ecx, %ecx;
	out %eax, (%dx);
	xor %eax, %eax;
	xor %ecx, %ecx;
	
	/* INVD, WBINVD, INVLPG */
	invd;
	xor %eax, %eax;
	xor %ecx, %ecx;
	
	invlpg (%edx);
	xor %eax, %eax;
	xor %ecx, %ecx;
	
	wbinvd;
	xor %eax, %eax;
	xor %ecx, %ecx;
	
	/* MOV to CRn */
	mov %ebx, %cr2;
	mov %cr2, %eax;
	xor %eax, %eax;
	xor %ecx, %ecx;
	mov %ebx, %cr0;
	mov %cr0, %eax;
	xor %eax, %eax;
	xor %ecx, %ecx;
	
	/* MOV to DRn */
	mov %eax, %dr0;
	mov %dr0, %ecx;
	
	pop %ebx;
	pop %ebp;
	ret;
.size kedr_test_barriers_other, .-kedr_test_barriers_other
