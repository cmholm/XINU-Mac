/*
 *  Intel 82586 ethernet chip driver (ie = Intel Ethernet)
 *
 *  (c) 1984, Stanford Univ. SUMEX project.
 *  May be used but not sold without permission.
 *
 *  (c) 1986, Kinetics, Inc.
 *  May be used but not sold without permission.
 *
 */

#include "gw.h"
#include "fp/pbuf.h"
#include "ie.h"
#include "ether.h"
#include "ab.h"
#include "fp/cmdmacro.h"

extern char broadcastaddr[];
extern struct pqueue *sendq;
extern struct pqueue *pq;
extern struct fp_promram pvars;

/*
 * wait for scb command word to clear
 *	After giving a command, the 82586 steals the bus
 *	and proceeds to capture the command end execute
 *	it. When execution is completed, it clears the
 *	command word. In general, we only check that
 *	the command word is clear before we issue a
 *	new command.
 */
#define	WAIT_CMD() { \
	while (scbptr->sc_cmd != 0) \
		pvars.fpr_iestats->fpi_cmdbusy++; \
}

/*
 * if the receiver is in the suspended state, give it the resume
 * command to get it in a state ready to receive Ethernet packets again.
 */
#define RESUME() { \
	if ((SWAB(scbptr->sc_status) & SC_RUS) == RUS_SUSP) { \
		WAIT_CMD(); \
		scbptr->sc_cmd |= SWAB(RUC_RES); \
		K_CA86(); \
	} \
}

/* transmit buffers */
#define	NTBD		1	/* number of transmit buffers */
struct t_bd tbd[NTBD];

/* receiver frame descriptors */
#define	NRFD		8	/* number of receive frame descriptors */
struct fdes rfd[NRFD];		/* receive frame descriptors */
struct fdes *rfdhead;		/* first in the list */
struct fdes *rfdtail;		/* end of the list */

#define	NRBD		8	/* number of receive buffers */
struct   r_bd  rbd[NRBD];	/* receive buffers */
struct   r_bd *rbdhead;		/* first in the list */
struct   r_bd *rbdtail;		/* end of the list */

char	tactive;		/* transmit active */

#ifdef	STATS
/* statistics */
int	iespur;			/* count of spurious interrupts */
int	iefalsexmit;		/* false transmit interrupt */
int	eintr;			/* count of ethernet interrupts */
int	iestuck;		/* count of stuck interrupts */
int	oknotset;		/* count of times ok bit is not set */
int	okbits;			/* bit settings last time ok not set */
int	ieready;		/* count of times receive unit already READY */
int	iefcnt;			/* count of times that no frame descrip left */
int	iebuf;			/* count of times that no rbuf descrip left */
int	norecbufs;		/* count of times there are no receive pbufs */
#endif	STATS

struct  scb *scbptr;
struct   cb *cbptr;
struct fdes *fdptr;

struct cb xmit;			/* transmit command buffer */
struct cb iasetup;		/* Ethernet address setup command buffer */
struct cb ieconfig;		/* Ethernet configuration command buffer */

/*
 * Ether slop factor.  Pbuf MAXDATA is 630, but (sizeof ether (14) +
 * sizeof ip (20) + sizeof udp (8) + max lap (603)) = 645.  This means
 * that max size appletalk packets from the ether were being lost.
 * We adjust the receive point in the pbuf upwards by at least 15 bytes
 * so these will fit.  Actually the real solution would be to increase
 * MAXDATA to 1K or so, but (1) this is currently hardwired into the PROM;
 * (2) KFPS is already tight on buffer memory.
 */
#define	ESLOP	20


/*
 * configure and ethernet address setup of the chip
 */
config()
{
	extern struct ifnet ifie;
	extern u_char etheraddr[];

	scbptr->sc_clist = SWAB(LO16(&ieconfig));
	iecmd();	/* configure */

	/* ethernet address loaded to i85286 */
 	eaddrcopy(etheraddr, &iasetup.cb_param[0]);
	scbptr->sc_clist = SWAB(LO16(&iasetup));
	iecmd();	/* iasetup */

	/* set ether address in interface struct */
 	eaddrcopy(etheraddr, ifie.if_haddr);
}

/*
 * Initialize the i82586 chip
 */
ie_init()
{
	register int ss;
	register struct fdes *rfdp;
	register struct	r_bd *rbdp;
	struct pbuf *pp;
	extern u_short ienet;

	/* initialize scb structure */
	scbptr = SCBADDR;
	/*
	 * normally CPU should never write the status word
	 * but 82586 'ors' bits into it so it should begin life blank
	 */
	/* empty status */
	scbptr->sc_status = SWAB(STAT_IDLE | CUS_IDLE | RUS_IDLE);
	/* no commands */
	scbptr->sc_cmd   = SWAB(ACK_NONE | CUC_NOP | RUC_NOP);
	scbptr->sc_clist = SWAB(LO16(CMDADDR));
	scbptr->sc_rlist = SWAB(LO16(FDESADDR));
	/*
	 * these are fetched, incremented and replaced by 82586
	 *  without releasing the bus. they stick at 0xffff when full
	 */
	scbptr->sc_crcerrs  = SWAB(0);
	scbptr->sc_alnerrs  = SWAB(0);
	scbptr->sc_rscerrs  = SWAB(0);
	scbptr->sc_ovrnerrs = SWAB(0);

	/* initialize the receive frame descriptor block */
	fdptr = FDESADDR;
	fdptr->fd_status = SWAB(0);
	fdptr->fd_stat2  = SWAB(0);
	fdptr->fd_link   = SWAB(CB_NIL);
	fdptr->fd_rbd    = SWAB(FD_NIL);
	for (ss = 0; ss < 6; ss++) {
		fdptr->fd_daddr[ss] = '\0';
		fdptr->fd_saddr[ss] = '\0';
	}
	fdptr->fd_type   = SWAB(0);

	/* hardware reset and establish our scb as the one to use */
	K_SET86(scbptr);

	/* configure and iasetup */
	config();

	/* transmit buffer stuff is set up at compile time, except... */
	xmit.cb_param[0]  = SWAB(LO16(&tbd[0]));
	tbd[0].tpbuf = 0;
	scbptr->sc_clist  = SWAB(LO16(&xmit));

	/*
	 * setup receiver structures
	 */
	for (rfdp = &rfd[0] ; rfdp < &rfd[NRFD] ; rfdp++ ) {
		rfdp->fd_link = SWAB(LO16(rfdp+1));
		rfdp->fd_rbd  = SWAB(FD_NIL);
		rfdp->fd_stat2 = SWAB(FD_S);
	}
	rfdp--;
	rfdtail = rfdp;
	rfdp->fd_stat2 |= SWAB(FD_EL);
	rfdp->fd_link = SWAB(LO16(&rfd[0]));
	rfdhead = rfdp = &rfd[0];
	rfdp->fd_rbd = SWAB(LO16(&rbd[0]));
	for (rbdp = &rbd[0]; rbdp < &rbd[NRBD]; rbdp++) {
		/* get a buffer from the free list */
		/* should already be at highest priority */
		K_PGET(PT_ERBF,pp);
		rbdp->rbd.bd_next  = SWAB(LO16(rbdp + 1));
		rbdp->rpbuf = pp;
		if (pp) {
			/* make the pbuf into an Ethernet receive buffer */
			rbdp->rbd.bd_buf   = SWAB(LO16(&pp->p_data[-ESLOP]));
			rbdp->rbd.bd_bufhi = SWAB(HI16(&pp->p_data[-ESLOP]));
			rbdp->rbd.bd_size  = SWAB(MAXDATA+ESLOP);
		}
	}
	rbdp--;
	rbdtail = rbdp;
	rbdp->rbd.bd_next = SWAB(LO16(&rbd[0]));
	rbdp->rbd.bd_size |= SWAB(BD_EL);
	rbdhead = &rbd[0];
	scbptr->sc_rlist = SWAB(LO16(&rfd[0]));
	scbptr->sc_cmd = SWAB(ACK_CX | ACK_CNA | ACK_FR | ACK_RNR);
	K_CA86();

	WAIT_CMD();	/* wait until chip is completely ready */
	/* send 1st receive cmd */
	ierstart();
	scbptr->sc_cmd = SWAB(RUC_START);
	K_CA86();
}

/*
 * Handle Ethernet interrupt.
 */
ieintr()
{
	short scbstatus;
	short scbcmd;
	extern struct ifnet ifie;


	scbstatus = SWAB(scbptr->sc_status);
	scbcmd = ACK_NONE | CUC_NOP | RUC_NOP;

	/* resume the receive unit if necessary */
	RESUME();

#ifdef	STATS
	eintr++;
	if ((scbstatus & (STAT_FR | STAT_CX)) == 0) {
		/* spurious interrupt */
		iespur++;
	}
#endif	STATS
	if ((scbstatus & STAT_RNR) && (rfdhead->fd_status & SWAB(FD_B))) {
		register struct r_bd *rbdp;
		/*
		 * Receiver not ready, yet still busy on 1st frame!
		 * This is a bogus packet of 'infinite' length
		 * and all ones.  Restart the RU.
		 *
		 * 4/29/86 - experiments indicate that this never
		 * happened in numerous startings of the gateway
		 * and over 250,000 packets. leftover from the original
		 * Croft Seagate code, and still included anyway -tim
		 */
		for (rbdp = OTOA(struct r_bd *, rfdhead->fd_rbd);
		     rbdp->rbd.bd_count & SWAB(BD_F);
		     rbdp = OTOA(struct r_bd *, rbdp->rbd.bd_next)) {
			rbdp->rbd.bd_count = SWAB(0);
		}
#ifdef	STATS
		iestuck++;
#endif	STATS
		ierstart();
		scbcmd = ACK_RNR | RUC_START;
	}
	if (scbstatus & STAT_CX) {
		/* interrupt after command (transmit) executed */
		if ((tactive == 0) ||
				((xmit.cb_status & SWAB(CB_COMPLETE)) == 0)) {
#ifdef	STATS
			/* false transmit interrupt */
			iefalsexmit++;
#endif	STATS
		} else {
			if ((xmit.cb_status & SWAB(CB_OK)) == 0) {
				/* increment total number of output errors */
				pvars.fpr_iestats->fpi_oerrors++;
			}
			/* increment total number of output packets */
			pvars.fpr_iestats->fpi_opackets++;

			/* put the transmitted buffer back on the free list */
			K_PFREE(tbd[0].tpbuf);
			tbd[0].tpbuf = 0;
			tactive = 0;
			scbcmd |= scbstatus & (STAT_CX | STAT_CNA);
			if (sendq->pq_head) {
				/* more on queue, restart output */
				iexstart();
				scbcmd |= CUC_START;
			}
		}
	}
	if (scbstatus & STAT_FR) {
		/* get received frames if any */
		ieframein();
		ierstart();	/* start the receiver and ack */
		scbcmd |= (scbstatus & (STAT_FR | STAT_RNR)) | RUC_START;
	}
	K_CLRINT();	/* clear interrupt flag */
	scbptr->sc_cmd = SWAB(scbcmd);
	K_CA86();	/* ack current interrupts, start cmds if any */
	if ((rfdhead->fd_status & SWAB(FD_C)) && (scbstatus & STAT_FR)) {
		/* more frames were received since we last checked */
		/* we might have just acked them... better get them again */
		ieframein();
	}
}

/*
 * receive some Ethernet packet frames
 */
ieframein()
{
	register struct fdes *rfdp;
	register struct r_bd *rbdp;
	struct pbuf *pp;
	struct pbuf *tp;
	u_char *cp;
	int free, count;
	short pri;

	for (rfdp = rfdhead; rfdp->fd_status & SWAB(FD_C);
			rfdp = rfdhead = OTOA(struct fdes *, rfdp->fd_link)) {

		/* resume the receive unit if necessary */
		RESUME();
#ifdef	STATS
		if ((rfdp->fd_status & SWAB(FD_OK)) == 0) {
			oknotset++;
			okbits = SWAB(rfdp->fd_status);
		}
#endif	STATS
		free = MAXDATA+ESLOP;
		/* get a buffer from the free list */
		K_PGET(PT_ERBF,pp);
		/* get the relevant receive buffer descriptor */
		rbdp = OTOA(struct r_bd *, rfdp->fd_rbd);
		/* get the pbuf allocated to that rbuf descriptor */
		tp = rbdp->rpbuf;
		/* attach the new pbuf to the receive buffer descriptor */
		rbdp->rpbuf = pp;
		if (pp) {
			rbdp->rbd.bd_buf   = SWAB(LO16(&pp->p_data[-ESLOP]));
			rbdp->rbd.bd_bufhi = SWAB(HI16(&pp->p_data[-ESLOP]));
			rbdp->rbd.bd_size  = SWAB(MAXDATA+ESLOP);
		}
		/* handle the packet just arrived, already in the pbuf */
		pp = tp;
		if (pp) {
			pp->p_type = PT_ENET;
			/* adjust counts of buffer types */
			pvars.fpr_bufs->fpb_pbntypes[PT_ERBF]--;
			pvars.fpr_bufs->fpb_pbntypes[PT_ENET]++;
			pp->p_off = &pp->p_data[-ESLOP];
			/* get the number of bytes received */
			count = SWAB(rbdp->rbd.bd_count & SWAB(BD_COUNT));
			/* where to copy any additional pbuf's captured */
			cp = &pp->p_data[count-ESLOP];
			free -= count;
#ifdef	STATS
		} else {
			++norecbufs;
#endif	STATS
		}
		/* is that the only receive buffer descriptor this frame? */
		if ((rbdp->rbd.bd_count & SWAB(BD_EOF)) == 0) {
		    /* more receive buffers in this frame, so... */
		    rbdp->rbd.bd_count = SWAB(0);
		    /* foreach additional rbuf descriptor with valid data */
		    for (rbdp = OTOA(struct r_bd *, rbdp->rbd.bd_next) ;
			 rbdp->rbd.bd_count & SWAB(BD_F) ;
			 rbdp = OTOA(struct r_bd *, rbdp->rbd.bd_next))
		    {
			count = SWAB(rbdp->rbd.bd_count & SWAB(BD_COUNT));
			if (count <= free) {
				if (pp)
					bcopy(rbdp->rpbuf->p_data, cp, count);
				cp += count;
				free -= count;
			} else {	/* buffer overflow */
				if (pp) {
					/* put buffer back on the free list */
					K_PFREE(pp);
					pp = 0;
					/* increment number of input errors */
					pvars.fpr_iestats->fpi_ierrors++;
				}
			}
			if (rbdp->rbd.bd_count & SWAB(BD_EOF))
				break;
			rbdp->rbd.bd_count = SWAB(0);
		    }
		}
		rbdp->rbd.bd_count = SWAB(0);
		rbdp->rbd.bd_size |= SWAB(BD_EL);
		/* link this last used rbuf to the end of the list */
		rbdtail->rbd.bd_size &= SWAB(BD_COUNT); /* clear previous EL */
		rbdtail = rbdp;
		rbdhead = OTOA(struct r_bd *, rbdp->rbd.bd_next);
		/* increment total number of input packets */
		pvars.fpr_iestats->fpi_ipackets++;
		if (pp) {
			K_SPLIMP(&pri);
			pp->p_len = (MAXDATA + ESLOP) - free;
			p_if(pp) = &ifie;
			/* enqueue a buffer to the rec'd packet queue */
			K_PENQNP(pq,pp);
			K_SPLX(&pri);
		}
		rfdp->fd_status = SWAB(0);
		rfdp->fd_stat2 = SWAB(FD_EL | FD_S);
		rfdp->fd_rbd = SWAB(FD_NIL);
		rfdtail->fd_stat2 = SWAB(FD_S);	/* clear previous FD_EL */
		rfdtail = rfdp;
	}
}

/*
 * Start or restart output.
 */
iexstart()
{
	register struct pbuf *p;

	if (sendq->pq_head == 0) {
		sendf("leave iexstart() early");
		return;		/* nothing in output queue */
	}
	/* dequeue a buffer from the Ethernet transmit queue */
	K_PDEQNP(sendq,p);

	if (p->p_len < 60)
		p->p_len = 60;
	tbd[0].tbd.bd_count = SWAB(p->p_len | BD_EOF);
	tbd[0].tbd.bd_buf   = SWAB(LO16(p->p_off));
	tbd[0].tbd.bd_bufhi = SWAB(HI16(p->p_off));
	tbd[0].tpbuf = p;
	pvars.fpr_bufs->fpb_pbntypes[PT_ETBF]++;
	pvars.fpr_bufs->fpb_pbntypes[p->p_type]--;
	p->p_type = PT_ETBF;

	tactive = 1;
	WAIT_CMD();
}
	
/*
 * Execute a single command
 *	command structure is already pointed at by scbptr->sc_clist
 */
iecmd()
{
	WAIT_CMD();
	scbptr->sc_cmd = SWAB(ACK_NONE | CUC_START);
	K_CA86();	/* pull channel attention */

	/* wait for indication of completion */
	WAIT_CMD();
	while ((scbptr->sc_status & SWAB(STAT_CNA)) == 0);
	/* ack to clear interrupt */
	scbptr->sc_cmd = scbptr->sc_status & SWAB(ACK_CX | ACK_CNA);
	K_CA86();	/* pull channel attention */
}

/*
 * Start receiver, if needed.
 */
ierstart()
{
	/* ignore if RU already running or less than 2 elements on lists */
	if ((scbptr->sc_status & SWAB(SC_RUS)) == RUS_READY) {
#ifdef	STATS
		ieready++;
#endif	STATS
		return;
	}
	if (rfdhead->fd_stat2 & SWAB(FD_EL)) {
#ifdef	STATS
		iefcnt++;
#endif	STATS
		return;
	}
	if (rbdhead->rbd.bd_size & SWAB(BD_EL)) {
#ifdef	STATS
		iebuf++;
#endif	STATS
		return;
	}
	WAIT_CMD();
	rfdhead->fd_rbd = SWAB(LO16(rbdhead));
	scbptr->sc_rlist = SWAB(LO16(rfdhead));
}

/*
 * Ethernet output routine.
 * Encapsulate a packet of type af for the local net.
 */
ieoutput(ifp, p, af, dst)
struct ifnet *ifp;
struct pbuf *p;
u_char *dst;
{
	short pri;
	int type, s;
	u_char edst[6];
	iaddr_t idst;
	register struct ether_header *eh;
	struct LAP *lap;
	extern u_char etheraddr[];
	/* compiler oddity? */
	u_char savedst = *dst;

	switch (af) {
	    case AF_SDDP:
	    case AF_DDP:
		if (ifp->if_dnode == 0)
			goto drop;	 /* don't know who we are yet */

		idst = 0L;
		((char *)(&idst))[3] = *dst;
		if (!arpresolve(ifp, p, &idst, edst))
			return (0);	/* if not yet resolved */
		lap = (struct LAP *)p->p_off;
		lap->src = ifp->if_dnode;
		lap->dst = savedst;
		lap->type = (af == AF_DDP) ? lapDDP : lapShortDDP;
		type = ETHERTYPE_ATALKTYPE;
		break;

	    case AF_LINK:
		lap = (struct LAP *)p->p_off;
		lap->src = ifp->if_dnode;
		eaddrcopy(dst, edst);
		type = (ETHERTYPE_ATALKTYPE);
		break;

	    case AF_RTMP:
		if (ifp->if_dnode == 0)
			goto drop;	 /* don't know who we are yet */
		lap = (struct LAP *)p->p_off;
		lap->src = ifp->if_dnode;
		lap->dst = 0xff;
		lap->type = lapShortDDP;
		eaddrcopy(dst, edst);
		type = ETHERTYPE_ATALKTYPE;
		break;

	    case AF_ARP:
		eaddrcopy(dst, edst);
		type = ETHERTYPE_ARPTYPE;
		break;

#ifdef	IGP
	    case AF_PUP:
		bcopy((caddr_t)dst, (caddr_t)edst, hln);
		type = ETHERTYPE_PUPTYPE;
		break;

#endif	IGP
	    case AF_IP:
		idst = *(iaddr_t *)dst;
		if (!arpresolve(ifp, p, &idst, edst))
			return (0);	/* if not yet resolved */
		type = ETHERTYPE_IPTYPE;
		break;

	    default:
		panic("ie%d: can't handle af%d", ifp->if_unit, af);
	}

	/*
	 * Add local net header.
	 */
	p->p_off -= sizeof (struct ether_header);
	p->p_len += sizeof (struct ether_header);
	eh = (struct ether_header *)p->p_off;
	eh->ether_type = htons((u_short)type);
	eaddrcopy(edst, &eh->ether_dhost);
	eaddrcopy(etheraddr, &eh->ether_shost);

	/*
	 * Queue message on interface, and start output if interface
	 * not yet active.
	 */
	K_SPLIE(&pri);
	/* enqueue a buffer to the Ethernet transmit queue */
	K_PENQNP(sendq,p);
	if (tactive == 0) {
		iexstart();
		scbptr->sc_cmd = SWAB(CUC_START);
		K_CA86();	/* send transmit cmd */
	}
	K_SPLX(&pri);
	return (0);
drop:
	/* put the buffer back on the free list */
	K_PFREE(p);
	return(-1);
}


#ifdef	STATS
/*
 * Print Ethernet statistics
 */
ieprintstats()
{
	K_WAIT(20);
	sendf(
	"ethernet stats: in %d, out %d, inerrs %d, outerrs %d, norbufs %d",
		pvars.fpr_iestats->fpi_ipackets,
		pvars.fpr_iestats->fpi_opackets,
		pvars.fpr_iestats->fpi_ierrors,
		pvars.fpr_iestats->fpi_oerrors,
		norecbufs);
	K_WAIT(20);
	sendf(
	"spurious %d, false xmit %d, int count %d", iespur, iefalsexmit, eintr);
	K_WAIT(20);
	sendf("stuck %d, ok not set %d-0x%x", iestuck, oknotset, okbits);
	K_WAIT(20);
	sendf("ready %d, frame %d, buf %d, cmdbusy %d",
		ieready, iefcnt, iebuf, pvars.fpr_iestats->fpi_cmdbusy);
	K_WAIT(20);
	sendf("scb status 0x%x, errs crc: %d, aln %d, rsc %d, ovrn %d",
			SWAB(scbptr->sc_status), SWAB(scbptr->sc_crcerrs),
			SWAB(scbptr->sc_alnerrs), SWAB(scbptr->sc_rscerrs),
			SWAB(scbptr->sc_ovrnerrs));
}

struct cb iedump;
char dumpbuf[256];

/* dumps internal 82586 registers */
dump()
{
	iedump.cb_cmd = SWAB(CB_EL|CBC_DUMP);
	iedump.cb_param[0] = SWAB(LO16(dumpbuf));
	scbptr->sc_clist = SWAB(LO16(&iedump));
	iecmd();	/* dump */
	sendf("dump results (7E): %x %x %x %x %x %x",dumpbuf[0x7e],
		dumpbuf[0x7f], dumpbuf[0x80],dumpbuf[0x81],
		dumpbuf[0x82],dumpbuf[0x83]);
	/* set up for transmit command again */
	scbptr->sc_clist  = SWAB(LO16(&xmit));
	ierstart();	/* kick the receiver */
	scbptr->sc_cmd = SWAB(RUC_START);
	K_CA86();	/* start receives unit again, in case it stopped */
}
#endif	STATS
