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

.global kedr_test_calls_jumps
.type   kedr_test_calls_jumps,@function; 

kedr_test_calls_jumps:
	push %ebp;
	mov %esp, %ebp;
	call kedr_test_calls_jumps_aux1;
	test %eax, %eax;
	mov %eax, %edx;
	xor %eax, %eax;
	call *%ecx;
	test %eax, %eax;
	xor %eax, %eax;
	call 1f;
1:	pop %edx;
	mov %edx, %eax;
	jmp kedr_test_calls_jumps_aux1;
	xor %eax, %eax;
	mov %eax, %edx;
	jmp *%ecx;
	mov %edx, %eax;
	xor %edx, %edx;
	
	/* x86-32 only */
	lcall $0xbaad, $0xf00dbeef;
	
	xor %edx, %edx;
	xor %eax, %eax;

	/* x86-32 only */
	ljmp $0xdaad, $0xb00ffeed;
	
	xor %edx, %edx;
	xor %eax, %eax;
	lcall *(%ecx);
	xor %edx, %edx;
	xor %eax, %eax;
	ljmp *(%ecx);
	xor %eax, %eax;
	pop %ebp;
	ret;
.size kedr_test_calls_jumps, .-kedr_test_calls_jumps
/* ====================================================================== */

.global kedr_test_calls_jumps_aux1
.type   kedr_test_calls_jumps_aux1,@function;

kedr_test_calls_jumps_aux1:
	push %ebp;
	mov %esp, %ebp;
	push %ebx;
	mov $0x01, %eax;
	cpuid;
	pop %ebx;
	pop %ebp;
	ret;
.size kedr_test_calls_jumps_aux1, .-kedr_test_calls_jumps_aux1
/* ====================================================================== */

.global kedr_test_calls_jumps_aux2
.type   kedr_test_calls_jumps_aux2,@function;

kedr_test_calls_jumps_aux2:
	push %ebp;
	mov %esp, %ebp;
	push %ebx;
	mov $0x05, %eax;
	cpuid;
	pop %ebx;
	pop %ebp;
	ret;
.size kedr_test_calls_jumps_aux2, .-kedr_test_calls_jumps_aux2
