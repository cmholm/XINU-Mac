/*
 * Constants and structures related to memory allocation
 *
 * Copyright (c) 1985, 1986 Kinetics, Inc.
 *
 * $Header: /usr/fp/include/fp/RCS/pbuf.h,v 1.3 86/10/19 18:01:05 tim Exp $
 */

#define	MAXHEAD		128		/* max space for header */
#define	MAXDATA		630		/* max space for data */

struct pbuf {
	struct	pbuf *p_next;		/* next buffer in queue */
	u_char	*p_off;			/* offset of data */
	short	p_len;			/* amount of data in this pbuf */
	short	p_type;			/* pbuf type (0 == free) */
	u_char	p_head[MAXHEAD];	/* header storage */
	u_char	p_data[MAXDATA];	/* data storage */
};
/* WARNING: assembler constants in lap.s use this pbuf structure */

/* pbuf types (p_type) */
#define	PT_FREE		0	/* should be on free list */
#define	PT_ABUS		1	/* received AppleTalk packet */
#define	PT_ENET		2	/* received Ethernet packet */
#define	PT_ARP		3	/* ARP packet */
#define	PT_DATA		4	/* general data */
#define	PT_ERBF		5	/* awaiting Ethernet receive packet */
#define	PT_ETBF		6	/* awaiting Ethernet transmit complete */

struct pqueue {
	struct pbuf *pq_head;
	struct pbuf *pq_tail;
	short	pq_len;
};
