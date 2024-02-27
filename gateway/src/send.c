/*
 *  (c) 1986, Kinetics, Inc.
 *  May be used but not sold without permission.
 *
 *  $Header: /usr/fp/src/fps/ram.udp/RCS/send.c,v 1.13 86/02/21 20:41:22 tim Rel $
 */

/*
 * Kernel/standalone sendf routines:
 *	- implement a reasonablly full-bodied printf except output to
 *	  appletalk with the following capabilities:
 *		%d %D %o %O %x %X %u %U %c %s
 *	- the numeric output descriptors can also optionally have a fill
 *	  width and zero filling ala standard printf (i.e. %2x, %02x)
 */

#include "gw.h"
#include "fp/pbuf.h"
#include "fp/cmdmacro.h"

char outbuf[256];
short dlen;
char *ocp;
struct fp_atwrite the_pkt[5];

char	tohex[]	= "0123456789ABCDEF";

sendch(cc)
int cc;
{
	switch (cc) {
		case -1:		/* flush */
			/* send a diagnostic/debug appletalk packet */
			dlen = ocp - outbuf;	/* length of diagnostic msg */
			the_pkt[3].fpw_length = dlen;
			dlen += 1 + 2;	/* add length of "D" and dlen */
			K_ATWRITE(the_pkt);
			break;
		
		case 0:			/* initialize */
			dlen = 0;
			ocp = outbuf;
			break;
		
		default:		/* any other char */
			*ocp++ = cc;
			break;
	}
	return;
}

/*
 * sendn:
 *	- send an unsigned long in base "base", using an output width of
 *	  "width", and zero filling if "zfill" is '0'.
 */
sendn(n, base, width, zfill)
register unsigned long n;
register int base, width, zfill;
{
	register int dig;
	register char c;
	char buf[30];

	dig = 0;
	if (n) {
		while (n) {
			buf[dig++] = tohex[n % base];
			n /= base;
		}
	} else
		buf[dig++] = '0';

    /* pad to width, then output result */

	while (dig < width) {
		if (zfill == '0')
			buf[dig++] = '0';
		else
			buf[dig++] = ' ';
	}
	while (dig) {
		c = buf[--dig];
		sendch(c);
	}
}

/*VARARGS*/
sendf(fmt, x1)
char *fmt;
int x1;
{
	int *adx = &x1;
	int c, base, width, zfill;
	char *s;
	int i;

	sendch(0);	/* init sendch */
	for (;;) {
		while ((c = *fmt++) != '%') {
			if (c == '\0') {
				sendch(-1);	/* do fflush */
				return;
			}
			sendch(c);
		}

		c = *fmt++;
		width = 0;
		zfill = c;
		while ((c >= '0') && (c <= '9')) {
			width = width*10  + (c - '0');
			c = *fmt++;
		}
		switch (c) {
		  case 0:
			sendch(-1);
			return;
		  case 'd': case 'u': case 'D': case 'U':
			sendn(*adx, 10, width, zfill);
			break;
		  case 'o': case 'O':
			sendn(*adx, 8, width, zfill);
			break;
		  case 'x': case 'X':
			sendn(*adx, 16, width, zfill);
			break;
		  case 's':
			s = (char *)*adx;
			while (c = *s++) {
				sendch(c);
			}
			break;
		  case 'c':
			sendch(*adx);
			break;
		}
		adx++;
	}
}

