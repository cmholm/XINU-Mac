/*
 *  (c) 1986, Stanford Univ. CSLI.
 *  May be used but not sold without permission.
 *
 *  (c) 1986, Kinetics, Inc.
 *  May be used but not sold without permission.
 *
 *  $Header$
 */

/*
 * Address resolution protocol.
 */

#include "gw.h"
#include "fp/pbuf.h"
#include "ab.h"
#include "inet.h"
#include "fp/cmdmacro.h"

#include "glob.h"

/*
 * See RFC 826 (see "arp.rfc") for protocol description.
 * Field names used correspond to RFC 826.
 */

#define	ARPHLNMAX	6	/* largest arp_hln value needed */
#define	ARPPLN		4	/* length of protocol address (IP) */

struct	arp {
	u_short	arp_hrd;	/* format of hardware address */
	u_short	arp_pro;	/* format of proto. address  */
	u_char	arp_hln;	/* length of hardware address  */
	u_char	arp_pln;	/* length of protocol address  */
	u_short	arp_op;
#define	ARPOP_REQUEST	1	/* request to resolve address */
#define	ARPOP_REPLY	2	/* response to previous request */
	u_char	arp_d[ARPHLNMAX*2+ARPPLN*2];
				/* contains 4 packed fields:
				   sender hardware/proto addresses,
				   target hardware/proto addresses */
};
#define	arp_sha(ea)	(&(ea)->arp_d[0])
#define	arp_spa(ea)	(&(ea)->arp_d[hln])
#define	arp_tha(ea)	(&(ea)->arp_d[hln+ARPPLN])
#define	arp_tpa(ea)	(&(ea)->arp_d[hln+hln+ARPPLN])
#define	sizeof_arp	(sizeof(struct arp) - ARPHLNMAX*2  + hln + hln)


/*
 * Internet to hardware address resolution table.
 */
struct	arptab {
	iaddr_t at_iaddr;		/* internet address */
	u_char	at_haddr[ARPHLNMAX];	/* hardware address */
	u_char	at_timer;		/* minutes since last reference */
	u_char	at_flags;		/* flags */
	struct	pbuf *at_hold;		/* last packet until resolved/timeout */
};
/* at_flags field values */
#define	ATF_INUSE	1		/* entry in use */
#define ATF_COM		2		/* completed entry (haddr valid) */

#define	ARPTAB_BSIZ	5		/* bucket size */
#define	ARPTAB_NB	11		/* number of buckets */
#define	ARPTAB_SIZE	(ARPTAB_BSIZ * ARPTAB_NB)

struct	arptab *arptnew();

struct	arptab arptab[ARPTAB_SIZE];

#define	ARPTAB_HASH(a) \
	((short)((((a) >> 16) ^ (a)) & 0x7fff) % ARPTAB_NB)

#define	ARPTAB_LOOK(at,addr) { \
	register n; \
	at = &arptab[ARPTAB_HASH(addr) * ARPTAB_BSIZ]; \
	for (n = 0 ; n < ARPTAB_BSIZ ; n++,at++) \
		if (at->at_iaddr == addr) \
			break; \
	if (n >= ARPTAB_BSIZ) \
		at = 0; }

int	arpt_age;		/* aging timer */

/* timer values */
#define	ARPT_AGE	(60*1)	/* aging timer, 1 min. */
#define	ARPT_KILLC	20	/* kill completed entry in 20 mins. */
#define	ARPT_KILLI	3	/* kill incomplete entry in 3 minutes */


u_char	broadcastaddr[6] = { 0xff, 0xff, 0xff, 0xff, 0xff, 0xff };

/*
 * Timeout routine.  Age arptab entries once a minute.
 * Entered once per second.
 */
arptimer()
{
	register struct arptab *at;
	register i;

	if (++arpt_age > ARPT_AGE) {
		arpt_age = 0;
		at = &arptab[0];
		for (i = 0; i < ARPTAB_SIZE; i++, at++) {
			if (at->at_flags == 0)
				continue;
			if (++at->at_timer < ((at->at_flags&ATF_COM) ?
			    ARPT_KILLC : ARPT_KILLI))
				continue;
			/* timer has expired, clear entry */
			arptfree(at);
		}
	}
}

/*
 * Broadcast an ARP packet, asking who has addr on interface ifp.
 */
arpwhohas(ifp, addr)
	register struct ifnet *ifp;
	iaddr_t *addr;
{
	register struct pbuf *p;
	register struct arp *ea;
	register hln;
	
	if (ifp == &ifab) {
		nbpwhohasip(*addr);
		return;
	}
	hln = ifp->if_haddrlen;
	K_PGET(PT_ARP,p); /* get a buffer from the free list */
 	if (p == 0)
		return;
	p->p_len = sizeof_arp;
	ea = (struct arp *)p->p_off;
	bzero((caddr_t)ea, sizeof_arp);
	ea->arp_hrd = htons(ifp->if_haddrform);
	ea->arp_pro = htons(ifp->if_addrform);
	ea->arp_hln = hln;	/* hardware address length */
	ea->arp_pln = ARPPLN;	/* protocol address length */
	ea->arp_op = htons(ARPOP_REQUEST);
	bcopy(ifp->if_haddr, arp_sha(ea), hln);
	bcopy((caddr_t)&ifp->if_addr, arp_spa(ea), ARPPLN);
	bcopy((caddr_t)addr, arp_tpa(ea), ARPPLN);
	(*ifp->if_output)(ifp, p, AF_ARP, broadcastaddr);
}

/*
 * Resolve an IP address into a hardware address.  If success, 
 * destha is filled in and 1 is returned.  If there is no entry
 * in arptab, set one up and broadcast a request 
 * for the IP address;  return 0.  Hold onto this pbuf and 
 * resend it once the address is finally resolved.
 */
arpresolve(ifp, p, destip, destha)
	register struct ifnet *ifp;
	struct pbuf *p;
	register iaddr_t *destip;
	register u_char *destha;
{
	register struct arptab *at;
	register hln = ifp->if_haddrlen;
	int lna = ntohl(*(long *)destip) & 0xFF;

	if (lna == 0xFF || lna == 0x0) { /* broadcast address */
		bcopy(broadcastaddr, destha, hln);
		return (1);
	}
	/*
	 * We used to do some (conservative) locking here at splimp, since
	 * arpinput was called  from input interrupt service.
	 *	
	 *	s = splimp();
	 */
	ARPTAB_LOOK(at, *destip);
	if (at == 0) {			/* not found */
		at = arptnew(destip);
		at->at_hold = p;
		arpwhohas(ifp, destip);
		/* splx(s); */
		return (0);
	}
	at->at_timer = 0;		/* restart the timer */
	if (at->at_flags & ATF_COM) {	/* entry IS complete */
		bcopy(at->at_haddr, destha, hln);
		/* splx(s); */
		return (1);
	}
	/*
	 * There is an arptab entry, but no hardware address
	 * response yet.  Replace the held pbuf with this
	 * latest one.
	 */
 	if (at->at_hold) {
 		/* put buffer back on the free list */
		K_PFREE(at->at_hold);
 	}
	at->at_hold = p;
	arpwhohas(ifp, destip);		/* ask again */
	/* splx(s); */
	return (0);
}


/*
 * Called when packet containing ARP is received.
 * Algorithm is that given in RFC 826.
 * In addition, a sanity check is performed on the sender
 * protocol address, to catch impersonators.
 */
arpinput(p)
	struct pbuf *p;
{
	register struct ifnet *ifp = p_if(p);
	register struct arp *ea;
	register struct arptab *at = 0;  /* same as "merge" flag */
	register hln = ifp->if_haddrlen;
	struct pbuf *phold;
	iaddr_t isaddr,itaddr,myaddr;

	if (p->p_len < sizeof_arp)
		goto out;
	myaddr = ifp->if_addr;
	ea = (struct arp *) p->p_off;
	if (ntohs(ea->arp_pro) != ifp->if_addrform)
		goto out;
	bcopy(arp_spa(ea), (caddr_t)&isaddr, ARPPLN);
	bcopy(arp_tpa(ea), (caddr_t)&itaddr, ARPPLN);
	if (!bcmp(arp_sha(ea), ifp->if_haddr, hln))
		goto out;	/* it's from me, ignore it. */
	if (isaddr == myaddr) {
		int i;
		u_char *cp = arp_sha(ea);
 		sendf("duplicate IP address %x!! sent from hardware address: ");
		for (i = 0 ; i < hln ; i++)
			sendf("%x ", *cp++);
		if (ntohs(ea->arp_op) == ARPOP_REQUEST) {
			itaddr = myaddr;
			goto reply;
		}
		goto out;
	}
	ARPTAB_LOOK(at, isaddr);
	if (at) {
		bcopy(arp_sha(ea), at->at_haddr, hln);
		at->at_flags |= ATF_COM;
		if (at->at_hold) {
			phold = at->at_hold;
			at->at_hold = 0;
			(*ifp->if_output)(ifp, phold, AF_IP, &isaddr);
		}
	}
	if (itaddr != myaddr && (*ifp->if_matchus)(itaddr) == 0)
		goto out;	/* if I am not the target */
	if (at == 0) {		/* ensure we have a table entry */
		at = arptnew(&isaddr);
		bcopy(arp_sha(ea), at->at_haddr, hln);
		at->at_flags |= ATF_COM;
	}
	if (ntohs(ea->arp_op) != ARPOP_REQUEST)
		goto out;
reply:
	bcopy(arp_sha(ea), arp_tha(ea), hln);
	bcopy(arp_spa(ea), arp_tpa(ea), ARPPLN);
	bcopy(ifp->if_haddr, arp_sha(ea), hln);
	bcopy((caddr_t)&itaddr, arp_spa(ea), ARPPLN);
	ea->arp_op = htons(ARPOP_REPLY);
	(*ifp->if_output)(ifp, p, AF_ARP, arp_tha(ea));
	return;
out:
 	/* put buffer back on the free list */
	K_PFREE(p);
	return;
}

/*
 * Got an arp reply (via nbpinput).  If it completes one of our arptab
 * entries, send off the held buffer.  This subroutine is modeled after
 * the inner section of arpinput.
 */
arpgotreply(ifp, iaddr, haddr)
	register struct ifnet *ifp;
	iaddr_t iaddr;
	caddr_t haddr;
{
	register struct arptab *at;
	struct pbuf *p;

	ARPTAB_LOOK(at, iaddr);
	if (at == 0)	/* if no matching entry, nothing to do */
		return;
	bcopy(haddr, at->at_haddr, 4);
	at->at_flags |= ATF_COM;
	if (at->at_hold) {
		p = at->at_hold;
		at->at_hold = 0;
		(*ifp->if_output)(ifp, p, AF_IP, &iaddr);
	}
}

/*
 * Delete an arp cache entry.  Called from ipgassign when
 * new assignment made.
 */
arpdelete(iaddr)
	iaddr_t iaddr;
{
	register struct arptab *at;
	ARPTAB_LOOK(at, iaddr);
	if (at)
		arptfree(at);
}

/*
 * Free an arptab entry.
 */
arptfree(at)
	register struct arptab *at;
{
 	short pri;
 	
 	K_SPLIMP(&pri);
 	if (at->at_hold) {
 		/* put buffer back on the free list */
		K_PFREE(at->at_hold);
 	}

	at->at_hold = 0;
	at->at_timer = at->at_flags = 0;
	at->at_iaddr = 0;
	K_SPLX(&pri);
}

/*
 * Enter a new address in arptab, pushing out the oldest entry 
 * from the bucket if there is no room.
 */
struct arptab *
arptnew(addr)
	iaddr_t *addr;
{
	register n;
	int oldest = 0;
	register struct arptab *at, *ato;

	ato = at = &arptab[ARPTAB_HASH(*addr) * ARPTAB_BSIZ];
	for (n = 0 ; n < ARPTAB_BSIZ ; n++,at++) {
		if (at->at_flags == 0)
			goto out;	 /* found an empty entry */
		if (at->at_timer > oldest) {
			oldest = at->at_timer;
			ato = at;
		}
	}
	at = ato;
	arptfree(at);
out:
	at->at_iaddr = *addr;
	at->at_flags = ATF_INUSE;
	return (at);
}


