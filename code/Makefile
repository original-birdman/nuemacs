# makefile for emacs, updated Sun Apr 28 17:59:07 EET DST 1996

# Make the build silent by default
V =

ifeq ($(strip $(V)),)
	E = @echo
	Q = @
else
	E = @\#
	Q =
endif
export E Q

uname_S := $(shell sh -c 'uname -s 2>/dev/null || echo not')

PROGRAM=uemacs

SRC=ansi.c basic.c bind.c buffer.c crypt.c display.c eval.c exec.c \
	file.c fileio.c ibmpc.c input.c isearch.c line.c lock.c main.c \
	pklock.c posix.c random.c region.c search.c spawn.c tcap.c \
	termio.c vmsvt.c vt52.c window.c word.c names.c globals.c version.c \
	usage.c wrapper.c utf8.c complet.c

OBJ=ansi.o basic.o bind.o buffer.o crypt.o display.o eval.o exec.o \
	file.o fileio.o ibmpc.o input.o isearch.o line.o lock.o main.o \
	pklock.o posix.o random.o region.o search.o spawn.o tcap.o \
	termio.o vmsvt.o vt52.o window.o word.o names.o globals.o version.o \
	usage.o wrapper.o utf8.o complet.o

HDR=ebind.h edef.h efunc.h epath.h estruct.h evar.h util.h version.h

# DO NOT ADD OR MODIFY ANY LINES ABOVE THIS -- make source creates them

UTF8DIR=../../utf8proc
LIBUTF8=$(UTF8DIR)/libutf8proc.a

CC=gcc
WARNINGS=-Wall -Wstrict-prototypes
CFLAGS=-O3 $(WARNINGS) -std=gnu99 -DGGR_MODE -I$(UTF8DIR)
# CFLAGS=-O0 -g $(WARNINGS) -std=gnu99 -DGGR_MODE
#CC=c89 +O3			# HP
#CFLAGS= -D_HPUX_SOURCE -DSYSV
#CFLAGS=-O4 -DSVR4		# Sun
#CFLAGS=-O -qchars=signed	# RS/6000
ifeq ($(uname_S),Linux)
 DEFINES=-DAUTOCONF -DPOSIX -DUSG -D_DEFAULT_SOURCE -D_BSD_SOURCE -D_SVID_SOURCE -D_XOPEN_SOURCE=600
endif
ifeq ($(uname_S),FreeBSD)
 DEFINES=-DAUTOCONF -DPOSIX -DSYSV -D_FREEBSD_C_SOURCE -D_DEFAULT_SOURCE -D_BSD_SOURCE -D_SVID_SOURCE -D_XOPEN_SOURCE=600
endif
ifeq ($(uname_S),Darwin)
 DEFINES=-DAUTOCONF -DPOSIX -DSYSV -D_DARWIN_C_SOURCE  -D_DEFAULT_SOURCE -D_BSD_SOURCE -D_SVID_SOURCE -D_XOPEN_SOURCE=600
endif
#DEFINES=-DAUTOCONF
#LIBS=-ltermcap                 # BSD
LIBS=$(LIBUTF8) -lcurses        # SYSV
#LIBS=-ltermlib
#LIBS=-L/usr/lib/termcap -ltermcap
LFLAGS=-hbx
BINDIR=/usr/bin
LIBDIR=/usr/lib

$(PROGRAM): $(OBJ)
	$(E) "  LINK    " $@
	$(Q) $(CC) $(LDFLAGS) $(DEFINES) -o $@ $(OBJ) $(LIBS)

SPARSE=sparse
SPARSE_FLAGS=-D__LITTLE_ENDIAN__ -D__x86_64__ -D__linux__ -D__unix__

sparse:
	$(SPARSE) $(SPARSE_FLAGS) $(DEFINES) $(SRC)

clean:
	$(E) "  CLEAN"
	$(Q) rm -f $(PROGRAM) core lintout makeout tags makefile.bak *.o

install: $(PROGRAM)
	strip $(PROGRAM)
	cp em ${BINDIR}
	cp emacs.hlp ${LIBDIR}
	cp emacs.rc ${LIBDIR}/.emacsrc
	chmod 755 ${BINDIR}/em
	chmod 644 ${LIBDIR}/emacs.hlp ${LIBDIR}/.emacsrc

lint:	${SRC}
	@rm -f lintout
	lint ${LFLAGS} ${SRC} >lintout
	cat lintout

errs:
	@rm -f makeout
	make em >makeout

tags:	${SRC}
	@rm -f tags
	ctags ${SRC}

source:
	@mv makefile makefile.bak
	@echo "# makefile for emacs, updated `date`" >makefile
	@echo '' >>makefile
	@echo SRC=`ls *.c` >>makefile
	@echo OBJ=`ls *.c | sed s/c$$/o/` >>makefile
	@echo HDR=`ls *.h` >>makefile
	@echo '' >>makefile
	@sed -n -e '/^# DO NOT ADD OR MODIFY/,$$p' <makefile.bak >>makefile

depend: ${SRC}
	@for i in ${SRC}; do\
	    cc ${DEFINES} -MM $$i ; done >makedep
	@echo '/^# DO NOT DELETE THIS LINE/+2,$$d' >eddep
	@echo '$$r ./makedep' >>eddep
	@echo 'w' >>eddep
	@cp makefile makefile.bak
	@ed - makefile <eddep
	@rm eddep makedep
	@echo '' >>makefile
	@echo '# DEPENDENCIES MUST END AT END OF FILE' >>makefile
	@echo '# IF YOU PUT STUFF HERE IT WILL GO AWAY' >>makefile
	@echo '# see make depend above' >>makefile

.c.o:
	$(E) "  CC      " $@
	$(Q) ${CC} ${CFLAGS} ${DEFINES} -c $*.c

# DO NOT DELETE THIS LINE -- make depend uses it

ansi.o: ansi.c estruct.h edef.h
basic.o: basic.c estruct.h edef.h efunc.h line.h utf8.h
bind.o: bind.c estruct.h edef.h efunc.h epath.h line.h utf8.h util.h
buffer.o: buffer.c estruct.h edef.h efunc.h line.h utf8.h
crypt.o: crypt.c estruct.h edef.h efunc.h
display.o: display.c estruct.h edef.h efunc.h line.h utf8.h version.h \
 wrapper.h
eval.o: eval.c estruct.h edef.h efunc.h evar.h line.h utf8.h util.h \
 version.h
exec.o: exec.c estruct.h edef.h efunc.h line.h utf8.h
file.o: file.c estruct.h edef.h efunc.h line.h utf8.h
fileio.o: fileio.c estruct.h edef.h efunc.h
ibmpc.o: ibmpc.c estruct.h edef.h
input.o: input.c estruct.h edef.h efunc.h wrapper.h line.h utf8.h
isearch.o: isearch.c estruct.h edef.h efunc.h line.h utf8.h
line.o: line.c line.h utf8.h estruct.h edef.h efunc.h
lock.o: lock.c estruct.h edef.h efunc.h
main.o: main.c estruct.h edef.h efunc.h ebind.h line.h utf8.h version.h
pklock.o: pklock.c estruct.h edef.h efunc.h
posix.o: posix.c estruct.h edef.h efunc.h utf8.h
random.o: random.c estruct.h edef.h efunc.h line.h utf8.h charset.h
region.o: region.c estruct.h edef.h efunc.h line.h utf8.h
search.o: search.c estruct.h edef.h efunc.h line.h utf8.h
spawn.o: spawn.c estruct.h edef.h efunc.h
tcap.o: tcap.c estruct.h edef.h efunc.h
termio.o: termio.c
vmsvt.o: vmsvt.c estruct.h edef.h
vt52.o: vt52.c estruct.h edef.h
window.o: window.c estruct.h edef.h efunc.h line.h utf8.h wrapper.h
word.o: word.c estruct.h edef.h efunc.h line.h utf8.h
names.o: names.c estruct.h edef.h efunc.h line.h utf8.h
globals.o: globals.c estruct.h edef.h
version.o: version.c version.h
usage.o: usage.c usage.h
wrapper.o: wrapper.c usage.h
utf8.o: utf8.c utf8.h
complet.o: complet.c estruct.h edef.h efunc.h

# DEPENDENCIES MUST END AT END OF FILE
# IF YOU PUT STUFF HERE IT WILL GO AWAY
# see make depend above
