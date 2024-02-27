/*
 * AppleTalk / Ethernet Gateway;  part 2.
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

/*
 * NBP packet input from appletalk (abreceive).
 */
nbpinput(p)
	struct pbuf *p;
{
	register struct NBP *n = (struct NBP *)(p->p_off + lapSize + ddpSize);
	register op = (n->control & nbpControlMask);
	Entity ent;
	register iaddr;

	/*
	 * if bridge request, put on backround queue
	 * to nbpback, who will send all the LkUp's.
	 */
	if (op == nbpBrRq) {
		K_PENQNP(&nbpq, p);
		return;
	}
	/* else packet is a LkUp or LkUpReply, check for local names */
	nbpgetent(n->tuple.name, &ent);	/* break up tuple name entity */
	/*
	 * if lookup reply for us, complete any pending requests.
	 */
	if (op == nbpLkUpReply) {
		if (ddp.dstNode != ifab.if_dnode
		    || strcmp(ent.type, "IPADDRESS") != 0
		    || (iaddr = atoip(ent.obj, 99)) == 0 )
			goto drop;
		/* pass the address reply to ARP and ipdad modules */
		arpgotreply(&ifab, iaddr, &n->tuple.addr);
		ipdadreply(&ifab, iaddr, &n->tuple.addr);
		goto drop;
	}
	/*
	 * else lookup;  handle requests for our IP related services.
	 */
	if (strcmp(ent.type, "IPGATEWAY") == 0) {
		iaddr = conf.ipaddr;
		goto replyus;
	}
	if (strcmp(ent.type, "IPADDRESS") == 0) {
		if ((iaddr = atoip(ent.obj, 99)) == 0
		    || ipbroadcast(iaddr))
			goto drop;	/* dont reply to bad addresses */
		if (abmatch(iaddr))
			goto replyus;
	}
	if (strcmp(ent.type, "=") == 0 && strcmp(ent.obj, "=") == 0) {
		iaddr = conf.ipaddr;
		strcpy(ent.type, "IPGATEWAY");
		goto reply;
	}
	goto drop;
replyus:
	if (getaroute(n->tuple.addr.net) == 0)
		goto drop;	/* not our responsibility */
reply:
	iptoa(iaddr, ent.obj);
	ddp.length = nbpsetent(n->tuple.name, ent.obj, ent.type, ent.zone)
		+ ddpSize + nbpMinSize;
	ddp.checksum = 0;
	ddp.srcNet = ifab.if_dnet;
	ddp.srcNode = ifab.if_dnode;
	ddp.dstNet = n->tuple.addr.net;
	ddp.dstNode = n->tuple.addr.node;
	ddp.dstSkt = n->tuple.addr.skt;
	n->tuple.addr.net = ddp.srcNet;
	n->tuple.addr.node = ddp.srcNode;
	n->tuple.addr.skt = ddpIPSkt;
	ddp.srcSkt = nbpNIS;
	n->control = nbpLkUpReply + 1;
	p->p_len = ddp.length + lapSize;
	bcopy((caddr_t)&ddp, p->p_off+lapSize, ddpSize);
	routeddp(p, 0, 0);
	return;
drop:
	K_PFREE(p);
}


/*
 * Queue an NBP lookup request.  If net/node is non-zero,
 * does an 'nbpconfirm' operation.
 */
nbplookup(ent, net, node)
	Entity *ent;
{
	register struct NBP *n;
	register struct pbuf *p;
	struct DDP d;

	K_PGET(PT_DATA, p);
	if (p == 0)
		return;
	n = (struct NBP *)(p->p_off + lapSize + ddpSize);
	n->control = nbpLkUp + 1;
	n->id = 0;
	n->tuple.enume = 0;
	n->tuple.addr.net = d.srcNet = ifab.if_dnet;
	n->tuple.addr.node = d.srcNode = ifab.if_dnode;
	n->tuple.addr.skt = d.srcSkt = d.dstSkt = nbpNIS;
	d.checksum = 0;
	d.type = ddpNBP;
	d.length = nbpsetent(n->tuple.name, ent->obj, ent->type, ent->zone)
		+ ddpSize + nbpMinSize;
	p->p_off[2] = lapDDP;
	p->p_len = d.length + lapSize;
	if (net) {	/* wants an nbpconfirm */
		d.dstNet = net;
		d.dstNode = node;
	}
	bcopy((caddr_t)&d, p->p_off + lapSize, ddpSize);
	if (net) {
		bcopy((caddr_t)&d, (caddr_t)&ddp, ddpSize);
		routeddp(p, 0, 0);
	} else {
		K_PENQNP(&nbpq, p);
	}
}


struct aroute *nback_arnext;	/* 'static' */

/*
 * NBP 'backround'.  Check nbpq for BrRq's to be sent out as LkUp's.
 * Sends to one net per call/loop.
 */
nbpback()
{
	register struct aroute *ar;
	register struct pbuf *pq, *p;
	struct NBP *n;

	if ((pq = nbpq.pq_head) == 0)
		return;		/* if nothing to do */
	if (sendq->pq_head)
		return;		/* delay if ether sendq exists */
	if ((ar = nback_arnext) == 0) {	/* first time for this packet */
		ar = nback_arnext = &aroute[0];
		bcopy(pq->p_off + lapSize, (caddr_t)&ddp, ddpSize);
		ddp.checksum = 0;
		ddp.dstNode = 0xFF;	/* dstNet set below */
		/* ddp.srcNode = ifab.if_dnode; ddp.srcNet = ifab.if_dnet; */
		bcopy((caddr_t)&ddp, pq->p_off + lapSize, ddpSize);
		n = (struct NBP *)(pq->p_off + lapSize + ddpSize);
		n->control = nbpLkUp + 1;
	}
	/* for each net, route a packet there */
	for ( ; ar < &aroute[NAROUTE] ; ar++) {
		if (ar->net == 0)
			continue;
		K_PGET(PT_DATA, p);
		if (p == 0)
			return;	/* try later */
		/* copy efficiently in 4 byte (long) chunks */
		bcopy(pq->p_off, p->p_off, ((pq->p_len + 3) & 0xFFFC));
		p->p_len = pq->p_len;
		/*
		 * Set the net number directly into the ddp header,
		 * without copying.  Setup a few ddp.xxx globals for
		 * the convenience of 'routeddp'.
		 */
		p->p_off[7] = (ar->net >> 8);
		p->p_off[8] = ar->net;
		ddp.dstNet = ar->net;
		ddp.dstNode = 0xFF;
		ddp.dstSkt = ddp.srcSkt = nbpNIS;
		routeddp(p, 0, 0);
		nback_arnext = ar + 1;
		return;
	}
	/* done with this BrRq, dequeue it */
	K_PDEQNP(&nbpq, pq);
	K_PFREE(pq);
	nback_arnext = 0;
}


/*
 * Ask "who has" IP address addr, using NBP on my locally
 * connected segments.
 */
nbpwhohasip(addr)
	iaddr_t addr;
{
	Entity ent;

	iptoa(addr, ent.obj);
	strcpy(ent.zone, "*");
	strcpy(ent.type, "IPADDRESS");
	nbplookup(&ent, 0, 0);
}


/*
 * Unpack an nbp entity starting at cp.
 * Returns total size of entity (for skipping to next tuple).
 */
nbpgetent(cp, ent)
	register char *cp;
	register Entity *ent;
{
	char *oldcp;
	register i;

	oldcp = cp;
	i = (*cp & 0x1F); bcopy(cp+1, ent->obj, i); ent->obj[i] = 0; cp += i+1;
	i = (*cp & 0x1F); bcopy(cp+1, ent->type, i); ent->type[i] = 0; cp += i+1;
	i = (*cp & 0x1F); bcopy(cp+1, ent->zone, i); ent->zone[i] = 0; cp += i+1;
	return (cp - oldcp);
}


/*
 * Setup an nbp entity field with object, type, and zone strings.
 * Returns length of this tuple.
 */
nbpsetent(cp, obj, type, zone)
	register char *cp;
	char *obj, *type, *zone;
{
	register char *oldcp = cp;
	register i;

	*cp = i = strlen(obj);  bcopy(obj, cp+1, i);  cp += i+1;
	*cp = i = strlen(type);  bcopy(type, cp+1, i);  cp += i+1;
	*cp = i = strlen(zone);  bcopy(zone, cp+1, i);  cp += i+1;
	return (cp - oldcp);
}


/*
 * Process an IP packet directed to the gateway's own IP address.
 * These are usually control packets such as admin configure/
 * net table, debug packets, ICMP packets.
 */
ip4me(p)
	struct pbuf *p;
{
	register struct ip *ip = (struct ip *)p->p_off;
	int hlen = ip->ip_hl << 2;
	struct udp *u;
	register struct gwdb *g;
	register struct aaconf *m;
	struct icmp *ic;

	if (ip->ip_p == IPPROTO_ICMP)
		goto icmp;
	if (ip->ip_p == IPPROTO_UDP)
		goto udp;
drop:
	K_PFREE(p);
	return;
udp:
	u = (struct udp *)(p->p_off + hlen);
	switch (u->dst) {
	case aaPort:
		m = (struct aaconf *)(u+1);
		if (m->magic != aaMagic || ipRus(ip->ip_src) == 0)
			goto drop;	/* not from our friends */
		switch (m->type) {
		case aaCONF:
			confready(m);
			break;

		case aaROUTEI:
		case aaROUTE:
		case aaROUTEQ:
			artinput(m, ip->ip_src);
			break;

		case aaRESTART:
			K_EXECUTE();
			break;
		}
		goto drop;

	case gwdbPort:
		goto gwdb;

	case rebPort:
		if (ip->ip_dst != conf.ipaddr)
			goto drop;
		bcopy((caddr_t)(u+1) + lapSize, &ddp, ddpSize);
		p->p_off += hlen + sizeof *u;
		p->p_len -= hlen + sizeof *u;
		routeddp(p, 0, 0);
		return;
	}
	goto drop;
gwdb:
	g = (struct gwdb *)(u+1);
	if (ip->ip_src != conf.ipdebug || g->magic != gwdbMagic)
		goto drop;
	switch (g->op) {
	case gwdbRead:
		bcopy(g->address, g->data, g->count);
		break;

	case gwdbWrite:
		bcopy(g->data, g->address, g->count);
		break;

	default:
		g->op = 0;	/* error reply code */
	}
	/*
	 * Assume the packet length does not change.
	 */
	u->checksum = 0;
flip:
	ip->ip_dst = ip->ip_src;
	ip->ip_src = conf.ipaddr;
	ip->ip_sum = 0;
	ip->ip_sum = in_cksum((caddr_t)ip, hlen);
	routeip(p, 0, 0);
	return;
icmp:
	ic = (struct icmp *)(p->p_off + hlen);
	if (ic->icmp_type == ICMP_ECHO) {
		ic->icmp_type = ICMP_ECHOREPLY;
		ic->icmp_cksum = 0;
		ic->icmp_cksum = in_cksum((caddr_t)ic, ip->ip_len - hlen);
		goto flip;
	}
	goto drop;
}


/*
 * IP are us.  Returns true if this IP address is 'one of us':
 * the administrator or one of the configured gateways.
 */
ipRus(ia)
	iaddr_t ia;
{
	register struct aroute *ar;
	register f;

	f = (arouteKbox|arouteAA);
	if (ia == conf.ipadmin || ia == conf.ipdebug)
		return (1);
	for (ar = &aroute[0] ; ar < &aroute[NAROUTE] ; ar++) {
		if (ar->net == 0)
			continue;
		if ((ar->flags & f) != f)
			continue;
		if (ar->node == ia)
			return (1);
	}
	return (0);
}


/*
 * Process a DDP packet directed to the gateway's own address.
 * Called from routeddp with the ddp header already unpacked
 * in global struct 'ddp'.
 */
ddp4me(p)
	register struct pbuf *p;
{
	register struct ATP *a;
	register struct IPGP *ig;

	if (ddp.type == ddpNBP && ddp.dstSkt == nbpNIS) {
		nbpinput(p);
		return;
	}
	if (ddp.type != ddpATP || ddp.dstSkt != ddpIPSkt)
		goto drop;
	a = (struct ATP *)(p->p_off + lapSize + ddpSize);
	if ((ddp.length & ddpLengthMask) < (ddpSize + sizeof *a + 4))
		goto drop;
	if ((a->control & (~atpFlagMask)) != atpReqCode)
		goto drop;
	ig = (struct IPGP *)(a + 1);
	switch (ig->op) {
	case ipgpAssign:
	case ipgpServer:
		ipgassign(ig, ig->op);
		break;

	/* case ipgpName:
		ipgname(ig);
		goto drop;*/

	default:
		strcpy(ig->string, "bad op");
		ig->op = -1;
		break;
	}
	ddp.length = (ddpSize + sizeof *a + ipgpMinSize 
	    + strlen(ig->string) + 1);
	p->p_len = ddp.length + lapSize;
	ddp.checksum = 0;
	ddp.dstNet = ddp.srcNet;
	ddp.dstNode = ddp.srcNode;
	ddp.dstSkt = ddp.srcSkt;
	ddp.srcNet = ifab.if_dnet;
	ddp.srcNode = ifab.if_dnode;
	ddp.srcSkt = ddpIPSkt;
	a->control = atpRspCode + atpEOM;
	a->bitmap = 0;
	bcopy((caddr_t)&ddp, p->p_off+lapSize, ddpSize);
	routeddp(p, 0, 0);
	return;
drop:
	K_PFREE(p);
}


/*
 * IPGATEWAY request to assign address.
 */
ipgassign(ig, op)
	register struct IPGP *ig;
{
	register struct ipdad *d;
	struct ipdad *od;
	int dmax, otimer;
	register i;

	if (op == ipgpServer)
		goto server;	/* skip most of this */
	dmax = conf.ipdynamic;
	otimer = 0;
	for (d = &ipdad[0], i = 0 ; i < dmax ; d++,i++ ) {
		if (d->timer == 0) {
			otimer = ipdadTimerMax;
			od = d;
		} else if (d->timer > otimer) {
			otimer = d->timer;
			od = d;
		}
		if (d->net == ddp.srcNet && d->node == ddp.srcNode)
			goto assign;
	}
	if (otimer <= ipdadTimerMin) {
		ig->op = -1;
		strcpy(ig->string, "no free address");
		return;
	}
	d = od;
assign:
	d->net = ddp.srcNet;
	d->node = ddp.srcNode;
	d->timer = 1;
	ig->ipaddress = (d - &ipdad[0]) + conf.ipaddr + conf.ipstatic + 1;
	if (i == dmax)	/* if this address may have been reassigned */
		arpdelete(ig->ipaddress);
server:
	ig->string[0] = 0;
	ig->ipname = conf.ipname;
	ig->ipbroad = conf.ipbroad;
	ig->ipfile = conf.ipfile;
	bcopy((caddr_t)conf.ipother, (caddr_t)ig->ipother, sizeof ig->ipother);
}


short	ipd_index, ipd_timer, ipd_first;	/* 'static' */

/*
 * Timeout unused ipdad table entries.  Called once per second, sends
 * one 'tickle' per minute to each 'active' IP address client.
 * Sends at most one NBP per second.
 */
ipdadtimer()
{
	Entity ent;
	register struct ipdad *d;

	if (conf.ipdynamic == 0)
		return;
	/*
	 * This one-time code runs between seconds 25 to 30 after the
	 * gateway is booted.  It does an nbplookup request for
	 * all IPADDRESSes so we can initially fill in our table.
	 */
	if (ipd_first < 30) {
		ipd_first++;
		if (ipd_first < 25)	/* wait for RTMPs to establish */
			return;
		strcpy(ent.obj, "=");
		strcpy(ent.type, "IPADDRESS");
		strcpy(ent.zone, "*");
		nbplookup(&ent, 0, 0);
		return;
	}
	/*
	 * This code runs every second after startup.  Step thru
	 * the table pinging active entries.
	 */
	ipd_timer++;
	d = &ipdad[ipd_index];
	for ( ; ; d++, ipd_index++) {
		if (ipd_index >= conf.ipdynamic) {
			if (ipd_timer < 60)
				return;
			ipd_timer = ipd_index = 0;
			d = &ipdad[0];
		}
		/* dont ping empty or really old entries */
		if (d->timer <= 0 ||  d->timer == ipdadTimerMax)
			continue;
		if (++d->timer > ipdadTimerMin)
			continue;
		iptoa(conf.ipaddr + conf.ipstatic + 1 + ipd_index, ent.obj);
		strcpy(ent.type, "IPADDRESS");
		strcpy(ent.zone, "*");
		nbplookup(&ent, d->net, d->node);
		ipd_index++;
		return;
	}
}


/*
 * Reply to ipdadtimer NBP 'tickle' received; reset timer.
 */
ipdadreply(ifp, iaddr, daddr)
	struct ifnet *ifp;
	iaddr_t iaddr;
	register AddrBlock *daddr;
{
	register struct ipdad *d;
	register i;

	i = iaddr - (conf.ipaddr + conf.ipstatic + 1);
	if (i < 0 || i >= conf.ipdynamic)
		return;
	d = &ipdad[i];
	d->timer = 1;
	d->net = daddr->net;
	d->node = daddr->node;
}


/*
 * Convert ASCII dot notation string 's' to long ip address.
 * 'n' is optional byte count.
 * Returns address, or 0 if error.
 */
atoip(s, n)
	register char *s;
{
	register addr, dots, num;

	dots = addr = num = 0;
	for ( ; ; s++, n--) {
		if (n == 0 || *s == '.' || *s == 0) {
			dots++;
			if (num > 255)
				return (0);
			addr = (addr << 8) + num;
			num = 0;
			if (n && *s == '.')
				continue;
			if (dots != 4)
				return (0);
			else
				return (addr);
		}
		if (*s >= '0' && *s <= '9')
			num = (num*10) + (*s - '0');
		else
			return (0);
	}
}


/*
 * Convert IP address to ASCII.  Converts 'n' to string at 's'.
 * Returns number of bytes in s (minus null).
 */
iptoa(n, s)
	register n;
	register char *s;
{
	char buf[20];
	register char *cp;
	int i;
	register b;

	cp = buf;
	for (i = 0 ; i < 4 ; i++) {
		b = (n & 0xFF);
		n >>= 8;
		if (b) {
			while (b) {
				*cp++ = (b % 10) + '0';
				b /= 10;
			}
		} else {
			*cp++ = '0';
		}
		*cp++ = '.';
	}
	cp -= 2;	/* skip last dot */
	i = 0;
	while (cp >= buf) {
		*s++ = *cp--;
		i++;
	}
	*s = 0;
	return (i);
}


/*
 * Setup the standard IP header fields for a destination
 */
setiphdr(p, dst)
	struct pbuf *p; 
	iaddr_t dst;
{
	register struct ip *ip = (struct ip *)p->p_off;

	ip->ip_v = IPVERSION;
	ip->ip_hl = sizeof(*ip) >> 2;
	ip->ip_tos = 0;
	ip->ip_len = p->p_len;
	ip->ip_id = ipid++;
	ip->ip_off = 0;
	ip->ip_ttl = IPFRAGTTL;
	ip->ip_p = IPPROTO_UDP;
	ip->ip_src = conf.ipaddr;
	ip->ip_dst = dst;
	ip->ip_sum = 0;
	ip->ip_sum = in_cksum((caddr_t)ip, sizeof(*ip));
}
