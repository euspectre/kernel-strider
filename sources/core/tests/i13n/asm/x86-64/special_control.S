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

.global kedr_test_special_control
.type   kedr_test_special_control,@function; 

kedr_test_special_control:
	push %rbp;
	mov %rsp, %rbp;
	test %rax, %rax;
	jne 1f;
	mov %rbp, %rax;
	ud2;
1:	xor %rcx, %rcx;
	test %rax, %rax;
2:	sub $0x3, %rcx;
	cmp %rcx, %rdx;
	mov %rcx, %rdx;
	jb 2b;
	xor %rcx, %rcx;
	xor %rax, %rax;
	int $0x3;
	xor %rcx, %rcx;
	xor %rax, %rax;
	int $0x2;
	xor %rax, %rax;
	pop %rbp;
	ret;
.size kedr_test_special_control, .-kedr_test_special_control
/* ====================================================================== */
