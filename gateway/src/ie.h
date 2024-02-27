/*
 *  Intel 82586 registers and structures.
 *
 *  (c) 1984, Stanford Univ. SUMEX project.
 *  May be used but not sold without permission.
 *
 *  (c) 1986, Kinetics, Inc.
 *  May be used but not sold without permission.
 *
 */

/*
 * Byte Swapping explained:
 *	When data is accessed a character at a time there is no
 *	problem, but when data is accessed as a short (2 bytes)
 *	or a long (4 bytes), the 68000 and the i82586 have
 *	different ways of ordering the bytes. The 68000 accesses
 *	the most significant byte as the even address and the
 *	i82586 accesses the least significant byte as the even
 *	address. Therefore, every read/write of shorts and longs
 *	must be reversed end-to-end by the 68000. All this boils
 *	down to the point that the following structure declarations
 *	look just like the way they are documented in the Intel manuals,
 *	but because those structures contain 16- and 32-bit fields,
 *	conversion must be done by the 68000 on all read/writes to
 *	those fields. This way data is always correct for the i82596.
 *	When OR'ing and AND'ing, don't forget that the data in the
 *	structures is already "swapped" as it were.
 */

/*
 * buffer descriptor.
 */
struct bd {
	u_short	bd_count;	/* data count */
	u_short	bd_next;	/* link to next */
	u_short	bd_buf;		/* buffer pointer */
	u_short	bd_bufhi;
	u_short	bd_size;	/* buffer size (rbd only) */
};

/* bd_count */
#define	BD_EOF		0x8000	/* end of frame */
#define	BD_F		0x4000	/* filled by 82586 */
#define	BD_COUNT	0x3FFF	/* count field */
/* bd_size */
#define	BD_EL		0x8000	/* end of list */

/*
 * transmit buffer
 */
struct t_bd {
	struct bd tbd;		/* the buffer descriptor */
	struct pbuf *tpbuf;	/* address of the pbuf used for transmit data */
};

/*
 * receive buffer
 */
struct r_bd {
	struct bd rbd;		/* the buffer descriptor */
	struct pbuf *rpbuf;	/* address of the pbuf used for receive data */
};

/*
 * Command block / receive frame descriptor.
 */
struct cb {
	u_short	cb_status;	/* status */
	u_short	cb_cmd;		/* command */
	u_short	cb_link;	/* link to next */
	u_short	cb_param[6];	/* different parameters here */
};

/* cb_status */
#define	CB_COMPLETE	0x8000	/* complete */
#define	CB_BUSY		0x4000	/* busy */
#define	CB_OK		0x2000	/* ok */
#define	CB_ABORT	0x1000	/* aborted */
/* cb_cmd */
#define	CB_EL		0x8000	/* end of list */
#define	CB_S		0x4000	/* suspend */
#define	CB_I		0x2000	/* hardware interrupt when done */
/* cb_link */
#define	CB_NIL		0xffff	/* empty pointer */
/* action commands */
#define	CBC_NOP		0	/* no operation */
#define	CBC_IASETUP	1	/* individual address setup */
#define	CBC_CONFIG	2	/* configure */
#define	CBC_TRANS	4	/* transmit */
#define	CBC_DUMP	6	/* dump internal regs */

/*
 * frame descriptor structure
 */
struct fdes {
	u_short	fd_status;	/* status */
	u_short	fd_stat2;	/* 2 more stat bits */
	u_short	fd_link;	/* link offset */
	u_short fd_rbd;		/* rbd offset */
	u_char	fd_daddr[6];	/* destination address */
	u_char	fd_saddr[6];	/* source address */
	u_short	fd_type;	/* type field of received frame */
};

/* fd_status */
#define	FD_C		0x8000	/* complete */
#define	FD_B		0x4000	/* busy */
#define	FD_OK		0x2000	/* frame completed ok */
/* fd_stat2 */
#define	FD_EL		0x8000	/* end of list */
#define	FD_S		0x4000	/* suspend after receive */
/* fd_rbd */
#define	FD_NIL		0xffff	/* empty pointer */

/*
 * System control block, plus some driver static structures.
 */
struct scb {
	u_short	sc_status;	/* status */
	u_short	sc_cmd;		/* command */
	u_short	sc_clist;	/* command list */
	u_short	sc_rlist;	/* receive frame list */
	u_short	sc_crcerrs;	/* crc errors */
	u_short	sc_alnerrs;	/* alignment errors */
	u_short	sc_rscerrs;	/* resource errors (lack of rfd/rbd's) */
	u_short	sc_ovrnerrs;	/* overrun errors (mem bus not avail) */
};

/* sc_status bits set by 82586 */
#define	STAT_CX		0x8000	/* command executed */
#define	STAT_FR		0x4000	/* frame received */
#define	STAT_CNA	0x2000	/* cmd unit went active */
#define	STAT_RNR	0x1000	/* rec unit left ready */
#define	STAT_IDLE	0x0000	/* no status bits */

/* sc_cmd bits ack'd by CPU */
#define	ACK_CX		0x8000	/* command executed */
#define	ACK_FR		0x4000	/* frame received */
#define	ACK_CNA		0x2000	/* cmd unit went active */
#define	ACK_RNR		0x1000	/* rec unit left ready */
#define	ACK_NONE	0x0000	/* no status bits */

/* command unit status */
#define	CUS_IDLE	0x000	/* idle */
#define	CUS_SUSP	0x100	/* suspended */
#define	CUS_ACTIVE	0x200	/* ready */
#define	SC_CUS		0x700	/* command unit status field */

/* receive unit status */
#define	RUS_IDLE	0x0	/* idle */
#define	RUS_SUSP	0x10	/* suspended */
#define	RUS_NORES	0x20	/* no resources */
#define	RUS_READY	0x40	/* ready */
#define	SC_RUS		0x70	/* receive unit status field */

/* command unit commands */
#define	CUC_NOP		0x000	/* nop */
#define	CUC_START	0x100	/* start */
#define	CUC_RES		0x200	/* resume */
#define	CUC_SUSP	0x300	/* suspend */
#define	CUC_ABORT	0x400	/* abort */

/* receive unit commands */
#define	RUC_NOP		0x00	/* nop */
#define	RUC_START	0x10	/* start */
#define	RUC_RES		0x20	/* resume */
#define	RUC_SUSP	0x30	/* suspend */
#define	RUC_ABORT	0x40	/* abort */

/* odd man out */
#define	SC_RESET	0x0080	/* software reset */

/*
 * intermediate system configuration pointer
 */
struct iscp {
	u_char	is_busy;	/* lo order byte is busy byte */
	u_char	is_test;	/* byte used in testing */
	u_short is_scb_off;	/* lo order 16 bits of scb address */
				/* base for scb and all other types of blocks */
	u_short	is_lo_scb_base;	/* lo order 16 bits of scb base */
	u_short	is_hi_scb_base;	/* hi order 8 bits of scb base (in lsb) */
};
#define	ISCPBUSY	0x01	/* cleared by 82586 when reset accepted */

/*
 * system configuration pointer
 * 10 (decimal) bytes from 0xFFFFF6 to 0xFFFFFF
 */
struct scp {
	u_short	scp_sysbus;	/* lo order byte is sysbus byte */
	u_short	scp_sp1;	/* unused */
	u_short scp_sp2;	/* unused by 82586 but used by 68000 */
	u_short	scp_lo_iscp;	/* lo order 16 bits of iscp address */
	u_short	scp_hi_iscp;	/* hi order 8 bits of iscp address (in lsb) */
};
#define	SYSBUS_8	0x01	/* indicates 8-bit bus in scp_sysbus */
#define	SYSBUS_16	0x00	/* indicates 16-bit bus in scp_sysbus */

/*
 * macros to extract address pieces...
 */
#define	HI16(addr)	((short)(((int)(addr)) >> 16) & 0xffff)
#define	LO16(addr)	((short)((int)(addr)) & 0xffff)

/*
 * swap bytes in a 16-bit value at compile time whenever possible
 */
#define	SWAB(jj)	(((jj & 0xff) << 8) | ((jj & 0xff00) >> 8))

struct	iscp	k_iscp;
struct	 scb	k_scb;
struct	  cb	k_cb;
struct	fdes	k_fdes;

#define	SCPADDR		((struct  scp *) 0xfffff6)
#define	ISCPADDR	(&k_iscp)
/*
 * base address for scb, command and receive frame descriptor blocks
 */
#define	SCBBASE		((int)&k_scb & 0xff0000)
#define	SCBADDR		((struct  scb *)(SCBBASE + LO16(&k_scb)))
#define	CMDADDR		((struct   cb *)(SCBBASE + LO16(&k_cb)))
#define	FDESADDR	((struct fdes *)(SCBBASE + LO16(&k_fdes)))

/*
 * convert an i82586 16-bit offset to a 68000 24-bit address
 */
#define	OTOA(type,off)	((type)(SCBBASE + SWAB(off)))

