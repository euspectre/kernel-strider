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
	push %ebp;
	mov %esp, %ebp;
	mov %eax, %ecx;
	jecxz 1f;
2:	add %ecx, %edx;
	inc %edx;
	loop 2b;
1:	mov %edx, %ecx;
	inc %ecx;
2:	test %eax, %ecx;
	je 1f;
	inc %edx;
	loope 2b;
	mov %edx, %ecx;
2:	test %eax, %ecx;
	inc %edx;
	loopne 2b;
1:	pop %ebp;
	ret;
.size kedr_test_jcxz_loop, .-kedr_test_jcxz_loop
/* ====================================================================== */
