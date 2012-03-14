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

.global kedr_test_locked_updates
.type   kedr_test_locked_updates,@function; 

kedr_test_locked_updates:
	push %rbp;
	mov %rsp, %rbp;
	mov %rcx, 0x58(%rax);
	lock bts $0x2, 0x10(%rax);
	mov 0x58(%rax), %rcx;
	xor %rcx, %rcx;
	xor %rax, %rax;
	lock cmpxchg %cl, 0x10(%rax);
	xchg %ax, %ax;
	xor %rcx, %rcx;
	xchg %rcx, 0x18(%rax);
	xor %rcx, %rcx;
	xchg %rax, %rax;
	lock decl 0x18(%rax);
	xor %rcx, %rcx;
	nop;
	lock cmpxchg8b 0x20(%rax);
	xor %rcx, %rcx;
	nop;
	lock cmpxchg16b 0x20(%rax);
	xor %rax, %rax;
	pop %rbp;
	ret;
.size kedr_test_locked_updates, .-kedr_test_locked_updates
