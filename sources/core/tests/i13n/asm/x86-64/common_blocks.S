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
	push %rbp;
	mov %rsp, %rbp;
	add $0x7, 0x10(%rax);
1:	or 0x18(%rax), %r11;
	call *(%rdx);
	cmp %rcx, 0x10(%rax);
	setg 0x20(%rax);
	setle %cl;
	js 2f;
	cmove 0x10(%rax), %rcx;
	jg 1b;
2:	jmp 3f;
	test %rcx, 0x18(%rax);
	decq 0x20(%rax);
	xlatb;
	movsxb 0x20(%rax), %ecx;
	movsxw 0x28(%rax), %edx;
	movsxl 0x30(%rax), %rdx;
	movzxb 0x20(%rax), %ecx;
	movzxw 0x30(%rax), %rdx;
	movabs 0xffffffffbaadf00d, %al;
	movabs 0xffffffffbaadf00d, %rax;
	movabs %rax, 0xbeefdeafbaadf00d;
	movabs %al, 0xbeefdeafbaadf11d;
	crc32b 0x28(%rax), %rdx;
	crc32q 0x28(%rax), %r11;
3:	xor %rax, %rax;
	pop %rbp;
	ret;
.size kedr_test_common_blocks, .-kedr_test_common_blocks
