/*
 *  AppleTalk / Ethernet Gateway.
 *
 *  (c) 1986, Stanford Univ. SUMEX project.
 *  May be used but not sold without permission.
 *
 *  (c) 1986, Kinetics, Inc.
 *  May be used but not sold without permission.
 */

char rcsident[] =
"$Header$";

#include "gw.h"
#include "gwctl.h"
#include "fp/pbuf.h"
#include "ab.h"
#include "ether.h"
#include "inet.h"
#include "fp/cmdmacro.h"

#include "glob.h"
#include "conf.h"



/*
 * Main event loop.
 */
main()
{
	register struct pbuf *p;
	register struct ifnet *ifp;
	short pri,nodenum;
	struct fp_mem mem;
	extern char begin;	/* first address in RAM */
	extern char edata;	/* last address in initialized data */
	extern char end;	/* last address in this downloaded program */

	K_PROMRAM(&pvars); /* get addresses of relevant prom variables */
	data_init(); /* initialize data normally done at compile time */
	pq    = pvars.fpr_bufs->fpb_pq; /* shorthand for queue headers */
	sendq = pvars.fpr_bufs->fpb_sendq;
	bcopy(pvars.fpr_state->fps_unused, (caddr_t)&conf, sizeof conf);
	bcopy(conf.etheraddr, etheraddr, 6);
	asminit();		/* set up ethernet interrupt vector */

	/* initialize buffer pool */
	K_INIPROM();
	mem.fpm_count = 0;
	K_GETMEM(&mem);		/* gets address of the top of available mem */
	topram = (int)mem.fpm_memp;
	/* use all the rest of available ram for buffers */
	bufs = (struct pbuf *)((int)(&end) + 2);
	mem.fpm_count = topram - ((int)(bufs)) - 2;
	mem.fpm_memp = (char *)bufs;
	K_CLRMEM(&mem);		/* initialize entire area to all zeroes */
	/* set free list and queue headers to empty */
	*(pvars.fpr_bufs->fpb_pfree) = 0;
	pq->pq_head = 0;
	pq->pq_tail = 0;
	pq->pq_len = 0;
	sendq->pq_head = 0;
	sendq->pq_tail = 0;
	sendq->pq_len = 0;
	K_BUFINIT(&mem);	/* divide the area into buffers */

	/* initialize interface structures */
	ifnet = &ifie; ifie.if_next = &ifab; ifab.if_next = 0;
#ifdef notdef
	K_ATINIT(&nodenum); /* initialize AppleTalk network interface */
	atnode = nodenum;
#else
	atnode = nodenum = pvars.fpr_state->fps_node;
#endif
	ie_init();	/* initialize Ethernet interface... */
	if (conf.ipaddr == 0 || etheraddr[2] != 0x89)
		panic("bad conf");

	ifie.if_addr = conf.ipaddr; /* initialize internet data structures */
	ipnet = ifie.if_addrnet = ipnetpart(ifie.if_addr);
	ifie.if_dnode = (conf.ipaddr & 0xFF);
	ifab.if_dnode = ifab.if_haddr[0] = atnode;

	checksum(&begin, &edata); /* set up RAM memory checksum protection */
	if (conf.ready == confReady)
		confready(0);	/* finish configuration */
	K_WAIT(20);
	sendf("KFPS(IP) node #%d lives!! - %d buffers", atnode,
	      mem.fpm_count/sizeof (struct pbuf));

	K_SPL0(&pri); /* allow interrupts */

	for (;;) {
		if (pq->pq_head == 0) { /* no packets on main queue */
			back();		/* perform background processing */
			continue;
		}
		K_PDEQ(SPLIMP,pq,p);	/* remove buffer from queue */
		if (p->p_len < 0) {	/* if error packet */
			if (p->p_len != -1)
				goto pan;
			K_PFREE(p); /* put the buffer back on the free list */
			continue;
		}
		switch (p->p_type) {	/* switch on packet type */
		case PT_ABUS:
			p_if(p) = &ifab; /* should be in prom code? */
 			abreceive(p);	/* receive AppleTalk packet */
			break;
		case PT_ENET:
			iereceive(p);	/* receive ethernet packet */
			break;
		default:
pan:
#ifdef	STATS
			K_SPLIMP(&pri);	/* prevent interruptions */
			p_print("main",p);
			dmpbf();
			ieprintstats();
			abprintstats();
#endif	STATS
			panic("main: bad pkt");
		}
	}
}


/*
 * Complete configuration process.  Called when configuration 'ready'
 * via preset struct conf or aaconf packet received.
 */
confready(aa)
	register struct aaconf *aa;
{
	register i, ia;

	if (aa) {	/* configure packet received */
		bcopy(aa->stuff, (caddr_t)(&conf) + confPrefix,
			sizeof conf - confPrefix);
		conf.ready = confReady;
	}
	if (conf.ipdynamic > NIPDAD)
		conf.ipdynamic = NIPDAD;
	ifie.if_dnet = conf.atnete;
	ifab.if_dnet = conf.atneta;
	aroute[0].net = conf.atneta;	/* hops=0, node=0, flags=0 */
	aroute[1].net = conf.atnete;
	aroute[1].node = (conf.ipaddr & ~0xFF);
	aroute[1].flags = arouteNet;
}


short	confdelay;	/* sigh, should be 'static', but checksum complains */

/*
 * Configuration clock routine;  get config from appletalk admin.
 */
conftimer()
{
	if (confdelay && ++confdelay < 10)
		return;
	confdelay = 1;
	confrequest(aaCONF);
}


/*
 * Request configuration (or net table) from appletalk administrator.
 */
confrequest(op)
{
	register struct aaconf *m;
	register struct udp *u;
	register struct pbuf *p;

	K_PGET(PT_DATA, p);
	if (p == 0)
		return;
	p->p_len = sizeof (struct ip) + sizeof *u + aaconfMinSize;
	setiphdr(p, conf.ipadmin);
	u = (struct udp *)(p->p_off + sizeof (struct ip));
	u->src = aaPort;
	u->dst = aaPort;
	u->length = p->p_len - sizeof (struct ip);
	u->checksum = 0;
	m = (struct aaconf *)(u + 1);
	m->magic = aaMagic;
	m->type = op;
	m->ipaddr = conf.ipaddr;
	m->count = 0;
	routeip(p, 0, 0);
}


int	backclock;	/* 'static' */

/*
 * Backround processing.  Call once per second timeout routines.
 * Call 'backround' routines.
 */
back()
{
	register i;

	nbpback();	/* NBP backround (sends 1 LkUp per loop) */
#ifdef 	KINETICS
	msclock++;
	if ((i = msclock - backclock) < 0)
		i = -i;
	if (i > 12000) {		/* ~one second */
#else
	if ((i = msclock - backclock) < 0)
		i = -i;
	if (i > 1000) {		/* one second */
#endif
		backclock = msclock;
		arptimer();	/* timeout arp cache */
		if (conf.ready != confReady) {
			conftimer();
			return;
		}
		rtmptimer();	/* routing table maint protocol timer */
		arttimer();	/* arouteTuple timer */
		ipdadtimer();	/* IP dynamic address assignment timer */
	}
}


/*
 * The functions of the "abreceive" and "ilreceive" routines below might 
 * normally belong in the input interrupt sections of their respective
 * drivers.  But since they do some protocol translation as well, we've
 * stuck them here.  (Another consideration is that the LAP input interrupt
 * code is very time critical).
 */

/*
 * Receive next packet from AppleTalk.
 * Forward the packet onto the ethernet leg, possibly encapsulating
 * in a UDP packet first.
 */
abreceive(p)
	register struct pbuf *p;
{
	register wasbroad;

	bcopy(p->p_off, (caddr_t)&lap, lapSize);
	wasbroad = (lap.dst == 0xFF);
	p->p_off += lapSize;
	p->p_len -= lapSize;
	wasddp = 0;	/* assume not a ddp */
	/*
	 * Below we copy the DDP header into a global structure to
	 * get word alignment.  Since lapSize and ddpSize are both odd,
	 * after handling the headers the packet is again on an even
	 * boundary (odd+odd=even).
	 */
	switch (lap.type) {
	case lapDDPS:
		if ((p->p_len -= ddpSSize) < 0)
			goto drop;
		bcopy(p->p_off, (caddr_t)&ddps, ddpSSize);
		p->p_off += ddpSSize;
		ddp.dstSkt = ddps.dstSkt;
		ddp.type = ddps.type;
		break;

	case lapDDP:
		if ((p->p_len -= ddpSize) < 0)
			goto drop;
		bcopy(p->p_off, (caddr_t)&ddp, ddpSize);
		p->p_off += ddpSize;
		wasddp = 1;
		break;

	case 'K':		/* Kinetics protocol type */
		K_KLAP(p);
		K_PFREE(p);
		return;

	default:
		goto drop;
	}

	if (conf.ready != confReady)
		goto drop;
	/*
	 * here we switch out immediately to short-DDP-only
	 * handlers, to avoid having to convert them to long DDPs.
	 */
	switch (ddp.dstSkt) {
	case 0:
		goto drop;

	case rtmpSkt:
		if (lap.src == ifab.if_dnode)
			goto drop;	/* from ourself */
		rtmpinput(p);
		return;

	case zipSkt:
		if (lap.src == ifab.if_dnode)
			goto drop;	/* from ourself */
		zipinput(p);
		return;
	}
	if (ddp.type == ddpIP) {
		routeip(p, &ifab, wasbroad);
		return;
	}
	/* not magic type, just route as a ddp */
	if (wasddp == 0)
		ddps2ddp(p);
route:
	p->p_off -= lapSize + ddpSize;
	p->p_len += lapSize + ddpSize;
	routeddp(p, &ifab, wasbroad);
	return;

drop:
	/* put the buffer back on the free list */
	K_PFREE(p);
	stats.dropabin++;
	return;
}


/*
 * Convert packet from short DDP form to long DDP form.
 */
ddps2ddp(p)
	register struct pbuf *p;
{
	if (wasddp)
		return;
	ddp.length = ddps.length - ddpSSize + ddpSize;
	ddp.checksum = 0;
	ddp.dstNet = ddp.srcNet = ifab.if_dnet;
	ddp.srcNode = lap.src;
	ddp.dstNode = lap.dst;
	ddp.srcSkt = ddps.srcSkt;
	ddp.dstSkt = ddps.dstSkt;
	ddp.type = ddps.type;
	bcopy((caddr_t)&ddp, p->p_off - ddpSize, ddpSize);
	wasddp = 1;
}


/*
 * Receive next packet from intel ethernet.
 */
iereceive(p)
register struct pbuf *p;
{
	struct ether_header *ehp;
	register struct ip *ip;
	register struct udp *up;
	register int l;
	int wasbroad;

#ifdef	EDATA
	K_WAIT(20);
	p_print("ierecv", p);
#endif	EDATA
	ehp = (struct ether_header *)p->p_off;
	wasbroad = (ehp->ether_dhost.ether_addr_octet[0] & 0x1);
	p->p_off += sizeof *ehp;
	p->p_len -= sizeof *ehp;
	switch (ntohs(ehp->ether_type)) {
	    case ETHERTYPE_ARPTYPE:
		arpinput(p);	/* pass it to ARP */
		return;
	    case ETHERTYPE_IPTYPE:
		break;
	    default:
		goto drop;		/* can't handle it */
	}

	/*
	 *  If the proto type is IP/UDP and the port
	 *  number is in the magic range, remove the
	 *  encapsulation from the DDP datagram.
	 */
	ip = (struct ip *)p->p_off;
	/* Checksum the IP header ? */
	if (p->p_len < sizeof (*ip))
		goto drop;
	if (conf.ready != confReady) {
		/* if conf not ready, only process control packets */
		if (ip->ip_dst != conf.ipaddr)
			goto drop;
		ip4me(p);
		return;
	}
	if (ip->ip_p != IPPROTO_UDP || ip->ip_dst != conf.ipaddr)
		goto toip;
	l = (ip->ip_hl << 2);
	up = (struct udp *)(p->p_off + l);
	if (p->p_len < (l + sizeof(*up)))
		goto drop;
	if ((up->dst >= ddpNWKSUnix + 128 && up->dst < ddpNWKSUnix + 256)
	    || (up->dst >= ddpWKSUnix && up->dst < ddpWKSUnix + 128)) {
		/* decapsulate */
		p->p_off += (l + sizeof(*up));
		p->p_len -= (l + sizeof(*up));
		/* route as ddp */
		if (p->p_len < ddpSize + lapSize)
			goto drop;
		bcopy(p->p_off + lapSize, (caddr_t)&ddp, ddpSize);
		l = (ddp.length & ddpLengthMask);
		if (l < p->p_len - lapSize)
			p->p_len = l + lapSize; /* zap padding */
		wasddp = 1;
		routeddp(p, &ifie, wasbroad);
		return;
	}
toip:
	routeip(p, &ifie, wasbroad);
	return;
drop:
/*	sendf("Got bad IP packet"); */
	stats.dropiein++;
	/* put the buffer back on the free list */
	K_PFREE(p);
	return;
}

/*
 * These match routines return 0 if no match and 1 (true) if a match occurs.
 * They are called from arpinput/nbpinput to determine if an IP
 * address is in our range.
 *
 * A 'true' return value causes the arpinput code to return a 'fake ARP'
 * to the requester:  i.e. the requester will think that the real destination
 * host responded to the ARP, whereas it was really this gateway 'faking'
 * a response.
 */

/*
 * Match an AppleTalk ARP.  If the target is not in my own
 * range, return true (match).  This has the effect of forwarding all
 * traffic for 'unknown' nets, to the backbone ethernet.
 */
abmatch(target)
	iaddr_t target;
{
	register i;

	if (target == conf.ipaddr)
		return (1);
	if (ipnetpart(target) == 0x7F000000)
		return (0);
	i = target - conf.ipaddr;
	if (i < 0)
		return (1);
	if (i >= conf.ipstatic + conf.ipdynamic + 1)
		return (1);
	return (0);
}

/*
 * Match an ethernet ARP.
 */
iematch(target)
	iaddr_t target;
{
	register i;

	i = target - conf.ipaddr;
	if (i >= 0 && i <= conf.ipstatic + conf.ipdynamic)
		return (1);
	return (0);
}


/*
 * Route an IP packet.  This code currently assumes only one AppleTalk and
 * ether interface per gateway;  instead it should scan a list of interfaces.
 * If a routing table existed within the gateway, it would determine
 * which interface pointer (ifp) and destination address (dst below)
 * are passed to ifp->if_output.
 */
routeip(p, srci, br)
	register struct pbuf *p;
	struct ifnet *srci;
{
	register struct ifnet *ifp = &ifie;	/* default output to ether */
	register struct ip *ip = (struct ip *)p->p_off;
	int dst = ip->ip_dst;
	register iaddr_t dstnet = ipnetpart(dst);

	/* omit any bytes past the ip data */
	if (p->p_len > ip->ip_len) {
		p->p_len -= (p->p_len - ip->ip_len);
	}
	if (dst == conf.ipaddr) {
		ip4me(p);
		return;
	    }

	if (iematch(dst))
		ifp = &ifab;
#ifdef PURDUE
	/* hack to forward ethernet IP broadcasts to the appletalk net	*/
	/* if ipbroadcast && from ethernet && broadcast flag <> 0	*/
	/* uses currently unused ipsmask field in the config structure	*/
	/* from the Unix host to turn it on and off			*/
	else if ( ((dst & 0xFF) == 0x00) && (srci == &ifie)
		                         && (conf.ipsmask != 0))
	        ifp = &ifab;
#endif	
	else if (dstnet != ifie.if_addrnet
	    || (ipbroadcast(dst) && dst != conf.ipbroad))
		dst = conf.iproutedef;	/* needs to be forwarded */
	if (ifp != srci) /* do not reflect packets to same net */
		return((*ifp->if_output)(ifp, p, AF_IP, &dst));
drop:
	K_PFREE(p);
	stats.droprouteip++;
	return;
}


/* IP directed broadcast types, used by 'routeddp' */
iaddr_t ipbroadtypes[] = { 0, 0xFF, 0xFFFF, 0xFFFFFF };


/*
 * Route a DDP packet.  Assumes ddp has already been unpacked into
 * global struct ddp.  If 'si' (source interface) is 0, packet
 * originates inside the gateway;  otherwise this is a forward.
 * 'br' is true if the packet was received as a broadcast.
 */
routeddp(p, si, br)
	register struct pbuf *p;
	struct ifnet *si;
{
	register struct udp *u;
	register struct aroute *ar;
	iaddr_t idst;
	register i, port;
	u_char dst,xx;

	if (ddp.dstNet == 0 || ddp.dstNode == 0)
		goto drop;
	if (p->p_len < (ddpSize + lapSize))
		goto drop;
	for (ar = &aroute[0] ; ar < &aroute[NAROUTE] ; ar++) {
		if (ar->net == 0)	/* empty slot */
			continue;
		if (ar->net == ddp.dstNet)
			goto found;
	}
	goto drop;	/* didnt find it */
found:
	/*
	 * if this packet originates externally and is addressed
	 * to us (or broadcast), hand it to 'ddp4me'.
	 */
	if (si == 0)	/* if can't be for us */
		goto fwd2;
	if (ar->hops)	/* if not one of our interfaces */
		goto fwd;
	if ((br && ddp.dstNode == 0xFF)
	    || (ddp.dstNet == ifab.if_dnet && ddp.dstNode == ifab.if_dnode)
	    || (ddp.dstNet == ifie.if_dnet && ddp.dstNode == ifie.if_dnode)) {
		ddp4me(p);
		return;
	}
fwd:
	if (((ddp.length >> ddpHopShift) & 0xF) == 0xF)
		goto drop;
	p->p_off[lapSize] += 4;	/* increment hop count in pbuf */
fwd2:
	if (br)
		goto drop;	/* dont forward broadcasts */
	if (arouteIP(ar) == 0) {	/* for our atalk segment */
		/* if (si == &ifab) goto drop; (but not with Hayes) */
		dst = (ar->node ? ar->node : ddp.dstNode);
		(*ifab.if_output)(&ifab, p, AF_DDP, &dst);
		return;
	}
	/*
	 * Else destination is IP address, encapsulate.
	 * Choose the appropriate IP host or directed broadcast address.
	 */
	port = ddp2ipskt(ddp.dstSkt);
	if (ar->flags & arouteKbox) {
		idst = ar->node;
	} else if (ar->flags & arouteNet) {
		if (ddp.dstNode != 0xFF) {
			idst = ar->node + ddp.dstNode;
		} else {
			if (ddp.dstNet == ifie.if_dnet) {
				idst = conf.ipbroad;
			} else {
				idst = ar->node 
				    + ipbroadtypes[ar->flags&arouteBMask];
			}
		}
	} else if (ar->flags & arouteHost) {
		if (ddp.dstNode != 0xFF) {
			idst = (ar->node & ~0xFF) + ddp.dstNode;
		} else {
			idst = ar->node;
			port = rebPort;
		}
	} else
		goto drop;
	lap.dst = 0xFA;
	lap.src = 0xCE;
	lap.type = lapDDP;
	bcopy((caddr_t)&lap, p->p_off, lapSize);
	p->p_off -= sizeof (struct ip) + sizeof *u;
	p->p_len += sizeof (struct ip) + sizeof *u;
	setiphdr(p, idst);
	u = (struct udp *)(p->p_off + sizeof (struct ip));
	u->src = ddp2ipskt(ddp.srcSkt);
	u->dst = port;
	u->length = p->p_len - sizeof (struct ip);
	u->checksum = 0;
	routeip(p, 0, 0);
	return;
drop:
	/*
	K_WAIT(20);
	sendf("routeddp: dropping packet");
	*/
	stats.droprouteddp++;
	/* put the buffer back on the free list */
	K_PFREE(p);
}


/*
 * AppleTalk output routine.
 * Encapsulate a packet of type af for the local net.
 * 
 * Look at a normal net output routine (such as ethernet output, iloutput),
 * for the more general case of an output routine which can handle 
 * multiple units (using the ifp->if_unit field) with interrupt driven
 * output (and output queues).
 *
 * Also, normally an output routine prepends only the link 
 * level header (e.g. LAP).  However this routine is a bit unusual:
 *
 *   Since the LAP and DDP header sizes are both odd, DDP packets are
 *   usually passed about with both headers on the front, so that the
 *   data will always be on an even boundary.
 *
 *   Since IPs are inside DDPs, we prefix a LAP/DDP header onto them.
 */
aboutput(ifp, p, af, dst)
	register struct ifnet *ifp;
	register struct pbuf *p;
	caddr_t dst;
{
	u_char adst;
	iaddr_t idst;
	AddrBlock tdst;
	struct LAP l;
	struct DDP d;
	WDS wds[2];		/* write data structure for abuswrite */

	l.src = ifp->if_dnode;	/* (also done as part of atwrite) */
	switch (af) {

	case AF_DDP:	/* DDP: already has LAP header */
		l.dst = *dst;
		l.type = lapDDP;
		bcopy((caddr_t)&l, p->p_off, lapSize);
		break;
		
	case AF_SDDP:	/* ShortDDP: already has LAP header */
		l.dst = *dst;
		l.type = lapDDPS;
		bcopy((caddr_t)&l, p->p_off, lapSize);
		break;

	case AF_IP:
		idst = *(iaddr_t *)dst;
#ifdef PURDUE
/* I don't understand why the code was written this way, but sending an	*/
/* appletalk broadcast to arpresolve() didn't work, so I did it this	*/
/* to fix it.								*/
		if ( (idst & 0xFF) == 0) {
		    tdst.node = 255;
		    tdst.net = ifp->if_dnet;
		    goto lapddp;
		}
#endif		
		if (!arpresolve(ifp, p, &idst, &tdst)) {
			return (0);	/* if not yet resolved */
		}
	lapddp:
		l.type = lapDDP;
		d.length = p->p_len + ddpSize;
		d.checksum = 0;
		d.srcNet = ifp->if_dnet;
		d.dstNet = tdst.net;
		d.srcNode = ifp->if_dnode;
		d.dstNode = tdst.node;
		d.srcSkt = d.dstSkt = ddpIPSkt;
		d.type = ddpIP;
		if (tdst.net == ifp->if_dnet)
			l.dst = tdst.node;
		else if ((l.dst = getaroute(tdst.net)) == 0)
			goto drop;
		p->p_off -= (lapSize + ddpSize);
		p->p_len += (lapSize + ddpSize);
		bcopy((caddr_t)&l, p->p_off, lapSize);
		bcopy((caddr_t)&d, p->p_off+lapSize, ddpSize);
		break;

	default:
		panic("ab%d: can't handle af%d\n", ifp->if_unit, af);
	}

	wds[1].size = 0;	/* terminate list */
	wds[0].size = p->p_len;
	wds[0].ptr = p->p_off;
	K_ATWRITE(&wds[0]);	/* appletalk write */
#ifdef PURDUE
	/* this is causing broadcast echoes back to ethernet */
	if ( (l.dst == 0xFF) && (af != AF_IP) ) {
#else
	if (l.dst == 0xFF) {	/* if was broadcast, give myself a copy */
#endif	    
		p->p_type = PT_ABUS;
		/* oops, atwrite smashes dst during xmit(?) */
		p->p_off[1] = ifp->if_dnode;
		K_PENQ(SPLIMP, pq, p);
		return;
	}
	
drop:
	/* put the buffer back on the free list */
	K_PFREE(p);
	return;
}

/*
 * Panic - fatal error
 */
/*VARARGS*/
panic(s,a,b,c)
{
	short pri;

	K_SPLIMP(&pri);
	sendf("PANIC!");
	sendf(s,a,b,c);
#ifdef	STATS
	ieprintstats();
	abprintstats();
#endif	STATS
	K_ERR();
	K_INIPROM();
	K_RESET();
}

#ifdef	STATS
/*
 * Send AppleTalk statistics
 */
abprintstats()
{
	register struct fp_abstats *ss;

	ss = pvars.fpr_abstats;
	K_WAIT(20);
	sendf("appletalk stats: ints %d, in %d, out %d, crc %d, ovr %d",
		ss->fpa_interrupts, ss->fpa_ipackets, ss->fpa_opackets,
		ss->fpa_crc, ss->fpa_ovr);
	K_WAIT(20);
	sendf("iund %d, bad %d, coll %d", ss->fpa_iund, ss->fpa_bad,
		ss->fpa_coll);
	K_WAIT(20);
	sendf("defer %d, idleto %d, nodata %d, ound %d, badddp %d, spur %d",
		ss->fpa_defer, ss->fpa_idleto, ss->fpa_nodata, ss->fpa_ound,
		ss->fpa_badddp, ss->fpa_spur);
}

dmpbf()
{
	struct pbuf *qq;
	int flag;
	register struct fp_bufinfo *ss;

	ss = pvars.fpr_bufs;
	sendf("dump pbufs - pbndrops = %d",ss->fpb_pbndrops);
	sendf("pfree: 0x%x", *(ss->fpb_pfree));
	sendf("   pq: head = 0x%x, tail= 0x%x, len = %d", pq->pq_head,
						pq->pq_tail, pq->pq_len);
	sendf("sendq: head = 0x%x, tail= 0x%x, len = %d",sendq->pq_head,
						sendq->pq_tail, sendq->pq_len);
	for (flag = 0, qq = bufs;
			qq < (struct pbuf *)(topram - ss->fpb_bsize + 1);
			qq++, flag++) {
		sendf("%d=0x%x: %d 0x%x",flag,qq,qq->p_type,qq->p_next);
	}
}

p_print(s, p)
char *s;
register struct pbuf *p;
{
	register u_char *cp = p->p_off;
	register ii,jj;
	char ll[256];
	char *lp;
	extern char tohex[];

	sendf("(%s) addr 0x%x p_type %d, p_len %d, p_off 0x%x\n",
		s, p, p->p_type, p->p_len, p->p_off);
	ii = p->p_len > 64 ? 64 : p->p_len;
	lp = ll;
	for (jj = 0 ; jj < ii ; jj++) {
		*lp++ = tohex[(*cp >> 4) & 0xF];
		*lp++ = tohex[*cp++ & 0xF];
		*lp++ = ' ';
	}
	*lp = '\0';
	sendf(ll);
}
#endif	STATS
