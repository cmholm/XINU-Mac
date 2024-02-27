/*
 *  AppleTalk definitions.
 *
 *  Copyright (c) 1984, Apple Computer Inc.
 *  Gene Tyacke, Alan Oppenheimer, G. Sidhu, Rich Andrews.
 *
 *  C language version (c) 1984, Stanford Univ. SUMEX project.
 *  May be used but not sold without permission.
 *
 *  (c) 1986, Kinetics, Inc.
 *  May be used but not sold without permission.
 *
 *  $Header$
 */

/*
 * history
 * 08/24/84	GRT	Created.
 * 10/23/84	GRT	
 * 12/01/84	Croft	Created C version;  added packet headers, *Params.
 */


struct LAP {			/* LAP */
	u_char	dst;
	u_char	src;
	u_char	type;
};

typedef struct LAP LAPAdrBlock;	/* LAPAdrBlock */
#define	dstNodeID	dst
#define	srcNodeID	src
#define	lapProtType	type

/* LAP definitions */
#define	lapDDPS		1	/* short DDP type */
#define	lapShortDDP	1
#define	lapDDP		2	/* DDP type */
#define	lapSize		3	/* size of lap header */

typedef struct {		/* AddrBlock */
	u_short	net;
	u_char	node;
	u_char	skt;
} AddrBlock;


struct DDP {			/* DDP */
	u_short	length;
	u_short	checksum;
	u_short	dstNet;
	u_short	srcNet;
	u_char	dstNode;
	u_char	srcNode;
	u_char	dstSkt;
	u_char	srcSkt;
	u_char	type;
};

struct DDPS {			/* DDPS */
	u_short	length;
	u_char	dstSkt;
	u_char	srcSkt;
	u_char	type;
};

/* DDP definitions */
#define	ddpMaxWKS	0x7F
#define	ddpMaxData	586
#define	ddpLengthMask	0x3FF
#define	ddpHopShift	10
#define	ddpSize		13	/* size of DDP header */
#define	ddpSSize	5
#define	ddpWKS		128	/* boundary of DDP well known sockets */
#define	ddpRTMP		1	/* RTMP type */
#define	ddpRTMPR	5	/* RTMP 'request' */
#define	ddpNBP		2	/* NBP type */
#define	ddpATP		3	/* ATP type */
#define	ddpZIP		6	/* ZIP type */
#define	ddpIP		22	/* IP type */
#define	ddpARP		23	/* ARP type */

typedef struct {		/* WDS (write data structure) */
	u_short	size;
	u_char	*ptr;
} WDS;

#ifdef	MAC

struct DDPParam {		/* DDP CSParam control / status calls */
	u_char	sktNum;
	u_char	checksum;
	Ptr	address;	/* socket listener or WDS */
};

/* DDP CSCodes */
#define	ddpWrite	246
#define	ddpCloseSkt	247
#define	ddpOpenSkt	248

#endif	MAC


struct ZIP {			/* ZIP */
	u_char	command;
	u_char	count;
};
#define	zipSkt		6	/* number of ZIP socket */
#define	zipQuery	1
#define	zipReply	2
#define	zipTakedown	3
#define	zipBringup	4


struct RTMP {			/* RTMP */
	u_short	net;
	u_char	idLen;
	u_char	id;		/* start of ID field */
};

struct RTMPtuple {
	u_short	net;
	u_char	hops;
};
#define	rtmpSkt	1		/* number of RTMP socket */
#define	rtmpSize	4	/* minimum size */
#define	rtmpTupleSize	3
#define	RTMPTuple	RTMPtuple


typedef struct {		/* NBPTuple */
	AddrBlock addr;
	u_char	enume;
	u_char	name[3];	/* minimum length */
} NBPTuple;

struct NBP {			/* NBP */
	u_char	control;
	u_char	id;
	NBPTuple tuple;
};

typedef struct {		/* Entity, unpacked */
	u_char	obj[34];
	u_char	type[34];
	u_char	zone[34];
} Entity;

#define	nbpControlMask	0xF0
#define	nbpCountMask	0x0F
#define	nbpBrRq		0x10
#define	nbpLkUp		0x20
#define	nbpLkUpReply	0x30
#define	nbpNIS		2
#define	nbpMinSize	(sizeof (struct NBP) -3)
#define	nbpEquals	'='
#define	nbpStar		'*'
	

struct ATP {			/* ATP */
	u_char	control;
	u_char	bitmap;
	u_short	transID;
	long	userData;
};
#define	atpReqCode	0x40
#define	atpRspCode	0x80
#define	atpRelCode	0xC0
#define	atpXO		0x20
#define	atpEOM		0x10
#define	atpSTS		0x08
#define	atpFlagMask	0x3F
#define	atpControlMask	0xF8
#define	atpMaxNum	8

#ifdef	MAC
struct ATPParam {		/* CSParam for ATP request */
	u_char	socket;
	u_char	flags;
	AddrBlock address;
	u_short	size;
	Ptr	buffer;
	BDS	bds;
	u_char	atpBitmap;
	u_char	atpTimeout;
	u_short	atpTID;
};
#define	atpNBuffs	atpBitmap
#define	atpBDSSize	atpTimeout
#define	atpRetryCnt	atpTID
#define	atpNResps	(atpTID>>8)
/* ATP CSCodes */
#define	atpRelRspCB	249
#define	atpCloseSkt	250
#define	atpAddResponse	251
#define	atpSendResponse	252
#define	atpGetRequest	253
#define	atpOpenSkt	254
#define	atpSendRequest	255
#define	atpRelTCB	256
#endif	MAC


typedef struct {		/* RetransType */
	u_char	retransInterval;
	u_char	retransCount;
} RetransType;

typedef struct {		/* BDSElement */
	u_short	buffSize;
	u_char	*buffPtr;
	u_short	dataSize;
	long	userBytes;
} BDSElement;

typedef struct {		/* BDSType */
	BDSElement a[8];
} BDSType;
