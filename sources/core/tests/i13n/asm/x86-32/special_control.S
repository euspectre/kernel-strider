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
	push %ebp;
	mov %esp, %ebp;
	test %eax, %eax;
	jne 1f;
	mov %ebp, %eax;
	ud2;
1:	xor %ecx, %ecx;
	test %eax, %eax;
2:	sub $0x3, %ecx;
	cmp %ecx, %edx;
	mov %ecx, %edx;
	jb 2b;
	xor %ecx, %ecx;
	xor %eax, %eax;
	int $0x3;
	xor %ecx, %ecx;
	xor %eax, %eax;
	int $0x2;
	
	/* x86-32 only */
	xor %ecx, %ecx;
	xor %eax, %eax;
	into;
	
	xor %eax, %eax;
	pop %ebp;
	ret;
.size kedr_test_special_control, .-kedr_test_special_control
/* ====================================================================== */
