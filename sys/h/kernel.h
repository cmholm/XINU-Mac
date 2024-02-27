/* kernel.h - disable, enable, halt, restore, isodd, min */

/* Symbolic constants used throughout Xinu */

typedef	char		Bool;		/* Boolean type			*/
#define	FALSE		0		/* Boolean constants		*/
#define	TRUE		1
#define	NULL		(char *)0	/* Null pointer for linked lists*/
#define	NULLCH		'\0'		/* The null character		*/
#define	NULLSTR		""		/* Pointer to empty string	*/
#define	SYSCALL		int		/* System call declaration	*/
#define	LOCAL		static		/* Local procedure declaration	*/
#define	COMMAND		int		/* Shell command declaration	*/
#define	BUILTIN		int		/* Shell builtin " "		*/
#define	INTPROC		int		/* Interrupt procedure  "	*/
#define	PROCESS		int		/* Process declaration		*/
#define	RESCHYES	1		/* tell	ready to reschedule	*/
#define	RESCHNO		0		/* tell	ready not to resch.	*/
#define	MININT		0100000		/* minimum integer (-32768)	*/
#define	MAXINT		0077777		/* maximum integer		*/
#define	LOWBYTE		0377		/* mask for low-order 8 bits	*/
#define	HIBYTE		0177400		/* mask for high 8 of 16 bits	*/
#define	LOW16		0177777		/* mask for low-order 16 bits	*/
#define A5		(8+5)		/* reg. A5 is global data ptr	*/
#define	SP		(8+7)		/* reg.	A7 is stack pointer	*/
#define	PC		16		/* program counter in 16th reg.	*/
#define	PS		17		/* proc. status	in 17th reg. loc*/
#define	MINSTK		40L		/* minimum process stack size	*/
#define	NULLSTK		4000L		/* process 0 stack size		*/
#define	DISABLE		0x2700		/* PS to disable interrupts	*/
#define	MAGIC		0125252		/* unusual value for top of stk	*/

/* Universal return constants */

#define	OK		 1		/* system call ok		*/
#define	SYSERR		-1		/* system call failed		*/
#define	EOF		-2		/* End-of-file (usu. from read)	*/
#define	TIMEOUT		-3		/* time out  (usu. recvtim)	*/
#define	INTRMSG		-4		/* keyboard "intr" key pressed	*/
					/*  (usu. defined as ^B)	*/
/* Initialization constants */

#define	EVTSTK		8000L		/* eventd process stack		*/
#define	EVTPRIO		1000		/* eventd process priority	*/
#define	EVTNAME		"eventd"	/* eventd process name		*/
#define	EVTARGS		0		/* eventd count/arguments	*/
#define	INITSTK		8000L		/* initial process stack	*/
#define	INITPRIO	20		/* initial process priority	*/
#define	INITNAME	"main"		/* initial process name		*/
#define	INITARGS	1,0		/* initial count/arguments	*/
#define	INITRET		userret		/* processes return address	*/
#define	INITPS		0x2000L		/* initial process PS		*/
#define	INITREG		0L		/* initial register contents	*/
#define INITA5		(*(long *)0x904)/* global data pointer		*/
#define	QUANTUM		5		/* clock ticks until preemption	*/

/* Miscellaneous utility inline functions */

#define	isodd(x)	(01&(int)(x))
#define	min(a, b)	( (a) < (b)? (a) : (b) )
#define	max(a, b)	( (a) > (b)? (a) : (b) )
#define	disable(ps)	_disable(&ps)	/* disable interrupts		*/
#define	restore(ps)	_restore(&ps)	/* restore interrupt status	*/

extern	int	rdyhead, rdytail;
extern	int	preempt;
