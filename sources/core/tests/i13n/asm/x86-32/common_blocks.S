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

.global kedr_test_common_blocks
.type   kedr_test_common_blocks,@function; 

kedr_test_common_blocks:
	push %ebp;
	mov %esp, %ebp;
	add $0x7, 0x10(%eax);
1:	or 0x18(%eax), %edx;
	call *(%edx);
	cmp %ecx, 0x10(%eax);
	setg 0x20(%eax);
	setle %cl;
	js 2f;
	cmove 0x10(%eax), %ecx;
	jg 1b;
2:	jmp 3f;
	test %ecx, 0x18(%eax);
	decl 0x20(%eax);
	xlatb;
	movsxb 0x20(%eax), %ecx;
	movsxw 0x28(%eax), %edx;
	movzxb 0x20(%eax), %ecx;
	movzxw 0x30(%eax), %edx;
	mov 0xbaadf00d, %al;
	mov 0xbaadf00d, %eax;
	mov %eax, 0xbeefdeaf
	mov %al, 0xbeadfeed;
	crc32b 0x28(%eax), %edx;
	crc32l 0x28(%eax), %ecx;
3:	xor %eax, %eax;
	pop %ebp;
	ret;
.size kedr_test_common_blocks, .-kedr_test_common_blocks
