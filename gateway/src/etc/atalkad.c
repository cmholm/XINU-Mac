#ifndef lint
static char sccsid[] = "@(#)atalkad.c	1.1 (Stanford) 10/22/86";
#endif

/*
 * Appletalk administration daemon.
 *
 * Answers request packets from client appletalk gateways by supplying
 * the appletalk-to-ip network configuration table and the local
 * appletalk gateway configuration information.
 */

/*
 * (C) 1986, Stanford Univ.  CSLI.
 * May be used but not sold without permission.
 */


#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <sys/file.h>
#include <sys/time.h>
#include <netinet/in.h>
#define iaddr_t long
#include "gwctl.h"

#include <signal.h>
#include <stdio.h>
#include <strings.h>
#include <errno.h>
#include <ctype.h>
#include <netdb.h>

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


int	debug;
extern	int errno;
struct	sockaddr_in sin = { AF_INET };
int	s;		/* socket fd */
struct	sockaddr_in fsin; /* foreign sockaddr_in */
int	fsinlen;
int	sig, sigint(), sighup();
struct aaconf aa;	/* receive packet */
struct aaconf aas;	/* send packet */
struct timeval tvwait;	/* timer for select */

/*
 * Globals below are associated with the atalka database file (atalkatab).
 */

char	*atalkatab = "/etc/atalkatab";
char	*atalkalog = "/usr/adm/atalkalog";
char	*atalkapid = "/usr/adm/atalkapid";
FILE	*fp;
char	line[256];	/* line buffer for reading atalkatab */
char	*linep;		/* pointer to 'line' */
int	linenum;	/* current ilne number in atalkatab */

#define	NANETS	64	/* max number of 'anets' structs */

struct anets {
	u_short	net;		/* atalk net */
	u_char	flags;		/* flags, see aroute* in gwctl.h */
	u_char	confsize;	/* size of databytes in conf below */
	iaddr_t	iaddr;		/* ip address */
	char	zone[32];	/* zone name */
	char	conf[64];	/* configuration info, if kbox */
} anets[NANETS];

int	nanets;		/* current number of anets */
long	modtime;	/* last modification time of atalkatab */


main(argc, argv)
	char *argv[];
{
	register int n;
	FILE *filep;
	int pid;

	for (argc--, argv++ ; argc > 0 ; argc--, argv++) {
		if (argv[0][0] == '-') {
			switch (argv[0][1]) {
			case 'd':
				debug++;
				break;
			}
			continue;
		}
		if (strcmp(argv[0], "boot") == 0)
			sig = SIGINT;
		else if (strcmp(argv[0], "route") == 0)
			sig = SIGHUP;
		else if (strcmp(argv[0], "exit") == 0)
			sig = SIGKILL;
		else {
			printf("usage: atalkad [-debug] [route|boot|exit]\n");
			exit(1);
		}
		if ((filep = fopen(atalkapid, "r")) == NULL
		    || fscanf(filep, "%d", &pid) != 1
		    || kill(pid, sig) < 0) {
			printf("failed to send signal to daemon\n");
			exit(1);
		} else {
			printf("sent signal to daemon\n");
			exit(0);
		}
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

	log("###	ATALKA daemon starting");
	pid = getpid();
	if ((filep = fopen(atalkapid, "w")) == NULL) {
		log("couldnt create pid file %s\n", atalkapid);
		exit(1);
	}
	fprintf(filep, "%d", pid); 
	fclose(filep);
	signal(SIGHUP, sighup);
	signal(SIGINT, sigint);

	while ((s = socket(AF_INET, SOCK_DGRAM, 0, 0)) < 0) {
		log("socket call failed");
		sleep(5);
	}
	sin.sin_port = htons(aaPort);
	if (bind(s, (caddr_t)&sin, sizeof (sin), 0) < 0) {
		log("bind call failed");
		exit(1);
	}
	readtab();
	for (;;) {
		fsinlen = sizeof (fsin);
		n = recvfrom(s, (caddr_t)&aa, sizeof aa,
			0, (caddr_t)&fsin, &fsinlen);
		readtab();
		if (n < 0) {
			if (errno != EINTR) {
				log("recv failed");
				exit(1);
			}
			sendall(sig);
			continue;
		}
		if (n < aaconfMinSize || ntohl(aa.magic) != aaMagic)
			continue;
		sendreply();
	}
}


/*
 * Interrupts are used to signal type of sendall().
 */
sighup() { sig = aaROUTEI; }
sigint() { sig = aaRESTART; }


/*
 * Send reply to packet aa.
 */
sendreply()
{
	register struct anets *an;
	register n;

	switch (aa.type) {
	case aaCONF:
		for (an = &anets[0], n = 0 ; n < nanets ; an++, n++)
			if (an->iaddr == aa.ipaddr && an->confsize
			    && (an->flags & arouteKbox))
				goto found;
		log("aaCONF from %s ***** not in table", inet_ntoa(aa.ipaddr));
		return;
found:
		log("aaCONF to %s", inet_ntoa(aa.ipaddr));
		n = an->confsize;
		bcopy(an->conf, aa.stuff, n);
		goto sendit;

	case aaROUTEI:
		log("aaROUTEI to %s", inet_ntoa(aa.ipaddr));
		n = buildart((struct arouteTuple *)aa.stuff);
sendit:
		aa.count = htons(n);
		sendto(s, (caddr_t)&aa, aaconfMinSize + n, 0,
			&fsin, sizeof fsin);
		return;
	}
}


/*
 * Build arouteTuple's into area provided by caller.
 * Return byte count of tuples deposited.
 */
buildart(at)
	register struct arouteTuple *at;
{
	register struct anets *an;
	register n, size;

	size = 0;
	for (an = &anets[0], n = 0 ; n < nanets ; an++, n++, at++) {
		size += sizeof *at;
		at->node = an->iaddr;
		at->net = an->net;
		at->flags = an->flags;
		at->hops = 1;
	}
	return (size);
}


/*
 * Send aaRESTART or aaROUTEI to all gateways.
 */
sendall(sig)
{
	register struct anets *an;
	register n;
	int trys, count, rcount;
	fd_set fds;
	struct sockaddr_in tsin;

	tsin = sin;
	log("sendall(%s)", sig == aaRESTART ? "aaRESTART" : "aaROUTEI");
	aas.magic = htonl(aaMagic); /* setup send packet */
	aas.type = sig;
	aas.flags = 0;
	if (sig == aaROUTEI)
		count = buildart((struct arouteTuple *)aas.stuff);
	else
		count = 0;
	aas.count = htons(count);
	count += aaconfMinSize;
	/*
	 * send to each kbox in the table.
	 */
	for (an = &anets[0], n = 0 ; n < nanets ; an++, n++) {
		if ((an->flags & arouteKbox) == 0)
			continue;
		trys = 0;
		tsin.sin_addr.s_addr = an->iaddr;
		sendto(s, (caddr_t)&aas, count, 0, &tsin, sizeof tsin);
		/*
		 * receive until we get a good reply or timeout.
		 */
		for (;;) {
			FD_ZERO(&fds);
			FD_SET(s, &fds);
			tvwait.tv_sec = 2;  /* select waits 2 seconds */
			tvwait.tv_usec = 0;
			if (select(NFDBITS, &fds, 0, 0, &tvwait) != 1) {
				/* timeout */
				if (++trys < 4) {
					sendto(s, (caddr_t)&aas, 
					    count, 0, &tsin, sizeof tsin);
					continue;
				}
				log("no response from %s",
				    inet_ntoa(an->iaddr));
				break;
			}
			fsinlen = sizeof fsin;
			rcount = recvfrom(s, (caddr_t)&aa, sizeof aa,
			    0, (caddr_t)&fsin, &fsinlen);
			if (rcount < 0) {
				if (errno == EINTR)
					continue;
				log("recv failed");
				exit(1);
			}
			if (rcount < aaconfMinSize 
			    || ntohl(aa.magic) != aaMagic)
				continue;
			if (aa.ipaddr != an->iaddr) {
				sendreply();
				continue;
			}
			/* our request got thru! */
			sendreply();
			break;
		}
	}
}


/*
 * Read atalkatab database file.  Avoid rereading the file if the
 * write date hasnt changed since the last time we read it.
 */
readtab()
{
	struct stat stat;
	register char *sp, *cpp;
	int v;
	register i;
	char st[128], *cp;
	register struct anets *an;
	iaddr_t iaddr;

	if (fp == 0) {
		if ((fp = fopen(atalkatab, "r")) == NULL) {
			log("can't open %s", atalkatab);
			exit(1);
		}
	}
	fstat(fileno(fp), &stat);
	if (stat.st_mtime == modtime && stat.st_nlink)
		return;	/* hasnt been modified or deleted yet */
	fclose(fp);
	if ((fp = fopen(atalkatab, "r")) == NULL) {
		log("can't open %s", atalkatab);
		exit(1);
	}
	fstat(fileno(fp), &stat);
	log("(re)reading %s", atalkatab);
	modtime = stat.st_mtime;
	nanets = 0;
	an = &anets[-1];
	linenum = 0;

	/*
	 * read and parse each line in the file.
	 */
	for (;;) {
		if (fgets(line, sizeof line, fp) == NULL)
			break;	/* done */
		if ((i = strlen(line)))
			line[i-1] = 0;	/* remove trailing newline */
		linep = line;
		linenum++;
		if (line[0] == '#' || line[0] == 0)
			continue;	/* skip comment lines */
		if (line[0] == ' ' || line[0] == '\t') 
			goto confinfo;
		/*
		 * lines not beginning with white space 
		 * represent a new net #
		 */
		if (++nanets > NANETS) {
			log("'anets' table length exceeded");
			exit(1);
		}
		an++;
		cp = an->conf;	/* store following lines here */
		an->confsize = 0;
		getfield(st, sizeof st);
		an->net = htons(getashort(st));
		getfield(st, sizeof st);
		i = 0;
		/* parse flags */
		for (cpp = st ; *cpp ; cpp++) {
			if (isupper(*cpp))
				*cpp = tolower(*cpp);
			switch (*cpp) {
			case 'c':
				i |= arouteCore;  break;
			case 'k':
				i |= arouteKbox;  break;
			case 'h':
				i |= arouteHost;  break;
			case 'n':
				i |= arouteNet;  break;
			case '0': case '1': case '2': case '3':
				i |= (*cpp - '0');  break;
			default:
				log("bad switch %s, linenum %d", st, linenum);
			}
		}
		an->flags = i;
		getfield(st, sizeof st);
		an->iaddr = getiaddr(st);
		getfield(an->zone, sizeof an->zone);
		continue;
confinfo:
		/*
		 * lines beginning with white space
		 * are configuration data for gateway.
		 */
		for (;;) {	/* for each field in line */
			int len;
			getfield(st, sizeof st);
			sp = st;
			if (*sp == 0)
				break;
			if (isupper(*sp))
				*sp = tolower(*sp);
			switch (*sp++) {
			case 'i':
				/* IP address name or number */
				iaddr = getiaddr(sp);
				bcopy((caddr_t)&iaddr, cp, sizeof iaddr);
				cp += sizeof iaddr;
				an->confsize += sizeof iaddr;
				break;

			case 'l':
				if ((int)cp & 1) 
					goto badalign;
				*(long *)cp = htonl(atoii(sp));
				cp += 4;
				an->confsize += 4;
				break;

			case 's':
				if ((int)cp & 1) 
					goto badalign;
				*(short *)cp = htons(getashort(sp));
				cp += 2;
				an->confsize += 2;
				break;
			
			case 'c':
				*cp = atoii(sp);
				cp++;
				an->confsize++;
				break;

			case '"':
				len = strlen(sp) - 1; /* drop trailing " */
				bcopy(sp, cp, len);
				cp += len;
				*cp++ = 0;
				len++;
				an->confsize += len;
				break;
			
			default:
				log("bad field type %s, linenum %d",
					st, linenum);
				break;
			}
			continue;	/* get next field in line */
badalign:
			log("long/short bad alignment %s, linenum %d",
				st, linenum);
			break;
		}
		/* get next line */
	}
	/* end of file */
}


/*
 * Get next field from 'line' buffer into 'str'.  'linep' is the 
 * pointer to current position.
 */
getfield(str, len)
	char *str;
{
	register char *cp = str;

	for ( ; *linep && (*linep == ' ' || *linep == '\t') ; linep++)
		;	/* skip spaces/tabs */
	if (*linep == 0 || *linep == '#') {
		*cp = 0;
		return;
	}
	len--;	/* save a spot for a null */
	for ( ; *linep && *linep != ' ' 
	    && *linep != '\t' && *linep != '#' ; linep++) {
		*cp++ = *linep;
		if (--len <= 0) {
			*cp = 0;
			log("string truncated: %s, linenum %d", str, linenum);
			return;
		}
	}
	*cp = 0;
}


/*
 * Ascii to integer, with base check.
 */
atoii(s)
	register char *s;
{
	int v;
	char *c;

	if (isupper(*s))
		*s = tolower(*s);
	if (*s == 'x' || *s == 'h') {
		c = "%x";
		s++;
	} else {
		c = "%d";
	}
	if (sscanf(s, c, &v) != 1)
		log("bad numeric field %s, linenum %d", s, linenum);
	return (v);
}


/*
 * Get an internet address as a hostname or dot format string.
 */
getiaddr(st)
	register char *st;
{
	iaddr_t iaddr;
	register struct hostent *host;

	if (isdigit(*st)) {
		if ((iaddr = inet_addr(st)) == -1 || iaddr == 0)
			log("bad ipaddress %s, linenum %d", st, linenum);
	} else {
		if ((host = gethostbyname(st)) == 0)
			log("bad hostname %s, linenum %d", st, linenum);
		bcopy(host->h_addr, (caddr_t)&iaddr, sizeof iaddr);
	}
	return (iaddr);
}


/*
 * Get a short number or address.
 */
getashort(st)
	register char *st;
{
	register char *cp;

	if ((cp = index(st, '.')) == 0)
		return (atoii(st));
	*cp++ = 0;
	return ((atoii(st)<<8) | atoii(cp));
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
		if ((fp = fopen(atalkalog, "a+")) == NULL)
			return;
	time(&tloc);
	tm = localtime(&tloc);
	fprintf(fp, "%d/%d %02d:%02d ", tm->tm_mon + 1, tm->tm_mday,
		tm->tm_hour, tm->tm_min);
	_doprnt(fmt, &args, fp);
	putc('\n', fp);
	if (!debug)
		fclose(fp);
}
