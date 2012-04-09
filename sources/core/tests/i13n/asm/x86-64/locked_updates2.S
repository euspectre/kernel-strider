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

.global kedr_test_locked_updates2
.type   kedr_test_locked_updates2,@function; 

kedr_test_locked_updates2:
	push %rbx;
	
	mov $kedr_test_array_lu01, %rbx;
	mov 0x8(%rbx), %rax;
	xchg %rax, (%rbx);
	addl $0x7, 0x4(%rbx);
	lock addl $0, (%rsp);
	lock subl $4, (%rbx);
	xor %cl, %cl;
	xchg 0x10(%rbx), %cl;
	
	xor %al, %al;
	mov $0x1, %cl;
	lock cmpxchg %cl, 0x8(%rbx);
	setne 0x8(%rbx);
	
	xor %ecx, %ecx;
	lock btsl %ecx, (%rbx);
	setc (%rbx);
	
	xor %eax, %eax;
	lock cmpxchg %rcx, (%rbx);
	jne 1f;
	
	xor %eax, %eax;
1:	pop %rbx;
	ret;
.size kedr_test_locked_updates2, .-kedr_test_locked_updates2
/* ====================================================================== */

.data
.align 8,0
kedr_test_array_lu01: .int 0xbee0feed, 0x12345678, 0x0, 0xabcdefab, 0x0, 0x0
/* ====================================================================== */ 
