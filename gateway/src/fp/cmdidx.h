/*
 * Copyright (c) 1985, 1986 Kinetics, Inc.
 *
 * $Header: /usr/fp/include/fp/RCS/cmdidx.h,v 1.6 86/02/20 19:54:20 root Exp $
 *
 *	Indices into the jump table for PROM routines.
 *	Also, the command numbers for the routines when invoked
 *	from Kinetics LAP type command packets.
 *	The values of these constants are actually determined by
 *	their position in the jump table in .../proms/kfpx.s.
 *	See cmdmacro.h for the actual calls through the jump table.
 *	After the indices are defined, structure definitions for
 *	each call's pointer parameter are also defined..
 */

#define	X_VERSION	 0
#define	X_EXECUTE	 1
#define	X_ACK		 2
#define	X_REBOOT	 3
#define	X_SET68		 4
#define	X_SET86		 5
#define	X_ATINIT	 6
#define	X_ATWRITE	 7
#define	X_WAIT		 8
#define	X_WHEREIS	 9
#define	X_GETMEM	10
#define	X_PROTECT	11
#define	X_COPYMEM	12
#define	X_CLRMEM	13
#define	X_PROMRAM	14
#define	X_RESET		15
#define	X_USER0		16
#define	X_USER1		17
#define	X_USER2		18
#define	X_USER3		19
#define	X_USER4		20
#define	X_USER5		21
#define	X_USER6		22
#define	X_USER7		23
#define	X_BUFINIT	24
#define	X_BUFGET	25
#define	X_BUFFREE	26
#define	X_BUFENQ	27
#define	X_BUFDEQ	28
#define	X_ERR		29
#define	X_IDLE		30
#define	X_KLAP		31
#define	X_WHO		32
#define	X_CA86		33
#define	X_RES86		34
#define	X_CLRINT	35
#define	X_RCSID		36
#define	X_EXPROM	37
#define	X_INIPROM	38
#define	X_RES8530	39
#define	X_DMTSERV	40
#define	X_SREC		41
#define	X_SPL		42
#define	X_NOTIMP	43

/* structures for the above commands */

/* X_VERSION */
struct fp_version {
	short	fpv_entries;	    /* number of valid entries in jump table */
	short	fpv_version;	    /* version of the prom code */
	long	fpv_model;	    /* "KFPS", "KFPQ", or "KFPM" */
};

/* X_EXECUTE */
/* no parameters */

/* X_ACK */
/* no parameters */

/* X_REBOOT */
/* no parameters */

/* X_SET68 */
/* no parameters */

/* X_SET86 */
/* this structure is really defined in ie.h, but it should look like this */
#ifdef	NEVER
struct scb {
	unsigned short	sc_status;	/* status */
	unsigned short	sc_cmd;		/* command */
	unsigned short	sc_clist;	/* command list */
	unsigned short	sc_rlist;	/* receive frame list */
	unsigned short	sc_crcerrs;	/* crc errors */
	unsigned short	sc_alnerrs;	/* alignment errors */
	unsigned short	sc_rscerrs;	/* resource errors (lack of rfd/rbd's) */
	unsigned short	sc_ovrnerrs;	/* overrun errors (mem bus not avail) */
};
#endif	NEVER

/* X_ATINIT */
/* The parameter is a pointer to a short which is filled in with
   the node number chosen by the AppleTalk initialization sequence */

/* X_ATWRITE */
struct fp_atwrite {
	short	fpw_length;	/* number of bytes in hte following string */
	unsigned char *fpw_str;	/* string to be output onto the network */
};

/* X_WAIT */
/* The parameter is a long integer which indicates
   the number of milliseconds to delay */

/* X_WHEREIS */
struct fp_whereis {
	char	*fpw_rom0;	/* first address of 1st ROM */
	char	*fpw_rom1;	/* first address of 2nd ROM */
	char	*fpw_8530;	/* address of Zilog 8530 chip */
	char	*fpw_0port;	/* address of 1st 8-bit port */
	char	*fpw_1port;	/* address of 2nd 8-bit port */
	char	*fpw_ram2;	/* address of 1st RAM location */
	char	*fpw_ramtop;	/* address of last RAM location */
};

/* X_GETMEM and X_CLRMEM and X_BUFINIT */
struct fp_mem {
	short	fpm_count;	/* number of bytes */
	char	*fpm_memp;	/* addr of allocated or to be cleared memory */
};

/* X_PROTECT */
struct fp_proelem {		/* protection array element */
	short	fpp_count;	/* number of bytes to protect */
	char	*fpp_memp;	/* address of memory to protect */
	short	fpp_chksum;	/* checksum of protected memory */
};

struct fp_protect {
	short	fpt_oper;	/* type of protection operation and result */
	short	fpt_count;	/* number of elements in protection array */
	struct fp_proelem *fpt_elem; /* addr 1st element in protection array */
};

/* fpt_oper operations */
#define	PR_FAIL		-1	/* result: check of checksums failed */
#define	PR_PASS		0	/* result: check of checksums passed */
#define	PR_PROTECT	1	/* operation: start protect via elem array */
#define	PR_CHECK	2	/* operation: verify protect via elem array */
#define	PR_CANCEL	3	/* operation: cancel protect via elem array */

/* X_COPYMEM */
struct fp_copy {
	char	*fpc_from;	/* location to copy bytes from */
	char	*fpc_to;	/* location to copy bytes to */
	short	fpc_count;	/* number of bytes to copy */
};

/* X_PROMRAM */
struct fp_bufinfo {
	struct pbuf **fpb_pfree;/* beginning of the free list */
	struct pqueue *fpb_pq;	/* received buffers queue (both networks) */
	struct pqueue *fpb_sendq;/* Ethernet transmit queue */
	short	fpb_bsize;	/* size of a buffer including header */
	short	fpb_pbnfree;	/* pbufs on our free list */
	short	fpb_pbndrops;	/* times failed to find space */
	short	fpb_pbntypes[16];/* type specific pbuf allocations */
};

struct fp_state {
	unsigned short	fps_atnet;  /* current net number for AppleTalk side */
	unsigned short	fps_etnet;  /* current net number for Ethernet side */
	unsigned char	fps_valid;  /* following AppleTalk node number valid */
	unsigned char	fps_node;   /* current AppleTalk LAP node number */
	unsigned char	fps_netw;   /* network number (not used) */
	unsigned char	fps_bridge; /* last known bridge num on net if any */
	unsigned char	fps_ether[6]; /* current ethernet address */
#define	SNAMESIZE	21	    /* extra char for null at end */
	char	fps_name[SNAMESIZE];/* ascii name of the gateway */
	char	fps_file[SNAMESIZE];/* ascii name of srec file last loaded */
	unsigned char	fps_pforce; /* non-zero forces execution of prom loop */
	unsigned char	fps_reserve;
	unsigned char	fps_unused[70];
};

struct fp_abstats {
	int	fpa_interrupts;	/* Appletalk interrupts */
	int	fpa_ipackets;	/* packets received */
	int	fpa_opackets;	/* packets transmitted */
	int	fpa_crc;	/* crc errors */
	int	fpa_ovr;	/* receive overrun errors */
	int	fpa_iund;	/* receive underrun errors */
	int	fpa_xx;
	int	fpa_yy;
	int	fpa_bad;	/* bad packets received */
	int	fpa_coll;	/* collisions */
	int	fpa_defer;	/* times deferred to other packets */
	int	fpa_idleto;	/* packets that timed out waiting for the end */
	int	fpa_zz;
	int	fpa_nodata;	/* packets without data (nothing after rts) */
	int	fpa_ound;	/* transmit underrun errors */
	int	fpa_badddp;	/* bad ddp packets */
	int	fpa_spur;	/* spurrious interrupts */
};

struct fp_iestats {
	struct scb *fpi_scbptr;	/* status control block */
	int	fpi_ipackets;	/* input packets */
	int	fpi_opackets;	/* output packets */
	int	fpi_ierrors;	/* input errors */
	int	fpi_oerrors;	/* output errors */
	int	fpi_cmdbusy;	/* busy waits */
};

struct fp_promram {
	short	fpr_count;	    	/* number of valid ptrs that follow */
	char	*fpr_jtable;	    	/* jump table */
	struct fp_bufinfo *fpr_bufs;	/* buffer manager vector */
	struct fp_state *fpr_state;	/* Prompt program's state vector */
	struct fp_abstats *fpr_abstats;	/* AppleTalk statistics vector */
	struct fp_iestats *fpr_iestats;	/* Ethernet statistics vector */
	char	*fpr_1debug;	    	/* first level debug flag */
	char	*fpr_2debug;	    	/* second level debug flag */
	char	*fpr_3debug;	    	/* third level debug flag */
	char	*fpr_unused[24];    	/* remaining ptrs are not defined yet */
};

/* X_RESET */
/* no parameters */

/* X_USER0, X_USER1, X_USER2, X_USER3, X_USER4, X_USER5, X_USER6, X_USER7 */
/* parameters (if any) defined by user */

/* X_BUFGET */
struct fp_bget {
	struct pbuf *fpg_buf;	/* address of buffer returned */
	short	fpg_type;	/* type of buffer to be "gotten" */
};

/* X_BUFFREE */
struct fp_bfree {
	struct pbuf *fpf_buf;	/* address of buffer to be freed */
	struct pbuf *fpf_nxt;	/* the freed buffer's link before it was freed*/
};

/* X_BUFENQ and  X_BUFDEQ */
struct fp_bqueue {
	struct pqueue *fpq_q;	/* addr of queue to enqueue to / dequeue from */
	struct pbuf *fpq_buf;	/* addr of buffer to be enqueued or dequeued */
};

/* X_ERR */
/* no parameters */

/* X_IDLE */
/* no parameters */

/* X_KLAP */
/* The parameter is the address of a pbuf structure. See pbuf.h. */

/* X_WHO */
/* no parameters */

/* X_CA86 */
/* no parameters */

/* X_RES86 */
/* no parameters */

/* X_CLRINT */
/* no parameters */

/* X_RCSID */
/* The parameter is the address of a character array which is filled
   in with the current RCS identification string of this PROM version. */

/* X_EXPROM */
/* no parameters */

/* X_INIPROM */
/* no parameters */

/* X_RES8530 */
/* no parameters */

/* X_DMTSERV */
/* no parameters */

/* X_SREC */
/* The parameter is the address of a character array which contains
   the S-record to be interpreted and used by the gateway */

/* X_SPL */
/* The parameter is the address short which becomes the next processor priority
   level. The previous priority level is returned at the same location */

/* X_NOTIMP */
/* no parameters */

