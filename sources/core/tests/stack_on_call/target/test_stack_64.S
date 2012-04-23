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

/* The functions defined here check that the argument passed to a function
 * on the stack remains where the function expects it even if the code has
 * been instrumented. */

.text

#define CANARY_WORD 0x07edf0ac12cd59af
/* ====================================================================== */

.global kedr_test_stack_on_call
.type   kedr_test_stack_on_call,@function; 

kedr_test_stack_on_call:
	sub $0x10, %rsp;
	
	mov $CANARY_WORD, %rax;
	mov %rax, (%rsp);
	call kedr_test_stack_func;
	
	add $0x10, %rsp;
	ret;
.size kedr_test_stack_on_call, .-kedr_test_stack_on_call
/* ====================================================================== */

.global kedr_test_stack_on_jmp
.type   kedr_test_stack_on_jmp,@function; 

kedr_test_stack_on_jmp:
	sub $0x10, %rsp;
	
	mov $CANARY_WORD, %rax;
	mov %rax, (%rsp);
	call kedr_test_stack_aux1;
	
	add $0x10, %rsp;
	ret;
.size kedr_test_stack_on_jmp, .-kedr_test_stack_on_jmp
/* ====================================================================== */

.global kedr_test_stack_aux1
.type   kedr_test_stack_aux1,@function; 

kedr_test_stack_aux1:
	/* Do something just to make sure the function is at least 5 bytes
	 * in size, then jump to the test function. */
	xor %rax, %rax;
	xor %rcx, %rcx;
	xor %rdx, %rdx;
	inc %rax;
	jmp kedr_test_stack_func;
.size kedr_test_stack_aux1, .-kedr_test_stack_aux1
/* ====================================================================== */

.global kedr_test_stack_func
.type   kedr_test_stack_func,@function; 

kedr_test_stack_func:
	mov 0x8(%rsp), %rdx;
	mov $CANARY_WORD, %rax;
	
	cmp %rdx, %rax
	jne 1f; /* Mismatch => exit without changing 'kedr_test_status' */
	
	/* Test passed */
	movl $0x1, kedr_test_status
	
1:	xor %rax, %rax;
	ret;
.size kedr_test_stack_func, .-kedr_test_stack_func
/* ====================================================================== */

.data
.align 8,0

.global kedr_test_status
.type kedr_test_status,@object
kedr_test_status: .int 0x0
.size kedr_test_status, .-kedr_test_status
/* ====================================================================== */
