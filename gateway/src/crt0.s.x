| C run-time startup kfps

| Copyright (c) 1986 Kinetics, Inc.
| May be used but not sold without permission.

| $Header: /usr/fp/src/fps/ram.udp/RCS/crt0.s,v 1.13 86/02/21 20:39:01 tim Rel $

| Hardware definitions for KFPS

| (c) 1986, Kinetics, Inc.
| May be used but not sold without permission.

| $Header: /usr/fp/src/fps/ram.udp/RCS/kfps.h,v 1.13 86/02/21 20:40:31 tim Rel $

| CPU and speed definition



| for vaxen running 4.2 and possibly others, use flexnames
| not that the '#' should be deleted as well... it's an m4 comment delimiter


|  the places to patch ram interrupt service







| memory map
ram7	= 0x6e000
ram6	= 0x6c000
ram5	= 0x6a000
ram4	= 0x68000
ram3	= 0x66000
ram2	= 0x64000
port1	= 0x62000
port0	= 0x60000
z8530	= 0x40000
prom1	= 0x20000
prom0	= 0x00000
| scc offsets from z8530
bctl	= 0
bdata	= 1
actl	= 2
adata	= 3
sccdata	= 1		| offset to data from ctl
| chip sizes
ramsize = 0x2000
romsize = 0x2000
| stack allocation
stksiz	= 80			| initial stack size
scrlen	= 80			| scratch area for out packets
| address of table which contains 82586 initialization root to reset 82586
| and 68000 interrupt vector numbers whenever interrupts are enabled
roottab	= 0xfffffff0		| address of table 
| commands used by kfps - see "cmdidx.h" for C programs
kc_boot	= 15
kc_idle	= 30


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

	.globl	table_ptr
	movl	sp,table_ptr

| set stack ptr
|stack_size	= 512
stack_size	= 2048

	lea	stack_a+stack_size,sp
| call main()
	jsr	main
| main not supposed to exit
|	illegal
	.word	0x4afc
	.comm	stack_a,stack_size

