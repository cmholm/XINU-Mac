/*
 * Copyright (c) 1985, 1986 Kinetics, Inc.
 *
 * $Header: /usr/fp/include/fp/RCS/cmdmacro.h,v 1.5 86/02/14 19:06:06 tim Exp $
 *
 * The PROM loader guarantees the following to be true when the
 * downloaded program begins executing.
 *
 * CPU is in supervisor state,
 * Interrupt service is at level 7
 * 'sp' contains the address of the boot prom 'jump_table' defined below
 */

#define NUM_STACK 20
#define NUM_ADDRS 64
#define NUM_JUMPS 64

struct fp_table {
	struct {
		short opcode;
		long dst_addr;
		} jump_table[NUM_JUMPS];

	int (*subr_table[NUM_ADDRS])();
};

extern struct fp_table *table_ptr;

/* get the position dependent command indices for the X_nnn constants */
#ifdef lint
#include "cmdidx.h"
#else
#include "fp/cmdidx.h"
#endif

#define	K_VERSION(a)	table_ptr->subr_table[X_VERSION](a)
#define	K_EXECUTE()	table_ptr->subr_table[X_EXECUTE]()
#define	K_ACK()		table_ptr->subr_table[X_ACK]()
#define	K_REBOOT()	table_ptr->subr_table[X_REBOOT]()
#define	K_SET68()	table_ptr->subr_table[X_SET68]()
#define	K_SET86(a)	table_ptr->subr_table[X_SET86](a)
#define	K_ATINIT(a)	table_ptr->subr_table[X_ATINIT](a)
#define	K_ATWRITE(a)	table_ptr->subr_table[X_ATWRITE](a)
#define	K_WAIT(a)	table_ptr->subr_table[X_WAIT](a)
#define	K_WHEREIS(a)	table_ptr->subr_table[X_WHEREIS](a)
#define	K_GETMEM(a)	table_ptr->subr_table[X_GETMEM](a)
#define	K_PROTECT(a)	table_ptr->subr_table[X_PROTECT](a)
#define	K_COPYMEM(a)	table_ptr->subr_table[X_COPYMEM](a)
#define	K_CLRMEM(a)	table_ptr->subr_table[X_CLRMEM](a)
#define	K_PROMRAM(a)	table_ptr->subr_table[X_PROMRAM](a)
#define	K_RESET()	table_ptr->subr_table[X_RESET]()
#define	K_USER0(a)	table_ptr->subr_table[X_USER0](a)
#define	K_USER1(a)	table_ptr->subr_table[X_USER1](a)
#define	K_USER2(a)	table_ptr->subr_table[X_USER2](a)
#define	K_USER3(a)	table_ptr->subr_table[X_USER3](a)
#define	K_USER4(a)	table_ptr->subr_table[X_USER4](a)
#define	K_USER5(a)	table_ptr->subr_table[X_USER5](a)
#define	K_USER6(a)	table_ptr->subr_table[X_USER6](a)
#define	K_USER7(a)	table_ptr->subr_table[X_USER7](a)
#define	K_BUFINIT(a)	table_ptr->subr_table[X_BUFINIT](a)
#define	K_BUFGET(a)	table_ptr->subr_table[X_BUFGET](a)
#define	K_BUFFREE(a)	table_ptr->subr_table[X_BUFFREE](a)
#define	K_BUFENQ(a)	table_ptr->subr_table[X_BUFENQ](a)
#define	K_BUFDEQ(a)	table_ptr->subr_table[X_BUFDEQ](a)
#define	K_ERR()		table_ptr->subr_table[X_ERR]()
#define	K_IDLE()	table_ptr->subr_table[X_IDLE]()
#define	K_KLAP(a)	table_ptr->subr_table[X_KLAP](a)
#define	K_WHO()		table_ptr->subr_table[X_WHO]()
#define	K_CA86()	table_ptr->subr_table[X_CA86]()
#define	K_RES86()	table_ptr->subr_table[X_RES86]()
#define	K_CLRINT()	table_ptr->subr_table[X_CLRINT]()
#define	K_RCSID(a)	table_ptr->subr_table[X_RCSID](a)
#define	K_EXPROM()	table_ptr->subr_table[X_EXPROM]()
#define	K_INIPROM()	table_ptr->subr_table[X_INIPROM]()
#define	K_RES8530()	table_ptr->subr_table[X_RES8530]()
#define	K_DMTSERV()	table_ptr->subr_table[X_DMTSERV]()
#define	K_SREC(a)	table_ptr->subr_table[X_SREC](a)
#define	K_SPL(a)	table_ptr->subr_table[X_SPL](a)
#define	K_NOTIMP()	table_ptr->subr_table[X_NOTIMP]()

/* macros for buffer operations */
#define	K_PGET(type,buf) { \
	struct fp_bget bg; \
	bg.fpg_type = (type); \
	K_BUFGET(&bg); \
	(buf) = bg.fpg_buf; \
}

#define	K_PFREE(buf) { \
	struct fp_bfree bf; \
	bf.fpf_buf = (buf); \
	K_BUFFREE(&bf); \
}

#define	K_PFREEN(buf,nxt) { \
	struct fp_bfree bf; \
	bf.fpf_buf = (buf); \
	K_BUFFREE(&bf); \
	(nxt) = bf.fpf_nxt; \
}

#define	K_PENQ(level,head,buf) { \
	struct fp_bqueue bq; \
	short pr; \
	bq.fpq_q = (head); \
	bq.fpq_buf = (buf); \
	pr = (level); \
	K_SPL(&pr);	 \
	K_BUFENQ(&bq); \
	K_SPLX(&pr); \
}

#define	K_PENQNP(head,buf) { \
	struct fp_bqueue bq; \
	bq.fpq_q = (head); \
	bq.fpq_buf = (buf); \
	K_BUFENQ(&bq); \
}

#define	K_PDEQ(level,head,buf) { \
	struct fp_bqueue bq; \
	short pr; \
	bq.fpq_q = (head); \
	pr = (level); \
	K_SPL(&pr);	 \
	K_BUFDEQ(&bq); \
	K_SPL(&pr); \
	(buf) = bq.fpq_buf; \
}

#define	K_PDEQNP(head,buf) { \
	struct fp_bqueue bq; \
	bq.fpq_q = (head); \
	K_BUFDEQ(&bq); \
	(buf) = bq.fpq_buf; \
}

/* macros for various spl options */
#define SPL0	0x2000
#define SPLIE	0x2200
#define SPLABUS	0x2500
#define SPLIMP	0x2700

#define	K_SPL0(a) { \
	*(a) = SPL0; \
	K_SPL(a); \
}

#define	K_SPLIE(a) { \
	*(a) = SPLIE; \
	K_SPL(a); \
}
#define	K_SPLABUS(a) { \
	*(a) = SPLABUS; \
	K_SPL(a); \
}

#define	K_SPLIMP(a) { \
	*(a) = SPLIMP; \
	K_SPL(a); \
}

#define	K_SPLX(a) { \
	K_SPL(a); \
}
