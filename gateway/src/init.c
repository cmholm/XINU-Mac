/*
 * Initialize data structures that might otherwise be initialized
 * at compile time, but for re-startable RAM code, must be at run-time.
 *
 * (c) Copyright 1986, Kinetics, Inc.
 * May be used but not sold without permission.
 *
 * $Header$
 */

#include "gw.h"
#include "fp/pbuf.h"
#include "ab.h"
#include "ether.h"
#include "inet.h"
#include "ie.h"
#include "fp/cmdmacro.h"

#include "glob.h"

/* from ie.c */
/*
 * Configuration command data (from page 4-5 of NI3210 manual).
 */
struct cb ieconf_proto = {
	SWAB(0),	SWAB(CB_EL|CBC_CONFIG),		SWAB(0xffff),
	/* 1st swab was 0x080c, 2nd was 2e40 */
	SWAB(0x020c),	SWAB(0x2ec0),			SWAB(0x6000),
	/* 2nd swap was 0x0000 */
	SWAB(0xf200),	SWAB(0x0400),			SWAB(0x0040) };

/* from send.c */
extern short dlen;
extern char outbuf[];
u_char lap_head[3];
u_char atwr_diag[1];

struct fp_atwrite atwr_proto[] = {
	3, (u_char *) lap_head,
	2, (u_char *) &dlen,
	1, (u_char *) atwr_diag,
	0, (u_char *) outbuf,
	0, (u_char *) 0
};

struct fp_proelem elem[1];
struct fp_protect prot;

data_init()
{
	/* from gw.c */
	extern struct ifnet ifie_proto, ifab_proto;
	/* from ie.c */
	extern struct cb xmit;
	extern struct cb iasetup;
	extern struct cb ieconfig;
	/* from send.c */
	extern struct fp_atwrite the_pkt[];
	register struct fp_atwrite *ap,*bp;

	/* from gw.c */
	ifie = ifie_proto;
	ifab = ifab_proto;

	/* from ie.c */
	xmit.cb_cmd = SWAB(CB_EL|CB_I|CBC_TRANS);
	iasetup.cb_cmd = SWAB(CB_EL | CBC_IASETUP);
	ieconfig = ieconf_proto;

	/* from send.c */
	lap_head[0] = (u_char) 0xFF;
	lap_head[1] = (u_char) 0x00;
	lap_head[2] = (u_char) 'K';
	atwr_diag[0] = 'D';
	ap = the_pkt;
	bp = atwr_proto;
	while (ap < &the_pkt[5]) {
		*ap++ = *bp++;
	}
}

checksum(begin,dataend)
char *begin;
char *dataend;
{
	/* set up protected ram areas */
	/* beginning of text segment through the end of initialized data */
	elem[0].fpp_memp = begin;
	elem[0].fpp_count = ((int)dataend) - ((int)begin);

	/* make the protect call */
	prot.fpt_oper = PR_PROTECT;
	prot.fpt_count = 1;
	prot.fpt_elem = elem;
	K_PROTECT(&prot);
}
