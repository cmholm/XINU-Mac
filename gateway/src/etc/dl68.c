/* Downloader for MACSBUG.  
   Author Anonymous.
   Modified to store symbols: V. Pratt, Jan. 1981.
   Assumed to run on a machine with VAX-type byte order in a word,
   namely first text byte is least significant byte.  Modify the function
   reverse(lwrd) appropriately to run on another machine.
   Jan. 1982 - V. Pratt - Modified to load symbol table where the Sun
   	bootloader puts it
 */

#include "b.out.h"
#include <stdio.h>

#define STARTADDR 0x1000	/* Default start address */
#define RCDSIZE 12		/* Size of Motorola S-type records */
#define MACSSYMTAB 0x6BA	/* Symbol table starts here - MACSBUG */
#define SUNSYMTAB  0x1f000	/* Symbol table starts here - SUN */
#define SYMTAB (vflag=='m'?MACSSYMTAB:SUNSYMTAB)
#define SYMTABADDR 0x570	/* Boundaries of symbol table here */
#define MAXSYMS (vflag=='m'?((0x1000-0x6BA)/12):(0x1000/12))
#define MAXSYMST (0x1000/12)	/* Max size of symbol table for either
					SUN or MACSBUG */

#define hex(c) c>9 ? c+0X37 : c+0X30  /* macro for hex convert */

struct bhdr filhdr;

struct sym68 {			/* fake sym for 68000 */
	short dummy1;		/* ignored */
	char stype;		/* type */
	char slength;		/* length */
	long svalue;
};

char	Sdata=2;    	    	/* Default to S2/S8 records (24 bit addr) */
char	Send=8;
char	vflag=0;		/* 1 -> Design Module */
FILE *infile, *outfile;
char infilename[30], outfilename[30] = {0};
int syms = 0;

puthex(b)			/* print byte as two hex chars */
char b;
{
register char c1,c2;

    c1=(b>>4) & 0XF; c2=b & 0XF;
    c1=hex(c1); c2=hex(c2);
    fprintf(outfile,"%c%c",c1,c2);
}

typedef struct Msymtab 		/* structure of the MACSBUG symbol space */
    {char name[8];
     int value;
    } msymtab;

reverse(lwrd) unsigned lwrd;
 {return((lwrd>>24)	     |
	 (lwrd>>8 & 0xff00)  |
	 (lwrd<<8 & 0xff0000)|
	 (lwrd<<24)
	);
 }

int checksum;

checkout(c) char c; {checksum += c; puthex(c);}

/* Procedure to print out one ExorMacs record of given type */
print_exormacs_record(type,buffer,Dcount,addr) char *buffer; int Dcount, addr;
	{fprintf(outfile,"S%d",type); checksum = 0;
    	 if (type == 2 || type == 8)
    	    	{checkout((char) Dcount+4);
	    	 checkout((char) (addr>>16));
    	    	}
    	 else
    	    	checkout((char) Dcount+3);
	 checkout((char) (addr>>8));
	 checkout((char) addr);
	 while (Dcount--) checkout(*buffer++);
	 puthex(~(checksum & 0XFF)); 
	 fprintf(outfile,"\n");
	}

/* Procedure that puts out records from a buffer */
outbuf(buffer,addr,size)
char *buffer;
int addr, size;
{
int count,			/* counts total number of bytes processed*/
    Dcount;			/* number of bytes in current record */

    for(count=0; count<size; count += RCDSIZE) 
	{Dcount= (size-count<RCDSIZE) ? size-count : RCDSIZE;
	 print_exormacs_record(Sdata,buffer,Dcount,addr);
	 buffer += Dcount;
	 addr += Dcount;
	}
}

/* Procedure that puts out records  from a file */
outbin(file,addr,size)
FILE *file;
int addr, size;
{
int count,			/* counts total number of bytes processed*/
    Dcount;			/* number of bytes in current record */
char buffer[RCDSIZE];		/* buffer for data */

    for(count=0; count<size; count += RCDSIZE) 
	{Dcount= (size-count<RCDSIZE) ? size-count : RCDSIZE;
	 if(fread(buffer,Dcount,1,file) != 1)
	     {fprintf(stderr,"Read error\n"); exit(1);}
	 print_exormacs_record(Sdata,buffer,Dcount,addr);
	 addr += Dcount;
	}
}

#define lower(c) 'a' <= c && c <= 'z'
#define upper(c) 'A' <= c && c <= 'Z'
#define digit(c) '0' <= c && c <= '9'

macify(c) char c;	/* convert c to a form acceptable to MACSBUG */
 {return
   	(lower(c)?	c-32:
    	 upper(c)?	c:
    	 digit(c)?	c:
    	 c == '_'?	'.':
    	 /*default*/	'$'				/* rags to riches */
   	);
 }

/* Procedure to convert symbol table to .r format */
rbuild(table,source,size) struct sym68 *table; FILE *source; int *size;
   {struct sym s; int symno, chrno; char c; 
    struct sym68 *s68 = table;
    int pos;
    for (pos=0; pos < filhdr.ssize; )
       {if (!fread(&s,sizeof s,1,source)) break;/* Get symbol descriptor */
	pos += sizeof s;
	s68 = (struct sym68 *)((int)s68 -2);	/* kludge */
	s68->stype = s.stype;
	s68->slength = s.slength;
	s68->svalue = reverse(s.svalue);
	s68++;
	while (*(char *)s68 = getc(source)) {
		s68 = (struct sym68 *)((int)s68 + 1);
		pos++;
	}
	s68 = (struct sym68 *)((int)s68 + strlen(s68)+1);
	if ((int)s68 & 1) {
		*((char *)s68) = 0;		/* clear extra byte */
		s68 = (struct sym68 *)((int)s68 + 1);	/* word align */
	}
       }
    *size = (int)s68 - (int)table;
   }

/* Procedure to convert symbol table to MACSBUG format */
build(table,source,size) msymtab table[]; FILE *source; int *size;
   {struct sym s; int symno, chrno; char c;
    for (symno=0; symno<MAXSYMS; symno++)	/* Keep count of symbols */
       {if (!fread(&s,sizeof s,1,source)) break;/* Get symbol descriptor */
	chrno = 0;
	while ((c = getc(source)) && chrno<8)
	    table[symno].name[chrno++] = macify(c);/* Write chars of sym.*/
	while (chrno<8)				/* Pad rest with spaces */
	    table[symno].name[chrno++] = ' ';
	while (c) c = getc(source);		/* discard symbol tail */
	table[symno].value = reverse(s.svalue);	/* Supply symbol value */
       }
    *size = symno * 12;				/* return size of table */
    return((int)table);				/* return address of table */
   }

main(argc,argv)
int argc;
char *argv[];
{
int Sa, Daddr=STARTADDR, i;
msymtab table[MAXSYMST];

    if(argc<2)
	{fprintf(stderr,"Usage: dl68 infile [ > outfile ]\n");
	 exit(1);
	}
    for (i=1; i<argc; i++)				/* get options */
	if (argv[i][0] == '-')
	switch (argv[i][1]) {
		case 'T': if (i++ < argc)
			    sscanf(argv[i],"%x",&Daddr); 
			    break;
		case 'v':
			vflag = argv[i][2];
			break;
		case 'o':
			if (i++ < argc)
				strcpy(outfilename,argv[i]);
			break;
    	    	case 's':
    	    	    	Sdata = argv[i][2] - '0';
    	    	    	Send = argv[i][3] - '0';
    	    	    	break;
	}
	else strcpy(infilename,argv[i]);
    if((infile=fopen(infilename,"r"))==NULL)	 /* open infile */
       {strcat(infilename,".68");     			 /* try .68 form */
	if ((infile=fopen(infilename,"r"))==NULL) 
           {fprintf(stderr,"dl68: Can't open %s\n",infilename);
	    exit(1);
	   }
       }
    if (*outfilename) {
	if ((outfile = fopen(outfilename,"w")) == NULL) {
		fprintf(stderr,"dl68: Can't open %s\n",outfilename);
		exit(1);
	}
    }
    else outfile = stdout;
    if(fread(&filhdr, sizeof(struct bhdr),1,infile) != 1)/* get header info */
	{fprintf(stderr,"dl68: %s wrong format\n",infilename);
	 exit(1);
	}
    if(filhdr.fmagic != FMAGIC) {		/* check magic number */
	 fprintf(stderr,"dl68: %s not proper b.out file\n",infilename); 
	 exit(1);
    }

    Sa = Daddr;

/* Output records for TEXT portion */
    outbin(infile,Daddr,filhdr.tsize);
    Daddr += filhdr.tsize;    /* adjust output address */

    fseek(infile,DATAPOS,0);       /* seek to DATA portion */
/* Output records for DATA segment */
    outbin(infile,Daddr,filhdr.dsize);
    Daddr += filhdr.dsize;    /* adjust output address */

    fseek(infile,SYMPOS,0);	/* seek to SYM portion */
    if (filhdr.ssize) {
	int sz;
	if (vflag != 'm') {
		/* Output symbol table in boot loader format */
		int ssize68;
		static char stable[8192];	/* enough? */
		(char*)rbuild(stable,infile,&sz);
		ssize68 = reverse(sz);
		Daddr += filhdr.bsize;	/* skip bss (should be 0 really) */
		print_exormacs_record(Sdata,&ssize68,4,Daddr);
		Daddr += 4;
		outbuf(stable,Daddr,sz);
	}

	else {
		int tabpnt[2]; msymtab table[MAXSYMST]; char *bbuf;
		bbuf = (char*)build(table,infile,&sz);
		outbuf(bbuf,SYMTAB,sz);
		tabpnt[0] = reverse(SYMTAB);		/* Lower limit */
		tabpnt[1] = reverse(SYMTAB+sz);		/* Upper limit */
		outbuf(tabpnt,SYMTABADDR,sizeof tabpnt);/* Output pointers */
	}
    }
/* Output entry point: as in file header, else default to start address */
    print_exormacs_record(Send,0,0,filhdr.entry? filhdr.entry: Sa);
}


