BINDIR=/etc
LOGDIR=/usr/adm
PROGS=atalkad atalkrd dl68
CFLAGS=-I..

all:	${PROGS}

atalkad:	atalkad.o
	cc -o atalkad atalkad.o

atalkrd:	atalkrd.o
	cc -o atalkrd atalkrd.o

dl68:	dl68.o
	cc -o dl68 dl68.o

install: ${PROGS}
	cp atalkad $(BINDIR)

installonce:
	cp atalkatab atalk.local $(BINDIR)
	cat /dev/null >>$(LOGDIR)/atalkalog
