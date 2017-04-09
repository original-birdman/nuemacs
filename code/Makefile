# (GNU)Makefile for uemacs

# Make the build silent by default
#
V =
ifeq ($(strip $(V)),)
    E = @echo
    Q = @
else
    E = @\#
    Q =
endif
export E Q

# No debug by default
#
DBG =
ifeq ($(strip $(DBG)),)
    DBG_OPTS = -O3
else
    DBG_OPTS = -O0 -g
endif

# Where is the utf8proc library?
# If no specifc option is given we go looking for it.

# The default assumption is that it is installed alongside the nuemacs
# installation, and will will load the static library.
# If UTF8 is set to sys, we assume a system installation (and use a
# dynamic library)
# If UTF8 is set to syslocal, we assume a /usr/local installation
# with a dynamic library that needs RPATH set in the executable
# If UTF8 is set a anything else we assume it is the location of
# utf8proc and load the static library
#
UTF8 =
ifeq ($(strip $(UTF8)),)
# No override - so go looking...
#
    dlist := /usr/include/utf8proc.h
    dlist += /usr/local/include/utf8proc.h
    dlist += /opt/local/include/utf8proc.h
    dlist += ../../utf8proc/utf8proc.h

    filex = $(shell test -f $(1) && echo $(1))
    res := $(firstword $(foreach dir, $(dlist), $(call filex, $(dir) ) ) )
    ifeq ($(strip $(res)),/usr/include/utf8proc.h)
# System location - use dynamic lib with no special options
        UTF8INCL =
        UTF8LIB = -lutf8proc
    else ifeq ($(strip $(res)),/usr/local/include/utf8proc.h)
        UTF8INCL = -I/usr/local/include
        UTF8LIB = -L/usr/local -lutf8proc
    else ifeq ($(strip $(res)),/opt/local/include/utf8proc.h)
        UTF8INCL = -I/opt/local/include
        UTF8LIB = -L/opt/local -lutf8proc
    else ifeq ($(strip $(res)),../../utf8proc/utf8proc.h)
# Parallel local build - use static lib
        UTF8INCL = -I../../utf8proc
        UTF8LIB = ../../utf8proc/libutf8proc.a
endif
# If the user gave a specifc UTF8 option, use that
else ifeq ($(strip $(UTF8)),sys)
    UTF8INCL =
    UTF8LIB  = -lutf8proc
else ifeq ($(strip $(UTF8)),syslocal)
    UTF8INCL =
    UTF8LIB  = -lutf8proc
    RPATH    =  -Wl,-rpath=/usr/local/lib
else
    UTF8INCL = -I$(UTF8)
    UTF8LIB  = $(UTF8)/libutf8proc.a
endif

# If we haven't got a defined UTF8LIB then we haven't foudn one
# and need to exit, reporting that fact
#
ifeq ($(strip $(UTF8LIB)),)
    .DEFAULT_GOAL := error
endif

# Who are we?
#
uname_S := $(shell sh -c 'uname -s 2>/dev/null || echo not')

ifeq ($(uname_S),Linux)
 DEFINES=-DAUTOCONF -DPOSIX -DUSG -D_DEFAULT_SOURCE -D_BSD_SOURCE -D_SVID_SOURCE -D_XOPEN_SOURCE=600
else ifeq ($(uname_S),FreeBSD)
 DEFINES=-DAUTOCONF -DPOSIX -DSYSV -D_FREEBSD_C_SOURCE -D_DEFAULT_SOURCE -D_BSD_SOURCE -D_SVID_SOURCE -D_XOPEN_SOURCE=600
else ifeq ($(uname_S),Darwin)
 DEFINES=-DAUTOCONF -DPOSIX -DSYSV -D_DARWIN_C_SOURCE  -D_DEFAULT_SOURCE -D_BSD_SOURCE -D_SVID_SOURCE -D_XOPEN_SOURCE=600
endif

PROGRAM=uemacs

SRC=ansi.c basic.c bind.c buffer.c crypt.c display.c eval.c exec.c \
	file.c fileio.c ibmpc.c input.c isearch.c line.c lock.c main.c \
	pklock.c posix.c random.c region.c search.c spawn.c tcap.c \
	termio.c vt52.c window.c word.c names.c globals.c version.c \
	usage.c wrapper.c utf8.c complet.c

OBJ=ansi.o basic.o bind.o buffer.o crypt.o display.o eval.o exec.o \
	file.o fileio.o ibmpc.o input.o isearch.o line.o lock.o main.o \
	pklock.o posix.o random.o region.o search.o spawn.o tcap.o \
	termio.o vt52.o window.o word.o names.o globals.o version.o \
	usage.o wrapper.o utf8.o complet.o

HDR=ebind.h edef.h efunc.h epath.h estruct.h evar.h util.h version.h

# DO NOT ADD OR MODIFY ANY LINES ABOVE THIS -- make source creates them

# Set the default options
#
CC = gcc
WARNINGS=-Wall -Wstrict-prototypes
CFLAGS=$(DBG_OPTS) $(WARNINGS) -std=gnu99 -DGGR_MODE $(UTF8INCL)
LIBS=$(UTF8LIB) -lcurses        # SYSV
LFLAGS=-hbx
BINDIR=/usr/bin
LIBDIR=/usr/lib

# The Rules
#
$(PROGRAM): $(OBJ)
	$(E) "  LINK    " $@
	$(Q) $(CC) $(LDFLAGS) $(DEFINES) -o $@ $(OBJ) $(LIBS) $(RPATH)

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

error:
	@echo "ERROR: Unable to find the Julia Lang utf8lib"
	@echo "       This is a pre-requisite!"

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
