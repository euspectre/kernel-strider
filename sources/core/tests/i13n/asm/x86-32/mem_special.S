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
	xor %eax, %eax;
	ret;
.size kedr_test_mem_special_aux, .-kedr_test_mem_special_aux
/* ====================================================================== */

.global kedr_test_mem_special_xlat_bx
.type   kedr_test_mem_special_xlat_bx,@function; 

/* Check handling of XLAT when %ebx is %base. */
kedr_test_mem_special_xlat_bx:
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
		
	mov $kedr_test_array_ms01, %ebx;
	mov $0x2, %al;
	xlat;	

	pop %ebx;
	pop %ebp;
	ret;
.size kedr_test_mem_special_xlat_bx, .-kedr_test_mem_special_xlat_bx
/* ====================================================================== */

.global kedr_test_mem_special
.type   kedr_test_mem_special,@function; 

kedr_test_mem_special:
	push %ebx;
	
	mov $kedr_test_array_ms01, %ebx;
	mov $0x2, %al;
	xlat;
	
	xor %al, %al;
	mov $0x1, %cl;
	cmpxchg %cl, 0x8(%ebx);
	jz 1f;
	cmpxchg %cl, %al;
	cmove %ecx, %eax;
		
1:	sete 0x10(%ebx);
	cmpl $0x10, 0x4(%ebx);
	cmovge (%ebx), %edx;
	
	mov (%ebx), %eax;
	mov 0x8(%ebx), %ecx;
	cmpxchg %ecx, 0x8(%ebx);
	jz 1f;
	mov %eax, (%ebx);

1:	xor %eax, %eax;
	mov kedr_test_array_ms01, %al;
	mov %al, kedr_test_array_ms01;
	mov kedr_test_array_ms01, %eax;
	mov %eax, kedr_test_array_ms01;

	pop %ebx;
	ret;
.size kedr_test_mem_special, .-kedr_test_mem_special
/* ====================================================================== */

.data
.align 8,0
kedr_test_array_ms01: .int 0xbee0feed, 0x12345678, 0x0, 0xabcdefab, 0x0, 0x0
/* ====================================================================== */ 
