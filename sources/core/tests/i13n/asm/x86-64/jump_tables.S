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

.global kedr_test_jump_tables
.type   kedr_test_jump_tables,@function; 

kedr_test_jump_tables:
	push %rbp;
	mov %rsp, %rbp;
	sub $0x10, %rsp;
	xor %rcx, %rcx;
	lea (%rax,%rax,2), %rdx;
	
	jmpq *jump_table1(,%rax,8);
	
	/* dest1 */
	add %rdx, %rcx;
	jmp 1f;
	
	/* dest2 */
	sbb %rdx, %rcx;
	jmp 1f;
	
	/* dest3 */
	sub %rdx, %rcx;
	jmp 1f;
	
1:	test %rcx, %rcx;

	jmpq *jump_table2(,%rdx,8);
	
	/* dest4 */
	adc %rdx, %rcx;
	jmp 1f;
	
	/* dest5 */
	adc %rax, %rcx;
		
1:	test %rcx, %rcx;
	je 2f;
	
	jmpq *jump_table_out(,%rax,8);

2:	dec %rax;
	jmpq *jump_table3(,%rax,8);
	
	/* dest6 */
	sub %rdx, %rcx;
	jmp 2f;
	
	/* dest7 */
	adc %rdx, %rcx;
	
2:	mov %rcx, %rax;
	add $0x10, %rsp;
	pop %rbp;
	ret;
.size kedr_test_jump_tables, .-kedr_test_jump_tables
/* ====================================================================== */

.global kedr_test_jump_tables_aux1
.type   kedr_test_jump_tables_aux1,@function;

kedr_test_jump_tables_aux1:
	push %rbp;
	mov %rsp, %rbp;
	push %rbx;
	mov $0x01, %rax;
	cpuid
	pop %rbx;
	pop %rbp;
	ret;
.size kedr_test_jump_tables_aux1, .-kedr_test_jump_tables_aux1
/* ====================================================================== */

.global kedr_test_jump_tables_aux2
.type   kedr_test_jump_tables_aux2,@function;

kedr_test_jump_tables_aux2:
	push %rbp;
	mov %rsp, %rbp;
	push %rbx;
	mov $0x05, %rax;
	cpuid
	pop %rbx;
	pop %rbp;
	ret;
.size kedr_test_jump_tables_aux2, .-kedr_test_jump_tables_aux2
/* ====================================================================== */

.data
.align 8,0

jump_table1: .quad \
	kedr_test_jump_tables+0x16, \
	kedr_test_jump_tables+0x1b, \
	kedr_test_jump_tables+0x16, \
	kedr_test_jump_tables+0x20

jump_table2: .quad \
	kedr_test_jump_tables+0x2f, \
	kedr_test_jump_tables+0x34

jump_table_out: .quad \
	kedr_test_jump_tables_aux1, \
	kedr_test_jump_tables_aux2, \
	kedr_test_jump_tables_aux1, \
	kedr_test_jump_tables_aux1

jump_table3: .quad \
	kedr_test_jump_tables+0x16, \
	kedr_test_jump_tables+0x34, \
	kedr_test_jump_tables+0x4d, \
	kedr_test_jump_tables+0x52 
