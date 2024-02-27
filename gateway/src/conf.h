/*
 * Gateway configuration.
 *
 * For Kinetics, struct 'conf' is initialized via the fpr_state
 * vector.  For a Seagate, this conf.h file is edited to fill the
 * struct with initialized data.
 *
 * $Header$
 */


#ifdef SEAGATE

#define IPDOT(a,b,c,d) ((a<<24)+(b<<16)+(c<<8)+d)

struct conf conf = {
	IPDOT(36,9,0,240),	/* ipaddr */
	IPDOT(36,9,0,9),	/* ipmaster */
	IPDOT(36,9,0,1),	/* iproutedef */
	{ 0x08, 0x00, 0x89, 0xF0, 0x05, 0x30 },	/* etheraddr */
	0x1234,			/* ready */
	IPDOT(36,9,0,255),	/* ipbroad */
	IPDOT(36,53,0,10),	/* ipname */
	IPDOT(36,9,0,9),	/* ipdebug */
	0,0,			/* ipsmask, ipsshift (currently unused) */
	0,			/* flags */
	0x0001,			/* ipstatic */
	0x0003,			/* ipdynamic */
	0x0051,			/* atneta */
	0x0000,			/* atnete */
	"TWILIGHT"		/* zone */
};

#define	ifie_proto ifie
#define	ifab_proto ifab

#endif SEAGATE

extern int ieoutput(), aboutput(), iematch();

struct ifnet ifie_proto = {
	"ie", 0, 			/* intel/interlan, unit 0 */
 	0, 0x800,			/* x.x.x.x, type 0x800 */
	6, 1,				/* ether addr is 6 bytes, format 1 */
	ieoutput, iematch
};

struct ifnet ifab_proto = {
	"ab", 0,			/* AppleTalk, unit 0 */
	0, 0x800,			/* x.x.x.x, type 0x800 */
	4, 3,				/* AddrBlock 4 bytes, format 3 */
	aboutput, /*abmatch*/ 0
};

