/*
 * Gateway control packets.
 *
 * (c) 1986, Stanford, BBN, Kinetics.
 * May be used but not sold without permission.
 *
 * $Header$
 */


#define	rebPort	902		/* rebroadcast port */


/*
 * Gateway debug protocol (via ddt68 on UNIX).
 */
struct gwdb {
	u_long	magic;		/* magic number */
	u_char	op,seq;		/* op code, sequence number */
	u_short	count;		/* byte count */
	u_long	address;	/* address of read/write */
	u_char	data[512];
};

#define	gwdbMagic	0xFF068020
#define	gwdbPort	900	/* udp port number */
/* op codes */
#define	gwdbRead	1
#define	gwdbWrite	2
#define	gwdbCall	3


/*
 * Appletalk administration packets from appletalk administrator
 * host (AA) or other gateways; configuration / routing information packet.
 */
struct aaconf {
	u_long	magic;		/* magic number */
	u_char	type;		/* op code */
	u_char	flags;
	u_short	count;		/* byte count of 'stuff' */
	iaddr_t	ipaddr;		/* IP address of sender */
	u_char	stuff[512];	/* config info or routing tuples */
};
#define	aaconfMinSize	12
#define	aaPort	901		/* udp port number (at gateway) */
#define	aaMagic	0xFF068030
/* type codes */
#define aaCONF		1	/* config request/reply */
#define	aaROUTEI	2	/* initial route table from AA */
#define	aaROUTE		3	/* route table update */
#define aaROUTEQ	4	/* route table update and query */
#define	aaRESTART	5	/* force restart/reconfigure */


/*
 * Routing tuple.
 */
struct arouteTuple {
	long	node;		/* IP net or node address */
	u_short	net;		/* atalk net number */
	u_char	flags;		/* flags, see below */
	u_char	hops;		/* hop count */
};
/* flag values */
#define	arouteBMask	0x03	/* directed broadcast format mask */
#define	arouteKbox	0x80	/* 'node' is IP address of a Kbox */
#define	arouteNet	0x40	/* node is an IP net#, allowing directed
				   broadcasts in the format given by BMask */
#define	arouteHost	0x20	/* node is an IP host with capability to
				   rebroadcast on its local net */
#define	arouteCore	0x10	/* node is a 'core' gateway */
#define	arouteAA	0x08	/* this entry received via AA (admin host)*/


/*
 * ATP packet used by client MacIP programs to request
 * name assignment and lookup services.
 */
struct IPGP {
	u_long	op;		/* opcode */
	long	ipaddress;	/* my IP address (or lookup reply)*/
	long	ipname;		/* address of my name server */
	long	ipbroad;	/* my broadcast address */
	long	ipfile;		/* my file server */
	long	ipother[4];	/* other addresses/flags */
	char	string[128];	/* null terminated name/error string */
};
#define	ipgpMinSize	36
/* op codes */
#define	ipgpAssign	1	/* assign new IP address */
#define	ipgpName	2	/* name lookup */
#define	ipgpServer	3	/* just return my server addresses */
#define	ipgpError	-1	/* error return op code; string=message */


/*
 * Stuff dealing with IP/DDP address conversion.
 * WKS = well known socket (DDP term).
 */
#define	ddpWKSUnix	768		/* start of WKS range on UNIX */
#define	ddpNWKSUnix	16384		/* start of non-WKS .. */
#define	ddpIPSkt	72		/* used by Dartmouth encapsulation */

#define	ddp2ipskt(dskt) (((dskt) & 0x80) ? (dskt) + ddpNWKSUnix \
			 : (dskt) + ddpWKSUnix)
/* caution ip2ddpskt can't be used to test if socket is in magic range */
#define ip2ddpskt(iskt) (((iskt) > ddpNWKSUnix) ? (iskt) - ddpNWKSUnix \
			 : (iskt) - ddpWKSUnix)
