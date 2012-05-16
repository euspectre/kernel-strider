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

.text
/* ====================================================================== */

.global kedr_test_stack_access
.type   kedr_test_stack_access,@function; 

kedr_test_stack_access:
	push %rbp;
	mov %rsp, %rbp;
	push %rbx;
	
	/* read the return address */
	mov 0x8(%rbp), %rbx;
	
	/* read the non-stack data */
	mov $kedr_test_array_sa01, %rbx;
	mov (%rbx), %rax;
	mov 0x8(%rbx), %rcx;
	
	pop %rbx;
	pop %rbp;
	ret;
.size kedr_test_stack_access, .-kedr_test_stack_access
/* ====================================================================== */

.data
.align 8,0

.global kedr_test_array_sa01
.type   kedr_test_array_sa01,@object
kedr_test_array_sa01: .int 0xbee0feed, 0x12345678, 0x0, 0xabcdefab, 0x0, 0x0
.size kedr_test_array_sa01, .-kedr_test_array_sa01
/* ====================================================================== */ 
