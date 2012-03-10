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

.global kedr_test_barriers_cpuid
.type   kedr_test_barriers_cpuid,@function; 

kedr_test_barriers_cpuid:
	push %rbp;
	mov %rsp, %rbp;
	push %rbx;
	mov $0x01, %rax;
	cpuid
	pop %rbx;
	pop %rbp;
	ret;
.size kedr_test_barriers_cpuid, .-kedr_test_barriers_cpuid
