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

/* The function reads the data byte(s) using disp32 addressing 
 * (w/o index and base) on x86-32 and IP-relative addressing on x86-64. 
 * The goal is just to check that these operations are instrumented 
 * properly. */

.text

.global kedr_test_disp32_iprel
.type   kedr_test_disp32_iprel,@function; 

kedr_test_disp32_iprel:
	push %ebp;
	mov %esp, %ebp;
	mov 0x6aadd00f, %edx;
	xor %eax, %eax;
	lea 0x6eaff00d, %edx;
	mov (%edx), %eax;
	pop %ebp;
	ret;
.size kedr_test_disp32_iprel, .-kedr_test_disp32_iprel
