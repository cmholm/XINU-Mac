/*
 * RTMP / ZIP etc.
 *
 * (c) 1986, Stanford Univ. CSLI.
 * May be used but not sold without permission.
 *
 * $Header$
 */

#include "gw.h"
#include "gwctl.h"
#include "fp/pbuf.h"
#include "ab.h"
#include "inet.h"
#include "fp/cmdmacro.h"

#include "glob.h"
extern u_char broadcastaddr[];


short rtmp_delay, rtmp_vdelay;		/* 'static' */

/*
 * RTMP clock routine.  
 * Broadcasts RTMP's on AppleTalk segment, age routes.
 */
rtmptimer()
{
	register struct aroute *ar;

	if (rtmp_delay && ++rtmp_delay < 11)
		return;	/* run only every 10 seconds */
	rtmp_delay = 1;
	/* broadcast the current routing table */
	rtmpsend(0xFF, rtmpSkt, 1);
	if (rtmp_vdelay++ == 0)	/* validity timer goes off every 20 secs */
		return;
	rtmp_vdelay = 0;
	/* age routes in our table */
	for (ar = &aroute[0] ; ar < &aroute[NAROUTE] ; ar++) {
		if (ar->net == 0 || ar->hops == 0 || (ar->flags & arouteAA))
			continue;
		if (arouteBad(ar)) {
			ar->net = 0;	/* delete it */
			continue;
		}
		if (ar->age < 255)
			ar->age++;
	}
}


/*
 * Send an RTMP packet to dnode, dsoc.  If 'tuples' is true, 
 * include the routing tuples.
 */
rtmpsend(dnode, dsoc, tuples)
{
	register struct pbuf *p;
	register struct RTMP *r;
	struct DDPS d;
	register i;
	u_char dst, xx;

	K_PGET(PT_DATA, p);
	if (p == 0)
		return;
	r = (struct RTMP *)(p->p_off + lapSize + ddpSSize);
	r->net = ifab.if_dnet;
	r->idLen = 8;
	r->id = ifab.if_dnode;
	if (tuples)
		i = rtmpsettuples((caddr_t)(r + 1));
	else
		i = 0;
	d.length = i + ddpSSize + rtmpSize;
	p->p_len = d.length + lapSize;
	d.dstSkt = dsoc;
	d.srcSkt = rtmpSkt;
	dst = dnode;
	d.type = ddpRTMP;
	bcopy((caddr_t)&d, p->p_off+lapSize, ddpSSize);
	(*ifab.if_output)(&ifab, p, AF_SDDP, &dst);
}


/*
 * Merge new routing tuples into our aroute table.
 */
rtmpmerge(cp, count, sender)
	register char *cp;	/* start of tuple array [3][n] */
	int count;		/* count of tuples */
	int sender;		/* node of tuple sender */
{
	register struct aroute *ar;
	register struct RTMPTuple *t;
	struct aroute *arfree;
	char tarray[4];

	t = (struct RTMPTuple *)&tarray[0];
	arfree = 0;
	for ( ; count > 0 ; count--) {
		tarray[0] = *cp++;
		tarray[1] = *cp++;
		tarray[2] = *cp++;
		for (ar = &aroute[0] ; ar < &aroute[NAROUTE] ; ar++) {
			if (ar->net == 0) { /* remember a free slot */
				if (arfree == 0)
					arfree = ar;
				continue;
			}
			/* search for this tuple net in table */
			if (t->net == ar->net)
				goto found;
		}
		goto add;	/* not in table, add it */
found:
		if (arouteIP(ar) || ar->hops == 0)
			continue;	/* dont replace local or IP nets */
		if (arouteBad(ar) && t->hops < 15)
			goto replace;
		if (ar->hops >= (t->hops+1) && t->hops < 15)
			goto replace;
		if (ar->node == sender) {
			if ((ar->hops = t->hops + 1) < 16)
				ar->age = 0;
			else
				ar->net = 0;
		}
		continue;
add:
		/* tuple net # wasnt in our table, add it */
		if ((ar = arfree) == 0)
			continue;	/* oops, out of room */
		arfree = 0;
replace:
		ar->net = t->net;
		ar->hops = t->hops + 1;
		ar->node = sender;
		ar->age = ar->flags = ar->zone = 0;
	}
}


/*
 * Setup a tuple array, starting at cp;  returns total size 
 * of the array in bytes.
 */
rtmpsettuples(cp)
	register char *cp;
{
	register count;
	char tarray[4];
	register struct RTMPTuple *t;
	register struct aroute *ar;

	t = (struct RTMPTuple *)&tarray[0];
	count = 0;
	for (ar = &aroute[0] ; ar < &aroute[NAROUTE] ; ar++) {
		if (ar->net == 0 || arouteBad(ar))
			continue;
		t->net = ar->net;
		t->hops = ar->hops;
		*cp++ = tarray[0];
		*cp++ = tarray[1];
		*cp++ = tarray[2];
		count += 3;
	}
	return (count);
}


/*
 * RTMP received from AppleTalk.
 */
rtmpinput(p)
	struct pbuf *p;
{
	register struct RTMP *r = (struct RTMP *)p->p_off;
	register tbytes, tuples;
	u_char dst;

	if (wasddp || p->p_len < 1)
		goto drop;
	if (ddps.type == ddpRTMPR) {
		/* RTMP request from Mac trying to get his net # */
		if (*p->p_off != 1)
			goto drop;	/* only opcode defined now is 1 */
		rtmpsend(lap.src, ddps.srcSkt, 0);
		goto drop;
	}
	if (p->p_len < sizeof *r 
	    || (tbytes = ddps.length - ddpSSize - sizeof *r) <= 0
	    || r->idLen != 8
	    || r->id == ifab.if_dnode)
		goto drop;
	if ((tuples = tbytes/3)) /* if we have any tuples, merge them in */
		rtmpmerge((caddr_t)(r+1), tuples, r->id);
drop:
	K_PFREE(p);
}


/*
 * Send arouteTuple's to gateway at 'ia'.
 * If 'all' flag is true, sends all routes not configured by AA.
 * If 'all' is 0, only sends locally discovered routes (new atalk
 * segments dynamically plugged in).
 */
artsend(ia, all)
	iaddr_t ia;
	register all;
{
	register struct aroute *ar;
	register struct arouteTuple *at;
	register struct aaconf *m;
	struct udp u;
	struct pbuf *p;
	register count;

	count = 0;
	K_PGET(PT_DATA, p);
	if (p == 0)
		return;
	m = (struct aaconf *)(p->p_off + sizeof (struct ip) + sizeof u);
	at = (struct arouteTuple *)m->stuff;
	for (ar = &aroute[0] ; ar < &aroute[NAROUTE] ; ar++) {
		if (ar->net == 0 || ar->hops == 0)
			continue;	/* skip null & local entries */
		if (all) {
			if (ar->flags & arouteAA)
				continue;	/* skip AA entries */
		} else {
			if (arouteIP(ar))
				continue;	/* skip IP entries */
		}
		if (arouteIP(ar)) {
			at->node = ar->node;
			at->flags = ar->flags;
		} else {
			/* if node was an atalk address, subst. our IP addr */
			at->node = conf.ipaddr;
			at->flags = arouteKbox;
		}
		at->net = ar->net;
		at->hops = ar->hops;
		at++;
		count++;
	}
	if (count == 0 && all)
		goto drop;	/* dont sent null aaROUTE packets */
	/* (but we would send a null aaROUTEQ query packet) */
	m->magic = aaMagic;
	m->type = (all ? aaROUTE : aaROUTEQ);
	m->flags = 0;
	m->count = count * sizeof *at;
	m->ipaddr = conf.ipaddr;
	u.src = u.dst = aaPort;
	u.length = m->count + aaconfMinSize + sizeof u;
	u.checksum = 0;
	*(struct udp *)(p->p_off + sizeof (struct ip)) = u;
	p->p_len = u.length + sizeof (struct ip);
	setiphdr(p, ia);
	routeip(p, 0, 0);
	return (1);
drop:
	K_PFREE(p);
	return (0);
}


short	art_delay;		/* 'static' */
struct aroute *art_last;	/* 'static', last aroute pointer */

/*
 * arouteTuple timer routine.  Once per minute send arouteTuple's
 * to 'core' gateways.
 */
arttimer()
{
	register struct aroute *ar;
	register count;

	if (art_delay && ++art_delay < 60)
		return;	/* run only every 60 seconds */
	art_delay = 1;
	if (arouteinit == 0) {
		/* if routes not yet received from AA */
		confrequest(aaROUTEI);
	}
	/*
	 * send local additions to one of the core's,
	 * chosen circularly.
	 */
	if ((ar = art_last) == 0)
		ar = &aroute[0];
	for (count = 0 ; count < NAROUTE ; count++) {
		ar++;
		if (ar >= &aroute[NAROUTE])
			ar = &aroute[0];
		if (ar->net == 0 || (ar->flags & arouteCore) == 0)
			continue;
		art_last = ar;
		artsend(ar->node, 0);
		return;
	}
	/* else no core gateways found */
	return;
}


/*
 * arouteTuples received from another gateway or AA.
 * Merge into our table and send reply if requested.
 */
artinput(aa, ipsrc)
	register struct aaconf *aa;
	iaddr_t ipsrc;
{
	register struct arouteTuple *at;
	register struct aroute *ar;
	register count, type;
	struct aroute *arfree;

	if ((type = aa->type) == aaROUTEI) {
		/* if init table from AA, clear all previous entries */
		for (ar = &aroute[0] ; ar < &aroute[NAROUTE] ; ar++)
			if(ar->flags & arouteAA)
				ar->flags = ar->net = 0;
	}
	at = (struct arouteTuple *)aa->stuff;
	arfree = 0;
	for (count = aa->count / sizeof *at ; count > 0 ; count--, at++) {
		for (ar = &aroute[0] ; ar < &aroute[NAROUTE] ; ar++) {
			if (ar->net == 0) {
				if (arfree == 0)
					arfree = ar;
				continue;
			}
			if (at->net == ar->net)
				goto found;
		}
		goto add;	/* not in table, add it */
found:
		if (at->net == conf.atnete) {
			/* found net number of our ether */
			if (type != aaROUTEI)
				continue;
			if (at->flags & arouteCore)
				aroutecore = 1; /* remember if we are a core*/
			if ((at->flags & arouteHost)
			    && at->node != conf.ipaddr) {
				ar->node = at->node;
				ar->flags = at->flags;
			}
			continue;
		}
		if (ar->hops == 0 || arouteIP(ar) == 0)
			continue;	/* dont replace local entries */
		/* more tests? */
		goto replace;
add:
		if ((ar = arfree) == 0)
			continue;	/* no free slots */
		arfree = 0;
replace:
		ar->net = at->net;
		ar->node = at->node;
		ar->flags = at->flags;
		if (type == aaROUTEI)
			ar->flags |= arouteAA;
		ar->hops = at->hops + 1;
		ar->zone = ar->age = 0;
	}
	if (type == aaROUTEQ)
		artsend(ipsrc, 1);
	else if (type == aaROUTEI) {
		artsend(ipsrc, 0);
		arouteinit = 1;
	}
}


/*
 * Get the aroute entry corresponding to the atalk net number
 * supplied.  Returns 0 if routed via ether, otherwise the bridge's
 * node number on our atalk segment (or -1 if local segment).
 */
getaroute(net)
	register net;
{
	register struct aroute *ar;

	for (ar = &aroute[0] ; ar < &aroute[NAROUTE] ; ar++) {
		if (ar->net == 0)	/* empty slot */
			continue;
		if (ar->net == net) {
			if (arouteIP(ar))
				return (0);
			return (ar->node ? ar->node : -1);
		}
	}
	/* not found */
	return (0);
}


/*
 * Process ZIP packet received on input.
 */
zipinput(p)
	struct pbuf *p;
{
	register struct ATP *a;
	register u_char *cp;
	register count, zlen, len;
	u_short *sp;
	struct ZIP *z;
	struct pbuf *p2;
	union {
		u_short s;
		u_char c[2];
	} u;
	struct DDPS d;
	u_char dst, xx;

	if (ddp.type == ddpATP)
		goto get;	/* if ATP style request */
	/* else pure ZIP in a short DDP */
	if (ddp.type != ddpZIP || wasddp)
		goto drop;
	z = (struct ZIP *)p->p_off;
	if (z->command == zipQuery)
		goto query;
	/* if (z->command == zipTakedown)
		goto takedown;
	... */
	goto drop;
query:
	K_PGET(PT_DATA, p2);
	if (p2 == 0)
		goto drop;
	cp = p2->p_off + lapSize + ddpSSize + sizeof *z;
	sp = (u_short *)(z+1);
	zlen = strlen(conf.zone);
	for (count = 0, len = 0 ; count < z->count && len < 512 ; count++) {
		u.s = *sp++;
		*cp++ = u.c[0];
		*cp++ = u.c[1];
		*cp++ = zlen;
		bcopy(conf.zone, cp, zlen);
		cp += zlen;
		len += (zlen + 3);
	}
	z = (struct ZIP *)(p2->p_off + lapSize + ddpSSize);
	z->command = zipReply;
	z->count = count;
	d.length = len + sizeof *z + ddpSSize;
	p2->p_len = d.length + lapSize;
	d.dstSkt = ddps.srcSkt;
	d.srcSkt = zipSkt;
	d.type = ddpZIP;
	dst = lap.src;
	bcopy((caddr_t)&d, p2->p_off+lapSize, ddpSSize);
	(*ifab.if_output)(&ifab, p2, AF_SDDP, &dst);
	goto drop;
get:
drop:
	K_PFREE(p);
}
