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

.global kedr_test_io_mem
.type   kedr_test_io_mem,@function; 

kedr_test_io_mem:
	push %rbp;
	mov %rsp, %rbp;
	mov $0x5, %rcx;
	rep insb;
	xor %rcx, %rcx;
	xor %rax, %rax;
	insw;
	xor %rcx, %rcx;
	xor %rax, %rax;
	insl;
	xor %rcx, %rcx;
	xor %rax, %rax;
	mov $0x5, %rcx;
	rep outsb;
	xor %rcx, %rcx;
	xor %rax, %rax;
	outsw;
	xor %rcx, %rcx;
	xor %rax, %rax;
	outsl;
	pop %rbp;
	ret;
.size kedr_test_io_mem, .-kedr_test_io_mem
