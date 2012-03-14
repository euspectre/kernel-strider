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
	push %ebp;
	mov %esp, %ebp;
	mov %ecx, 0x58(%eax);
	lock bts $0x2, 0x10(%eax);
	mov 0x58(%eax), %ecx;
	xor %ecx, %ecx;
	xor %eax, %eax;
	lock cmpxchg %cl, 0x10(%eax);
	xchg %ax, %ax;
	xor %ecx, %ecx;
	xchg %ecx, 0x18(%eax);
	xor %ecx, %ecx;
	xchg %eax, %eax;
	lock decl 0x18(%eax);
	xor %ecx, %ecx;
	nop;
	lock cmpxchg8b 0x20(%eax);
	xor %eax, %eax;
	pop %ebp;
	ret;
.size kedr_test_locked_updates, .-kedr_test_locked_updates
