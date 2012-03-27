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

/* The code below can be used to check the following:
 * - the choice of the base register (general case; the functions with 
 *   string operations);
 * - releasing %base (general case; PUSHAD & POPAD; nops);
 * - handling of function entry and exits. */

.text
/* ====================================================================== */

.global kedr_test_base_reg
.type   kedr_test_base_reg,@function; 

kedr_test_base_reg:
	push %ebx;
	push %ebp;
	push %edi;
	push %esi;
	
	mov %ebp, %ebp;
	lea (%ebp), %ebp;
	nop;
	
	xor %eax, %eax;
	inc %ebp;
	xor %eax, %eax;
	
	pusha;
	popa;
	
	xor %eax, %eax;
	
	pusha;
	xor %eax, %eax;
	popa;
	
	inc %ebx;
	inc %esi;
	inc %edi;
	inc %ebx;
	inc %esi;
	inc %edi;
	
	pop %esi;
	pop %edi;
	pop %ebp;
	pop %ebx;
	ret;
.size kedr_test_base_reg, .-kedr_test_base_reg
/* ====================================================================== */

/* Checking that %esi is not used as %base if the function has a string 
 * operation of type X. */
.global kedr_test_base_reg_no_esi
.type   kedr_test_base_reg_no_esi,@function; 

kedr_test_base_reg_no_esi:
	push %ebx;
	push %ebp;
	push %edi;

	inc %ebp;
	inc %ebx;
	inc %ebp;
	inc %ebx;
	inc %ebp;
	inc %ebx;

	mov %esi, %ebx;
	mov $kedr_test_string1, %esi;
	lodsb;
	mov %ebx, %esi;

	dec %ebp;
	inc %edi;
	dec %edi;
	xor %edi, %edi;

	pop %edi;
	pop %ebp;
	pop %ebx;
	ret;
.size kedr_test_base_reg_no_esi, .-kedr_test_base_reg_no_esi
/* ====================================================================== */

/* Checking that %edi is not used as %base if the function has a string 
 * operation of type Y. */
.global kedr_test_base_reg_no_edi
.type   kedr_test_base_reg_no_edi,@function; 

kedr_test_base_reg_no_edi:
	push %ebx;
	push %ebp;
	push %esi;
	
	inc %ebp;
	inc %ebx;
	inc %ebp;
	inc %ebx;
	inc %ebp;
	inc %ebx;
	
	mov %edi, %ebx;
	mov $kedr_test_string1, %edi;
	stosb;
	mov %ebx, %edi;
	
	dec %ebp;
	inc %esi;
	dec %esi;
	xor %esi, %esi;
	
	pop %esi;
	pop %ebp;
	pop %ebx;
	ret;
.size kedr_test_base_reg_no_edi, .-kedr_test_base_reg_no_edi
/* ====================================================================== */

/* Checking that neither %esi nor %edi is used as %base if the function has
 * string operation both of type X and of type Y. */
.global kedr_test_base_reg_no_esi_edi1
.type   kedr_test_base_reg_no_esi_edi1,@function; 

kedr_test_base_reg_no_esi_edi1:
	push %ebx;
	push %ebp;
	mov %esi, %ebx;
	mov %edi, %ebp;
	mov $kedr_test_string1, %esi;
	mov $kedr_test_string2, %edi;
	lodsb;
	stosb;
	mov %ebp, %edi;
	mov %ebx, %esi;
	inc %ebx;
	inc %ebp;
	pop %ebp;
	pop %ebx;
	ret;
.size kedr_test_base_reg_no_esi_edi1, .-kedr_test_base_reg_no_esi_edi1
/* ====================================================================== */

/* Checking that neither %esi nor %edi is used as %base if the function has
 * a string operation of type XY. */
.global kedr_test_base_reg_no_esi_edi2
.type   kedr_test_base_reg_no_esi_edi2,@function; 

kedr_test_base_reg_no_esi_edi2:
	push %ebx;
	push %ebp;
	mov %esi, %ebx;
	mov %edi, %ebp;
	mov $kedr_test_string1, %esi;
	mov $kedr_test_string2, %edi;
	cmpsb;
	mov %ebp, %edi;
	mov %ebx, %esi;
	inc %ebx;
	inc %ebp;
	pop %ebp;
	pop %ebx;
	ret;
.size kedr_test_base_reg_no_esi_edi2, .-kedr_test_base_reg_no_esi_edi2
/* ====================================================================== */

.data
.align 8,0
kedr_test_string1: .asciz "A quick brown fox"
kedr_test_string2: .asciz "jumps over a lazy dog."
/* ====================================================================== */
