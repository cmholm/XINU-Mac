/*
 *  AppleTalk / Ethernet gateway definitions.
 *
 *  (c) 1984, Stanford Univ. SUMEX project.
 *  May be used but not sold without permission.
 *
 *  (c) 1986, Kinetics, Inc.
 *  May be used but not sold without permission.
 *
 *  $Header$
 */

#include "mung_gw.h"

#ifdef noprepend
#define	end	_end
#define	begin	_begin
#define	edata	_edata
#endif

typedef	unsigned char	u_char;
typedef	unsigned short	u_short;
typedef	unsigned int	u_int;
typedef	unsigned long	u_long;
typedef unsigned short	n_short;
typedef unsigned long	n_time;

#define	ntohs(a) (a)
#define	ntohl(a) (a)
#define	htons(a) (a)
#define	htonl(a) (a)

typedef	char *	caddr_t;		/* 'core' address type */
typedef long iaddr_t;			/* internet address type */

#define	NULL	0


/*
 * Configuration structure.
 */
struct conf {
	iaddr_t	ipaddr;			/* IP address of gateway */
	iaddr_t	ipadmin;		/* addr of admin host */
	iaddr_t	iproutedef;		/* default route */
	u_char	etheraddr[6];		/* ethernet hardware address */
	u_short	ready;			/* ready flag */
#define	confPrefix	20		/* manditory config prefix length */
#define	confReady	0x1234		/* magic flag value in 'ready' field*/
	iaddr_t	ipbroad;		/* broadcast addr on ether */
	iaddr_t	ipname;			/* address of name server */
	iaddr_t	ipdebug;		/* address of debug host */
	iaddr_t	ipfile;			/* address of file server */
	u_long	ipother[4];		/* other addresses passed via IPGP */
	u_short	ipsmask;		/* subnet mask (currently unused) */
	u_short	ipsshift;		/* subnet shift (unused) */
	u_long	flags;			/* various bit flags */
	u_short	ipstatic;		/* number of static IP addrs */
	u_short	ipdynamic;		/* number of dynamic IP addrs */
	u_short	atneta;			/* atalk net #, appletalk */
	u_short	atnete;			/* atalk net #, ethernet */
	u_char	zone[32];		/* my atalk zone name */
};


/*
 * Network interface structure.
 */
struct ifnet {
	char	if_name[8];
	short	if_unit;
	iaddr_t	if_addr;		/* IP address */
	short	if_addrform;		/* ARP address format for IP */
	short	if_haddrlen;		/* hardware address length */
	short	if_haddrform;		/* ARP hardware addr format */
	int	(*if_output)();		/* packet output routine */
	int	(*if_matchus)();	/* match us routine */
 	u_short	if_dnet;		/* ddp net # */
	u_char	if_dnode;		/* ddp node # */
	u_char	if_xxx;
	char	if_haddr[8];		/* big enough to hold any haddr */
	iaddr_t	if_addrnet;		/* IP address, net part */
	struct	ifnet *if_next;		/* next if */
};

/*
 * Address families understood by interface output encapsulation 
 * routine (*if_output)();
 */
#define	AF_IP		0		/* address family IP */
#define	AF_DDP		1		/* .. .. DDP */
#define	AF_LINK		2		/* link level (Ethernet or AppleTalk) */
#define	AF_ARP		3		/* address resolution proto */
#define	AF_SDDP		4		/* short DDP */
#define	AF_RTMP		5		/* RTMP */
/* Private field(s) used in packet buffer header */
#define	p_if(p)	(*(struct ifnet **)&((p)->p_head[0])) /* interface ptr */
/* fast copy of ether address, assumes address on an even boundary */
#define eaddrcopy(src,dst) { \
	*((long *)dst) = *((long *)src); \
	*((short *)(((char *)dst) + 4)) = *((short *)(((char *)src) + 4));}


/*
 * AppleTalk route table.
 */
struct aroute {
	long	node;			/* atalk node number OR IP addr */
	u_short	net;			/* atalk net number, 0 if unused */
	u_char	flags;			/* flags */
	u_char	hops;			/* # hops to this net */
	u_char	zone;			/* index+1 of slot in azone */
	u_char	age;			/* age of entry in 20 second ticks */
};
#define	NAROUTE	32			/* max # table entries */
#define arouteIP(ar) ((ar)->node & 0xFFFFFF00) /* true if route via internet*/
#define	arouteBad(ar) (arouteIP(ar) ? ((ar)->age > 15) : ((ar)->age > 1))
/* also see flags defined in struct arouteTuple (file gwctl.h) */


/*
 * Zone name table.
 *
 * Aroute table contains an index (+1) into the first dimension
 * of this array.  Each slot in the table is azoneSlot bytes
 * long.  The zone name is stored as a Pascal string (bytecount, string),
 * begining at a slot boundary.  If the string is longer than one slot,
 * it continues into the next slot.  A slot is "free" if its first byte
 * is zero.
 */
#define	NAZONE		32	/* max number of zone name slots */
#define	azoneSlot	8	/* slot size */
/* char azone[NAZONE][azoneSlot]; */


/*
 * IP dynamic address assignment table.
 *
 * Index into the table is the offset from conf.ipaddr + conf.ipstatic.
 */
struct ipdad {
	u_short	net;		/* appletalk net number of last user */
	u_char	node;
	u_char	flags;		/* flags */
	u_short	timer;		/* ticks since last used */
};
#define	NIPDAD		32	/* max number of table entries */
#define	ipdadTimerMax	0x7FFF
#define	ipdadTimerMin	5	/* unused for 5 minutes before reassigned */


/*
 * Statistics.
 */
struct stats {
	int	dropabin;		/* dropped ab in pkts */
	int	dropiein;
	int	droprouteip;		/* pkts dropped by routeip */
	int	droprouteddp;
};


/*
 * IP address macros.
 */
#define	ipsub(ia)	(((ia)>>ipsubshift) & ipsubmask)
#define	ipbroadcast(ia)	(((ia) & 0xFF) == 0xFF)	/* Jeff would complain */
#define	ipnetpart(ia)	(IN_CLASSA((ia)) ? ((ia) & IN_CLASSA_NET) : \
	(IN_CLASSB((ia)) ? ((ia) & IN_CLASSB_NET) : ((ia) & IN_CLASSC_NET)))
#define	iphostpart(ia)	(IN_CLASSA((ia)) ? ((ia) & IN_CLASSA_HOST) : \
	(IN_CLASSB((ia)) ? ((ia) & IN_CLASSB_HOST) : ((ia) & IN_CLASSC_HOST)))
