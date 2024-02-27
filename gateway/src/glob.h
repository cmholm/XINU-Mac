/*
 *  AppleTalk / Ethernet gateway globals.
 *
 *  (c) 1984, Stanford Univ. SUMEX project.
 *  May be used but not sold without permission.
 *
 *  (c) 1986, Kinetics, Inc.
 *  May be used but not sold without permission.
 *
 *  $Header$
 */

struct ifnet ifie;		/* interface struct, intel ethernet */
struct ifnet ifab;		/* interface struct, applebus */
struct ifnet *ifnet;		/* head of ifnet list */

struct conf conf;		/* site configuration parameters */
iaddr_t ipnet;			/* major IP net number */
short ipid;			/* next value in ip_id field */

struct DDP ddp;			/* last ddp unpacked by abreceive */
struct LAP lap;			/* last lap */
struct DDPS ddps;		/* last short ddp */
short wasddp;			/* true if ddp, else ddps */

u_char atnode;			/* gateway's AppleTalk node number */
u_char etheraddr[6];		/* our Ethernet address */
struct pqueue *pq;		/* main received packet queue */
struct pqueue *sendq;		/* ethernet send queue */
struct pqueue nbpq;		/* NBP BrRq's waiting for transmission */
u_short in_cksum();

#ifdef SEAGATE
#define	msclock	(*(long *)0x278)	/* ms clock */
#endif
#ifdef KINETICS
int msclock;
struct fp_promram pvars;	/* the addresses of some prom variables */
struct fp_table *table_ptr;	/* pointer to PROM routines' jump table */
struct pbuf *bufs;		/* beginning of the pbuf structs */
int topram;			/* highest ram address available to us */
#endif

struct aroute aroute[NAROUTE];	/* AppleTalk route table */
short arouteinit;		/* have read initial table from AA */
short aroutecore;		/* I am a core gateway */
struct ipdad ipdad[NIPDAD];	/* IP dynamic address table */
struct stats stats;		/* misc dropped packet statistics */
