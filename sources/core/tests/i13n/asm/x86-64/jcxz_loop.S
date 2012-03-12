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

.global kedr_test_jcxz_loop
.type   kedr_test_jcxz_loop,@function; 

kedr_test_jcxz_loop:
	push %rbp;
	mov %rsp, %rbp;
	mov %rax, %rcx;
	jrcxz 1f;
2:	add %rcx, %rdx;
	inc %rdx;
	loop 2b;
1:	mov %rdx, %rcx;
	inc %rcx;
2:	test %rax, %rcx;
	je 1f;
	inc %rdx;
	loope 2b;
	mov %rdx, %rcx;
2:	test %rax, %rcx;
	inc %rdx;
	loopne 2b;
1:	pop %rbp;
	ret;
.size kedr_test_jcxz_loop, .-kedr_test_jcxz_loop
/* ====================================================================== */
