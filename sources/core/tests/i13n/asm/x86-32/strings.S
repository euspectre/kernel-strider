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
	push %esi;
	push %edi;

	movw kedr_test_strings01, %dx;
	mov $kedr_test_strings01, %eax;
	mov $kedr_test_strings03, %edx;
	mov %eax, %esi;
	mov %edx, %edi;
	
	cld;
	mov $0x28, %ecx;
	rep movsb;

	/* Make sure a string insn is located at the end of the block,
	 * a special case that should also be checked (nodes will be
	 * added after the last reference node of the block during the 
	 * process). */
	call kedr_test_strings_aux;

	mov $kedr_test_strings01, %eax;
	mov $kedr_test_strings03, %edx;
	
	std;
	mov $0x4, %ecx;
	/* CMPS will process the strings from the end (DF=1) */
	repe cmpsl;
	cld;
	mov %eax, %esi;
	mov %edx, %edi;
	je 1f;
	xor %ecx, %ecx;

1: 	lodsw;
	stosb;
	mov kedr_test_strings02, %eax;
	xor %eax, %eax;
	scasl;

	pop %edi;
	pop %esi;
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
