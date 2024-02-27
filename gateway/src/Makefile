#
# makefile for KFPS/IP (KIP)
#
# Copyright (c) 1986 Kinetics, Inc.
#
# $Header$
#
TARG=	gw
CSRCS=	gw.c gw2.c rtmp.c ie.c arp.c send.c init.c
ASRCS=	crt0.s gwasm.s
CRT0=	crt0.b
OBJS=	gwasm.b gw.b gw2.b rtmp.b ie.b arp.b send.b init.b
LIBS=	-lc
INCPATH=.
PBUFH=$(INCPATH)/fp/pbuf.h $(INCPATH)/fp/cmdmacro.h
INCS=	-I$(INCPATH)
CC=	/usr/stanford/bin/cc68
#CFLAGS=	-z $(INCS) -DSTATS -DEDATA -DIP
#CFLAGS=	-z $(INCS) -DSTATS -DIP
CFLAGS=	$(INCS) -Dnoprepend -DPURDUE -DKINETICS -DSTATS
AS=	/usr/stanford/bin/as68
LD=	/usr/stanford/bin/ld68

.SUFFIXES: .b

.c.b:
	$(CC) -c $(CFLAGS) $<

.s.b:
	m4 < $< > $<.x
	$(AS) -g -o $@ $<.x

all: $(TARG).srec

load: $(TARG).srec
	macput -u $(TARG).srec

$(TARG).srec: $(TARG).out
	etc/dl68 -T 64000 -o x.srec $(TARG).out
	cat s0 x.srec > $(TARG).srec
	rm x.srec
#	etc/efsputtext gw.srec ../mac

$(TARG).out: $(CRT0) $(OBJS) makefile
	rm -f $(TARG).out
	$(LD) -T 64000 -q -X -o $(TARG).out -x $(CRT0) $(OBJS) $(LIBS)
	cp $(TARG).out $(TARG).sym
	strip $(TARG).out

lint:
	lint -nhx $(LIBS) $(CSRCS)

clean:
	rm -i *.b *.out

#gw.h:	mung_gw.h
#$(INCPATH)/fp/cmdmacro.h: $(INCPATH)/fp/cmdidx.h
#arp.b:	gw.h $(PBUFH) inet.h 
#gw.b:	gw.h gwctl.h $(PBUFH) ab.h ether.h inet.h conf.h
#gw2.b:	gw.h gwctl.h $(PBUFH) ab.h inet.h 
#ie.b:	gw.h $(PBUFH) ie.h ether.h ab.h 
#rtmp.b:	gw.h gwctl.h $(PBUFH) ab.h inet.h 
#send.b:	gw.h $(PBUFH) 
#gwasm.b:	kfps.h
#$(CRT0):	kfps.h
#init.b:	gw.h $(PBUFH) ab.h ether.h inet.h ie.h 
