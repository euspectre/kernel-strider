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

.global kedr_test_long_blocks
.type   kedr_test_long_blocks,@function; 

kedr_test_long_blocks:
	push %ebp;
	mov %esp, %ebp;
	
	/* A sequence w/o string operations that should be split in 4
	 * blocks. */
	xor %edx, %edx;
	add (%eax), %edx; 
	push (%ecx, %eax, 4);
	movb %cl, (%ecx);
	addw (%ecx), %ax; /* #4 */
	
	.rept 14
	xor %ecx, %ecx;
	add (%eax), %edx;
	add (%eax), %edx;
	add (%eax), %edx;
	add (%eax), %edx; 
	.endr /* #60 */
	
	xor %ecx, %ecx;
	pop (%edx);
	add (%eax), %edx;
	add (%eax), %edx;
	rep lodsb; /* values #64 and #65 */

	xor %ecx, %ecx;
	add (%eax), %edx;
	rep stosw; /* values #67 and #68 */
	
	.rept 6
	xor %ecx, %ecx;
	add (%eax), %edx;
	add (%eax), %edx;
	add (%eax), %edx;
	add (%eax), %edx; 
	.endr /* #92 */
	
	add (%eax), %edx; /* #93 */
	
	movsl /* values #94-97 */
	add (%eax), %edx;
	movsl
	
	pop %ebx;
	pop %ebp;
	ret;
.size kedr_test_long_blocks, .-kedr_test_long_blocks
