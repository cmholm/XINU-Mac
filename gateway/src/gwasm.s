|  (c) 1984, Stanford Univ. SUMEX project.
|  May be used but not sold without permission.
|
|  (c) 1986, Kinetics, Inc.
|  May be used but not sold without permission.

| $Header: /usr/fp/src/fps/ram.udp/RCS/gwasm.s,v 1.13 86/02/21 20:39:43 tim Rel $

include(`fp/kfps.h')

|
| one time initialization to setup vectors, etc.
|
ifdef(`flexnames',`
	.globl	asminit, table_ptr
asminit:
| Just catch the exceptions we want to handle
	movl	a0,sp@-
	movl	table_ptr,a0
',`
	.globl	_asminit, _table_p
_asminit:
| Just catch the exceptions we want to handle
	movl	a0,sp@-
	movl	_table_p,a0
')
	movw	`#'0x4ef9,a0@(m4_ieservice)	| "jmp" opcode
	movl	`#'ieintx,a0@(m4_ieservice+2)
	movl	sp@+,a0
	rts

ieintx:	
	moveml	#0xFFFE,sp@-
	clrl	sp@-			| unit# 0
| entire 'C' routine executed at level 2
ifdef(`flexnames',`
	.globl	ieintr
	jsr	ieintr
',`
	.globl	_ieintr
	jsr	_ieintr
')
	addql	#4,sp
	moveml	sp@+,#0x7FFF
	rte

|
| copy bytes, using movb,movw, or movl as appropriate.
|
ifdef(`flexnames',`
	.globl	bcopy
bcopy:
',`
	.globl	_bcopy
_bcopy:
')
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
ifdef(`flexnames',`
	.globl	bzero
bzero:
',`
	.globl	_bzero
_bzero:
')
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
ifdef(`flexnames',`
	.globl	bcmp
bcmp:
',`
	.globl	_bcmp
_bcmp:
')
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
	
ifdef(`flexnames',`
	.globl	in_cksum
in_cksum:
',`
	.globl	_in_cksu
_in_cksu:
')
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
