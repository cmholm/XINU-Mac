#ifndef lint
static char sccsid[] = "@(#)atalkrd.c	1.1 (Stanford) 10/22/86";
#endif

/*
 * atalkrd - appletalk rebroadcast daemon.
 *
 * This daemon retransmits appletalk-in-IP packets received on IP socket
 * 'rebPort'.  It retransmits the packets to one or more IP addresses
 * supplied as command line arguments.  The outgoing IP port is determined
 * by translating the socket number found in the appletalk destination
 * socket field.
 * 
 * This retransmit / rebroadcast service is required for those ether
 * segments and ether gateways that do not support directed broadcast and
 * do not have an appletalk gateway (which can also act as a rebroadcast
 * server).
 * 
 * The daemon is started at boot time from /etc/rc.local and given as
 * arguments, one or more IP addresses.  Usually just one address is
 * supplied, the one used for local broadcast.  But if desired one can
 * instead list individual host addresses;  this may make sense if only a
 * few hosts on this ether cable are involved in appletalk traffic.
 * 
 * Since host name to address lookup is slow in 4.2 BSD, it is best not to
 * try converting this daemon to run from inetd;  but you could 
 * avoid name lookup by only calling it with numeric addresses.
 * 
 */


/*
 * (C) 1986, Stanford Univ.  CSLI.
 * May be used but not sold without permission.
 */


#include <sys/types.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <sys/time.h>
#include <netinet/in.h>
#define iaddr_t long

#include <signal.h>
#include <stdio.h>
#include <strings.h>
#include <errno.h>
#include <ctype.h>
#include <netdb.h>
#include "gwctl.h"

/* for 4.2 systems */
#ifndef FD_SETSIZE
# define FD_SETSIZE sizeof(int)*8
# define NFDBITS sizeof(int)*8
# define howmany(x,y) (1)
# define FD_SET(n, p)  (p)->fds_bits[0] |= (1<<(n))
# define FD_CLR(n, p)  (p)->fds_bits[0] &= ~(1<<(n))
# define FD_ISSET(n, p) ((p)->fds_bits[0] & (1<<(n)))
# define FD_ZERO(p) bzero((char *)p, sizeof(*p))
#endif

#define	NDADDR	16	/* max number of daddr's */


iaddr_t	daddr[NDADDR];	/* destination addresses */
int	ndaddr;		/* number of daddr's supplied on command line */
struct	sockaddr_in sin = { AF_INET };
int	s;		/* socket fd */
struct	sockaddr_in fsin; /* foreign sockaddr_in */
int	fsinlen;
struct timeval tvwait;	/* timer for select */
char	*atalkrlog = "/usr/adm/atalkrlog";
int	debug;
extern	int errno;

struct ap {		/* appletalk inside IP/UDP */
	u_char	ldst;	/* lap header */
	u_char	lsrc;
	u_char	ltype;
	u_char	dd[10];	/* misc ddp header */
	u_char	dstskt;
	u_char	srcskt;
	u_char	type;
	u_char	data[1024];
} ap;



main(argc, argv)
	char *argv[];
{
	register struct hostent *host;
	register int n, i;
	iaddr_t iaddr;

	for (argc--, argv++ ; argc > 0 ; argc--, argv++) {
		if (argv[0][0] == '-') {
			switch (argv[0][1]) {
			case 'd':
				debug++;
				break;
			}
			continue;
		} else {
			if (isdigit(**argv)) {
				if ((iaddr = inet_addr(*argv)) == -1 
				    || iaddr == 0)
					log("bad ipaddress %s", *argv);
			} else {
				if ((host = gethostbyname(*argv)) == 0)
					log("bad hostname %s", *argv);
				bcopy(host->h_addr, (caddr_t)&iaddr, 
				    sizeof iaddr);
			}
			if (ndaddr >= NDADDR) {
				log("too many addresses");
				break;
			}
			daddr[ndaddr++] = iaddr;
		}
	}
	if (ndaddr < 1) {
		log("must supply at least ONE address");
		exit(1);
	}
	if (debug == 0) {
		int t, f;
		if (fork())
			exit(0);
		for (f = 0; f < 10; f++)
			(void) close(f);
		(void) open("/", 0);
		(void) dup2(0, 1);
		(void) dup2(0, 2);
		t = open("/dev/tty", 2);	
		if (t >= 0) {
			ioctl(t, TIOCNOTTY, (char *)0);
			(void) close(t);
		}
	}

	while ((s = socket(AF_INET, SOCK_DGRAM, 0, 0)) < 0) {
		log("socket call failed");
		sleep(5);
	}
	sin.sin_port = htons(rebPort);
	if (bind(s, (caddr_t)&sin, sizeof (sin), 0) < 0) {
		log("bind call failed");
		exit(1);
	}
	for (;;) {
		fsinlen = sizeof (fsin);
		n = recvfrom(s, (caddr_t)&ap, sizeof ap,
			0, (caddr_t)&fsin, &fsinlen);
		if (n < 0) {
			log("recv failed");
			exit(1);
		}
		if (ap.ldst != 0xFA || ap.lsrc != 0xCE || ap.ltype != 2)
			continue;	/* not valid lap header */
		if (debug)
			log("recvfrom %s", inet_ntoa(fsin.sin_addr.s_addr));
		fsin.sin_port = htons(ddp2ipskt(ap.dstskt));
		for (i = 0 ; i < ndaddr ; i++) {
			fsin.sin_addr.s_addr = daddr[i];
			if (debug)
				log("sendto %s, ddpsoc %d, ipsoc %d", 
				    inet_ntoa(daddr[i]), ap.dstskt,
				    ntohs(fsin.sin_port));
			if (sendto(s, (caddr_t)&ap, n, 0, 
			    &fsin, sizeof fsin) != n){
				log("sendto failed");
				exit(1);
			}
		}
	}
}


/*
 * log an error message 
 */
log(fmt, args)
char *fmt;
{
	FILE *fp;
	long time(), tloc;
	struct tm *tm, *localtime();

	if (debug)
		fp = stderr;
	else
		if ((fp = fopen(atalkrlog, "a+")) == NULL)
			return;
	time(&tloc);
	tm = localtime(&tloc);
	fprintf(fp, "%d/%d %02d:%02d ", tm->tm_mon, tm->tm_mday,
		tm->tm_hour, tm->tm_min);
	_doprnt(fmt, &args, fp);
	putc('\n', fp);
	if (!debug)
		fclose(fp);
}
