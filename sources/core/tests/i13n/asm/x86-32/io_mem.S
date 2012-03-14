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
	push %ebp;
	mov %esp, %ebp;
	mov $0x5, %ecx;
	rep insb;
	xor %ecx, %ecx;
	xor %eax, %eax;
	insw;
	xor %ecx, %ecx;
	xor %eax, %eax;
	insl;
	xor %ecx, %ecx;
	xor %eax, %eax;
	mov $0x5, %ecx;
	rep outsb;
	xor %ecx, %ecx;
	xor %eax, %eax;
	outsw;
	xor %ecx, %ecx;
	xor %eax, %eax;
	outsl;
	pop %ebp;
	ret;
.size kedr_test_io_mem, .-kedr_test_io_mem
