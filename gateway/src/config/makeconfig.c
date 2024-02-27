/*
 * simple program to configure a kfps for KIP
 *
 * Charlie C. Kim, User Services Group, Academic Information Services Division,
 *  Columbia University, March 1987
*/
#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>

char gwname[22];		/* kfps name: max len 21 */
char gwsrec[22];		/* gw.srec name: max len 21 */
struct in_addr ipaddr;		/* kfps ip address */
struct in_addr admin;		/* kfps admin host address */
struct in_addr gwaddr;		/* kfps gw address */
int serial;

char *ptime(), *hname();
char tmpbuf[100];

main()
{
  if (!isatty(fileno(stdin))) {
    close(2);
    open("/dev/null",2);
  }
  gwsrec[22] = gwname[22] = '\0'; /* tie off just in case */
  fprintf(stderr,"KFPS configurator\n\n");
  fprintf(stderr,"Input your gateway name\n");
  getstr(tmpbuf);
  strncpy(gwname, tmpbuf, 21);
  fprintf(stderr,"Input the default gw srec file name\n");
  getstr(tmpbuf);
  strncpy(gwsrec, tmpbuf, 21);
  fprintf(stderr,"Input the ip address of the KFPS box\n");
  getipaddr(&ipaddr);
  fprintf(stderr,"Input the ip address of the KIP administrative host\n");
  getipaddr(&admin);
  fprintf(stderr,
	 "Input the ip address of default IP gateway for the KFPS box\n");
  getipaddr(&gwaddr);
  fprintf(stderr,"Give the serial number of your KFPS box\n");
  getstr(tmpbuf);
  serial = atoi(tmpbuf);

  fprintf(stderr,"%s loads from %s\n",gwname,gwsrec);
  fprintf(stderr,"kfps ip address is: %s, ",inet_ntoa(ipaddr));
  fprintf(stderr,"admin: %s, ",inet_ntoa(admin));
  fprintf(stderr,"gw: %s\n",inet_ntoa(gwaddr));
  printf("* Config file for KFPS %s, KIP versions 10/86 and 02/87\n", gwname);
  printf("* Generated %s\n", ptime());
  printf("*\n");
  printf("* These bytes are ignored but must be left as placeholders: \n");
  printf("0000 0000 FF FF FF 00 000000 000000\n");
  printf("* Gateway name: %s\n", gwname);
  hexdump(gwname, 21);
  printf("* Load file name: %s\n", gwsrec);
  hexdump(gwsrec, 21);
  printf("* reserved (this field should be 00FF): \n00FF\n");
  printf("*\n");
  printf("* Start of 'mandatory' parameters, the minimum information that\n");
  printf("* must be supplied for the gateway to begin operation.\n");
  printf("*\n");
  printf("* My (KFPS) ip address: %s (%s)\n",inet_ntoa(ipaddr),hname(ipaddr));
  printf("%08X\n",htonl(ipaddr.s_addr));
  printf("* Admin host: %s (%s)\n",inet_ntoa(admin),hname(admin));
  printf("%08X\n",htonl(admin.s_addr));
  printf("* IP default route: %s (%s)\n",inet_ntoa(gwaddr),hname(gwaddr));
  printf("%08X\n",htonl(gwaddr.s_addr));
  printf("* KFPS enet address\n080089 %06x\n",serial);
  printf("* next value is a flag, if it is '1234' the remainder \n");
  printf("* of this file is considered valid;  any other value means\n");
  printf("* that the remaining parameters will be obtained from atalkad.\n");
  printf("0000\n");
}

getipaddr(ipaddr)
struct in_addr *ipaddr;
{
  struct hostent *host;

  do {
    getstr(tmpbuf);
    if ((host=gethostbyname(tmpbuf))) {
      bcopy(host->h_addr, (caddr_t)ipaddr, host->h_length);
      return;
    }
    if ((ipaddr->s_addr = inet_addr(tmpbuf)) != -1)
      return;
    fprintf(stderr,"Invalid ip address or unknown name, please try again\n");
  } while (1);
}

/*
 * get a string allowing for comments and blank lines
 *
*/
getstr(buf)
char *buf;
{
  do {
    gets(buf);
    if (buf[0] != '*' && buf[0] != '\0' && buf[0] != '\n')
      break;
  } while (1);
}

/*
 * returns nicely formated time string
*/
char *
ptime()
{
  long clock;
  char *timestr;

  clock = time(0);
  timestr = (char *)asctime(localtime(&clock));
  timestr[24] = '\0';			/* pretty hooky - but it works */
  return(timestr);
}

hexdump(s, len)
char *s;
int len;
{
  while (len--)
    printf("%02x",*s++);
  putchar('\n');
}

char *
hname(ipaddr)
struct in_addr ipaddr;
{
  struct hostent *host;

  host = gethostbyaddr(&ipaddr.s_addr, sizeof(ipaddr.s_addr), AF_INET);
  if (!host)
    return("name unknown");
  return(host->h_name);
}
