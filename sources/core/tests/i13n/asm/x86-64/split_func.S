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

.global kedr_test_split_func
.type   kedr_test_split_func,@function; 
.global kedr_test_split_func_tail

kedr_test_split_func:
	push %rbp;
	mov %rsp, %rbp;
	push %rbx;
	mov $0x01, %rax;
	cpuid;
kedr_test_split_func_tail:
	pop %rbx;
	pop %rbp;
	ret;
.size kedr_test_split_func, .-kedr_test_split_func
