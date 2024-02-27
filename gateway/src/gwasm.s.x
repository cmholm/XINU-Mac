|  (c) 1984, Stanford Univ. SUMEX project.
|  May be used but not sold without permission.
|
|  (c) 1986, Kinetics, Inc.
|  May be used but not sold without permission.

| $Header: /usr/fp/src/fps/ram.udp/RCS/gwasm.s,v 1.13 86/02/21 20:39:43 tim Rel $

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


|
| one time initialization to setup vectors, etc.
|

	.globl	asminit, table_ptr
asminit:
| Just catch the exceptions we want to handle
	movl	a0,sp@-
	movl	table_ptr,a0

	movw	#0x4ef9,a0@(0x120)	| "jmp" opcode
	movl	#ieintx,a0@(0x120+2)
	movl	sp@+,a0
	rts

ieintx:	
	moveml	#0xFFFE,sp@-
	clrl	sp@-			| unit# 0
| entire 'C' routine executed at level 2

	.globl	ieintr
	jsr	ieintr

	addql	#4,sp
	moveml	sp@+,#0x7FFF
	rte

|
| copy bytes, using movb,movw, or movl as appropriate.
|

	.globl	bcopy
bcopy:

	movl	sp@(4),d0
	movl	d0,a0
	movl	d0,d1
	movl	sp@(8),d0
	movl	d0,a1
	orl	d0,d1
	movl	sp@(12),d0
	bles	0$
	orl	d0,d1
	btst	#0,d1
	beqs	2$
	subql	#1,d0
1$:	movb	a0@+,a1@+
	dbra	d0,1$
0$:	rts

2$:	btst	#1,d0
	beqs	4$
	asrl	#1,d0
	subql	#1,d0
3$:	movw	a0@+,a1@+
	dbra	d0,3$
	rts

4$:	asrl	#2,d0
	subql	#1,d0
5$:	movl	a0@+,a1@+
	dbra	d0,5$
	rts


|
| zero bytes
|

	.globl	bzero
bzero:

	movl	sp@(4),a0
	movl	sp@(8),d0
	subql	#1,d0
	moveq	#0,d1
1$:	movb	d1,a0@+
	dbra	d0,1$
	rts


|
| compare bytes
|

	.globl	bcmp
bcmp:

	movl	sp@(4),a0
	movl	sp@(8),a1
	movl	sp@(12),d0
	subql	#1,d0
1$:	cmpmb	a0@+,a1@+
	dbne	d0,1$
	bnes	2$
	moveq	#0,d0
	rts
2$:	moveq	#1,d0
	rts

|
| Return the 1 s complement checksum of the word aligned buffer
| at s, for n bytes.
|
| in_cksum(s,n) 
| u_short *s; int n;
	

	.globl	in_cksum
in_cksum:

	movl	sp@(4),a0
	movl	sp@(8),d1
	asrl	#1,d1
	subql	#1,d1
	clrl	d0
1$:
	addw	a0@+,d0
	bccs	2$
	addqw	#1,d0
2$:
	dbra	d1,1$
	notw	d0
	rts
