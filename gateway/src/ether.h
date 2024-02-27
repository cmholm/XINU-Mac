/*
 *  C language version (c) 1984, Stanford Univ. SUMEX project.
 *  May be used but not sold without permission.
 *
 *  (c) 1986, Kinetics, Inc.
 *  May be used but not sold without permission.
 *
 *  $Header: /usr/fp/src/fps/ram.udp/RCS/ether.h,v 1.13 86/02/21 20:39:08 tim Rel $
 */

/*
 * Ethernet address - 6 octets
 */
struct ether_addr {
	u_char	ether_addr_octet[6];
};

/*
 * Structure of a 10Mb/s Ethernet header.
 */
struct	ether_header {
	struct	ether_addr ether_dhost;
	struct	ether_addr ether_shost;
	u_short	ether_type;
};

#define	ETHERTYPE_PUPTYPE	0x0200		/* PUP protocol */
#define	ETHERTYPE_IPTYPE	0x0800		/* IP protocol */
#define ETHERTYPE_ARPTYPE	0x0806		/* Addr. resolution protocol*/
/* kinetics/apple official ethernet types from xerox */
#define	ETHERTYPE_ATALKTYPE	0x809b	/* an appletalk packet on the ether */
#define	ETHERTYPE_ATARPTYPE	0x809c	/* an appletalk-ARP packet - not used */

#define	ETHERMTU	1500
#define	ETHERMIN	(60-14)
