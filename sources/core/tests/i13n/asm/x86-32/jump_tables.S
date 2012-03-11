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
	push %ebp;
	mov %esp, %ebp;
	sub $0x10, %esp;
	xor %ecx, %ecx;
	lea (%eax,%eax,2), %edx;
	
	jmpl *jump_table1(,%eax,4);
	
	/* dest1 */
	add %edx, %ecx;
	jmp 1f;
	
	/* dest2 */
	sbb %edx, %ecx;
	jmp 1f;
	
	/* dest3 */
	sub %edx, %ecx;
	jmp 1f;
	
1:	test %ecx, %ecx;

	jmpl *jump_table2(,%edx,4);
	
	/* dest4 */
	adc %edx, %ecx;
	jmp 1f;
	
	/* dest5 */
	adc %eax, %ecx;
		
1:	test %ecx, %ecx;
	je 2f;
	
	jmpl *jump_table_out(,%eax,4);

2:	dec %eax;
	jmpl *jump_table3(,%eax,4);
	
	/* dest6 */
	sub %edx, %ecx;
	jmp 2f;
	
	/* dest7 */
	adc %edx, %ecx;
	
2:	mov %ecx, %eax;
	add $0x10, %esp;
	pop %ebp;
	ret;
.size kedr_test_jump_tables, .-kedr_test_jump_tables
/* ====================================================================== */

.global kedr_test_jump_tables_aux1
.type   kedr_test_jump_tables_aux1,@function;

kedr_test_jump_tables_aux1:
	push %ebp;
	mov %esp, %ebp;
	push %ebx;
	mov $0x01, %eax;
	cpuid
	pop %ebx;
	pop %ebp;
	ret;
.size kedr_test_jump_tables_aux1, .-kedr_test_jump_tables_aux1
/* ====================================================================== */

.global kedr_test_jump_tables_aux2
.type   kedr_test_jump_tables_aux2,@function;

kedr_test_jump_tables_aux2:
	push %ebp;
	mov %esp, %ebp;
	push %ebx;
	mov $0x05, %eax;
	cpuid
	pop %ebx;
	pop %ebp;
	ret;
.size kedr_test_jump_tables_aux2, .-kedr_test_jump_tables_aux2
/* ====================================================================== */

.data
.align 8,0

jump_table1: .long \
	kedr_test_jump_tables+0x12, \
	kedr_test_jump_tables+0x16, \
	kedr_test_jump_tables+0x12, \
	kedr_test_jump_tables+0x1a

jump_table2: .long \
	kedr_test_jump_tables+0x27, \
	kedr_test_jump_tables+0x2b

jump_table_out: .long \
	kedr_test_jump_tables_aux1, \
	kedr_test_jump_tables_aux2, \
	kedr_test_jump_tables_aux1, \
	kedr_test_jump_tables_aux1

jump_table3: .long \
	kedr_test_jump_tables+0x12, \
	kedr_test_jump_tables+0x2b, \
	kedr_test_jump_tables+0x40, \
	kedr_test_jump_tables+0x44 
