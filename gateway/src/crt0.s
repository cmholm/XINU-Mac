| C run-time startup kfps

| Copyright (c) 1986 Kinetics, Inc.
| May be used but not sold without permission.

| $Header: /usr/fp/src/fps/ram.udp/RCS/crt0.s,v 1.13 86/02/21 20:39:01 tim Rel $

include(`fp/kfps.h')

	.globl	_begin, _end, _edata
_begin:
| zero the bss
	movl	#_end,d0
	movl	#_edata,a0
	subl	a0,d0
	asrl	#1,d0		| make word count
	jra	L2
L1:
	clrw	a0@+
L2:
	dbra	d0,L1
| save pointer to tables
ifdef(`flexnames',`
	.globl	table_ptr
	movl	sp,table_ptr
',`
	.globl	_table_p
	movl	sp,_table_p
')
| set stack ptr
|stack_size	= 512
stack_size	= 2048
ifdef(`flexnames',`
	lea	stack_a+stack_size,sp
| call main()
	jsr	main
| main not supposed to exit
|	illegal
	.word	0x4afc
	.comm	stack_a,stack_size
',`
	lea	_stack_a+stack_size,sp
| call main()
	jsr	_main
| main not supposed to exit
	illegal
	.comm	_stack_a,stack_size
')
