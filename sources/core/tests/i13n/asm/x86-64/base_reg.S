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
 * - the choice of the base register;
 * - releasing %base;
 * - handling of function entry and exits. */

.text
/* ====================================================================== */

.global kedr_test_base_reg
.type   kedr_test_base_reg,@function; 

kedr_test_base_reg:
	push %rbx;
	push %rbp;
	push %r15;
	push %r13;
	push %r14;
	
	mov %r12, %r12;
	lea (%r12), %r12;
	nop;
	
	xor %rbx, %rbx;
	xor %rbp, %rbp;
	xor %r15, %r15;
	xor %r13, %r13;
	xor %r14, %r14;
	
	mov %r12, %rbp;

	pop %r14;
	pop %r13;
	pop %r15;
	pop %rbp;
	pop %rbx;
	ret;
.size kedr_test_base_reg, .-kedr_test_base_reg
/* ====================================================================== */
