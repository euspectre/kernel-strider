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
	push %rbp;
	mov %rsp, %rbp;
	call kedr_test_calls_jumps_aux1;
	test %rax, %rax;
	mov %rax, %rdx;
	xor %rax, %rax;
	callq *%rcx;
	test %rax, %rax;
	xor %rax, %rax;
	call 1f;
1:	pop %rdx;
	mov %rdx, %rax;
	jmp kedr_test_calls_jumps_aux1;
	xor %rax, %rax;
	mov %rax, %rdx;
	jmpq *%rcx;
	mov %rdx, %rax;
	xor %rdx, %rdx;
	lcall *(%rcx);
	xor %rdx, %rdx;
	xor %rax, %rax;
	ljmp *(%rcx);
	xor %rax, %rax;
	pop %rbp;
	ret;
.size kedr_test_calls_jumps, .-kedr_test_calls_jumps
/* ====================================================================== */

.global kedr_test_calls_jumps_aux1
.type   kedr_test_calls_jumps_aux1,@function;

kedr_test_calls_jumps_aux1:
	push %rbp;
	mov %rsp, %rbp;
	push %rbx;
	mov $0x01, %rax;
	cpuid;
	pop %rbx;
	pop %rbp;
	ret;
.size kedr_test_calls_jumps_aux1, .-kedr_test_calls_jumps_aux1
/* ====================================================================== */

.global kedr_test_calls_jumps_aux2
.type   kedr_test_calls_jumps_aux2,@function;

kedr_test_calls_jumps_aux2:
	push %rbp;
	mov %rsp, %rbp;
	push %rbx;
	mov $0x05, %rax;
	cpuid;
	pop %rbx;
	pop %rbp;
	ret;
.size kedr_test_calls_jumps_aux2, .-kedr_test_calls_jumps_aux2
