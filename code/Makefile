# makefile for emacs, updated Tue 15 May 10:24:46 BST 2018

PROGRAM=uemacs

SRC=basic.c bind.c buffer.c crypt.c display.c eval.c \
	exec.c file.c fileio.c globals.c idxsorter.c input.c \
	isearch.c line.c lock.c main.c names.c pklock.c posix.c random.c \
	region.c search.c spawn.c tcap.c usage.c utf8.c \
	version.c window.c word.c wrapper.c
OBJ=basic.o bind.o buffer.o crypt.o display.o eval.o \
	exec.o file.o fileio.o globals.o idxsorter.o input.o \
	isearch.o line.o lock.o main.o names.o pklock.o posix.o \
	random.o region.o search.o spawn.o tcap.o usage.o \
	utf8.o version.o window.o word.o wrapper.o
HDR=charset.h ebind.h edef.h efunc.h epath.h estruct.h evar.h \
	idxsorter.h line.h usage.h utf8.h util.h version.h

# DO NOT ADD OR MODIFY ANY LINES ABOVE THIS -- make source creates them

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

# A function to echo a filename if it exists
#
filex = $(shell test -f $(1) && echo $(1))

# Where is the utf8proc library?
# If no specific option is given we go looking for it.
#
# First we look in likely locations for a system installation, and
# use a dynamic libray from there.
# The fallback assumption is that it is installed alongside the nuemacs
# installation, and will will load the static library.
# If UTF8 is set to sys, we assume a system installation (and use a
# dynamic library)
# If UTF8 is set to syslocal, we assume a /usr/local installation
# with a dynamic library that needs RPATH set in the executable
# If UTF8 is set a anything else we assume it is the location of
# utf8proc lib *and* include files and load the static library
#
UTF8 =
ifeq ($(strip $(UTF8)),)
# No override - so go looking...
#
    ulist := /usr/include/utf8proc.h
    ulist += /usr/local/include/utf8proc.h
    ulist += /opt/local/include/utf8proc.h
    ulist += ../utf8proc*/utf8proc.h
    ulist += ../../utf8proc*/utf8proc.h
# Expand the wildcards...
    xulist = $(shell echo $(ulist))

    ures := $(firstword $(foreach arg, $(xulist), $(call filex, $(arg) ) ) )

    ifeq ($(strip $(ures)),/usr/include/utf8proc.h)
# System location - use dynamic lib with no special options
        UTF8INCL =
        UTF8LIB = -lutf8proc
    else ifeq ($(strip $(ures)),/usr/local/include/utf8proc.h)
        UTF8INCL = -I/usr/local/include
        UTF8LIB = -L/usr/local/lib -lutf8proc
    else ifeq ($(strip $(ures)),/opt/local/include/utf8proc.h)
        UTF8INCL = -I/opt/local/include
        UTF8LIB = -L/opt/local/lib -lutf8proc
    else ifeq ($(notdir $(strip $(ures))),utf8proc.h)
# Parallel local build - use static lib
#
        UTF8LOC = $(dir $(strip $(ures)))
        UTF8INCL = -I$(UTF8LOC)
        UTF8LIB = $(UTF8LOC)/libutf8proc.a
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

# If we haven't got a defined UTF8LIB then we haven't found one
# and need to exit, reporting that fact
#
ifeq ($(strip $(UTF8LIB)),)
    .DEFAULT_GOAL := utf8_error
endif

# Does the user want stacktraceing enabled?
# It's only available on 64-bit Intel (x86_64) as arm and mips
# don't produce any useful output.
# On x86_64 it is enabled if the include file is found, but you can
# manully disable it.
#
NOTRACE =
ifeq ($(strip $(NOTRACE)),)
    uname_M := $(shell sh -c 'uname -m 2>/dev/null || echo not')
    ifneq ($(uname_M), x86_64)
        NOTRACE = 1
    endif
endif

ifeq ($(strip $(NOTRACE)),)

# Where is the libbacktrace library?
# If no specific option is given we go looking for it.

# The default assumption is that it is installed alongside the nuemacs
# installation, and will will load the static library.
# If BCKT is set to sys, we assume a system installation (and use a
# dynamic library)
# If BCKT is set to syslocal, we assume a /usr/local installation
# with a dynamic library that needs RPATH set in the executable
# If UTF8 is set a anything else we assume it is the location of
# utf8proc and load the static library
#
    BCKT =
    ifeq ($(strip $(BCKT)),)
# No override - so go looking...
#
        blist := /usr/include/backtrace.h
        blist += /usr/local/include/backtrace.h
        blist += /opt/local/include/backtrace.h
        blist += ../libbacktrace*/backtrace.h
        blist += ../../libbacktrace*/backtrace.h
# Expand the wildcards...
        xblist = $(shell echo $(blist))

        bres := $(firstword $(foreach arg, $(xblist), $(call filex, $(arg))))
        ifeq ($(strip $(bres)),/usr/include/backtrace.h)
# System location - use dynamic lib with no special options
            BCKTINCL =
            BCKTLIB = -lbacktrace
        else ifeq ($(strip $(bres)),/usr/local/include/backtrace.h)
            BCKTINCL = -I/usr/local/include
            BCKTLIB = -L/usr/local/lib -lbacktrace
        else ifeq ($(strip $(bres)),/opt/local/include/backtrace.h)
            BCKTINCL = -I/opt/local/include
            BCKTLIB = -L/opt/local/lib -backtrace
        else ifeq ($(notdir $(strip $(bres))),backtrace.h)
# Parallel local build - use static lib
#
            BCKTLOC = $(dir $(strip $(bres)))
            BCKTINCL = -I$(BCKTLOC)
# If we found a libbacktrace-master we assume that the library is under
# .libs (where it is build) otherwise we assume it is in what we found.
#
            ifeq ($(lastword $(subst /, , $(BCKTLOC))), libbacktrace-master)
                BCKTLIB = $(BCKTLOC)/.libs/libbacktrace.a
            else
                BCKTLIB = $(BCKTLOC)/libbacktrace.a
            endif
    endif
# If the user gave a specifc BCKT option, use that
    else ifeq ($(strip $(BCKT)),sys)
        BCKTINCL =
        BCKTLIB  = -lbacktrace
    else ifeq ($(strip $(BCKT)),syslocal)
        BCKTINCL =
        BCKTLIB  = -lbacktrace
        RPATH    =  -Wl,-rpath=/usr/local/lib
    else
        BCKTINCL = -I$(BCKT)
        BCKTLIB  = $(BCKT)/libbcktproc.a
    endif

# If we haven't got a defined BCKTLIB then we haven't found one
# so don't enable it.
#
    ifneq ($(strip $(BCKTLIB)),)
# Now set the compilation options for it...
# -g1 is sufficient to get the line numbers (but not for variable debugging)
# We can still do -Ox optimizing.
        TRACE_OPTS = -g1 -DNUTRACE
    endif
endif

# Who are we?
#
uname_S := $(shell sh -c 'uname -s 2>/dev/null || echo not')

ifeq ($(uname_S),Linux)
 DEFINES=-DPOSIX -DUSG -D_DEFAULT_SOURCE -D_BSD_SOURCE -D_SVID_SOURCE -D_XOPEN_SOURCE=600 -D_GNU_SOURCE
else ifeq ($(uname_S),FreeBSD)
 DEFINES=-DPOSIX -DSYSV -D_FREEBSD_C_SOURCE -D_DEFAULT_SOURCE -D_BSD_SOURCE -D_SVID_SOURCE -D_XOPEN_SOURCE=600
else ifeq ($(uname_S),Darwin)
 DEFINES=-DPOSIX -DSYSV -D_DARWIN_C_SOURCE  -D_DEFAULT_SOURCE -D_BSD_SOURCE -D_SVID_SOURCE -D_XOPEN_SOURCE=600
# CygWin is just like Linux....
else ifeq ($(firstword $(subst -, ,$(uname_S))),CYGWIN_NT)
 DEFINES=-DPOSIX -DUSG -D_DEFAULT_SOURCE -D_BSD_SOURCE -D_SVID_SOURCE -D_XOPEN_SOURCE=600 -D_GNU_SOURCE
endif

# Are we building an ARM version for an Android 'phone?
#
ifeq ($(strip $(ANDROID)),moto8g)
    uname_M := $(shell sh -c 'uname -m 2>/dev/null || echo not')
    ifneq ($(uname_M),aarch64)
        .DEFAULT_GOAL := notarm64_error
    else
        STATIC=-static
        STATIC_XLIBS=-ltinfo
        STATIC_XDEFS=-DANDROID=moto8g
        PROGRAM=uemacs-moto8g
    endif
else ifeq ($(strip $(ANDROID)),moto4g)
    uname_M := $(shell sh -c 'uname -m 2>/dev/null || echo not')
    ifneq ($(uname_M),armv7l)
        .DEFAULT_GOAL := notarm_error
    else
        STATIC=-static
        STATIC_ARM=-L/usr/lib/arm-linux-gnueabihf
        STATIC_XLIBS=-ltinfo
        STATIC_XDEFS=-DANDROID=moto4g
        PROGRAM=uemacs-moto4g
    endif
endif

# Set the default options
#
CC = gcc $(STATIC)
WARNINGS = -Wall -Wstrict-prototypes -Wextra
CFLAGS = $(DBG_OPTS) $(TRACE_OPTS) $(WARNINGS) -std=gnu99 -DGGR_MODE \
    $(UTF8INCL) $(BCKTINCL) $(STATIC_XDEFS)

LIBS = -lcurses     # SYSV
LFLAGS = -hbx

#Let's try to find libcurses/libncurses etc.
# Start by finding term.h (for Oleg's router distros)
#
tlist := /opt/include/ncurses/term.h
tres := $(firstword $(foreach arg, $(tlist), $(call filex, $(arg) ) ) )
ifeq ($(strip $(tres)),/opt/include/ncurses/term.h)
   CFLAGS += -I/opt/include/ncurses
   LIBS= -lncurses
endif

# The Rules
#
$(PROGRAM): $(OBJ)
	$(E) "  LINK    " $@
	$(Q) $(CC) $(LDFLAGS) $(DEFINES) -o $@ $(OBJ) $(STATIC_ARM) \
           $(LIBS) $(STATIC_XLIBS) $(UTF8LIB) $(BCKTLIB) $(RPATH)

clean:
	$(E) "  CLEAN"
	$(Q) rm -f $(PROGRAM) core lintout makeout tags makefile.bak *.o
	$(Q) rm -f *.tfile uetest.rc

test-clean:
	$(Q) rm -f *.tfile uetest.rc

# These use makefile rather than Makefile so that you don't run them by
# mistake.
#
source:
	-@mv -f makefile makefile.bak
	@echo "# (GNU)Makefile for emacs, updated `date`" >makefile
	@echo '' >>makefile
	@echo PROGRAM=uemacs >>makefile
	@echo SRC=`ls *.c` >>makefile
	@echo OBJ=`ls *.c | sed s/c$$/o/` >>makefile
	@echo HDR=`ls *.h` >>makefile
	@echo '' >>makefile
	@echo '# DO NOT DELETE THIS LINE' >>makefile
	@echo '# make depend bits will be added after this' >>makefile
	@echo '# when make depensd is run' >>makefile
	@sed -n -e '/^# DO NOT ADD OR MODIFY/,$$p' <makefile.bak >>makefile

depend: ${SRC}
	@for i in ${SRC}; do\
	    cc ${DEFINES} -MM $$i ; done >makedep
	@echo '/^# DO NOT DELETE THIS LINE/+2,$$d' >eddep
	@echo '$$r ./makedep' >>eddep
	@echo 'w' >>eddep
	-@cp makefile makefile.bak
	@ed - makefile <eddep
	@rm eddep makedep
	@echo '' >>makefile
	@echo '# DEPENDENCIES MUST END AT END OF FILE' >>makefile
	@echo '# IF YOU PUT STUFF HERE IT WILL GO AWAY' >>makefile
	@echo '# see make depend above' >>makefile

.c.o:
	$(E) "  CC      " $@
	$(Q) ${CC} ${CFLAGS} ${DEFINES} -c $*.c

# Add BUILDER definition for version.c
#
uname_N := $(shell sh -c 'uname -n 2>/dev/null || echo unknown')
version.o: version.c
	$(E) "  CC      " $@
	$(Q) ${CC} ${CFLAGS} ${DEFINES} -DBUILDER=$(uname_N) -c $*.c

bt_error:
	@echo "ERROR: Unable to find libbacktrace"
	@echo "  This is a pre-requisite for TRACE!"
	@echo "  See https://github.com/ianlancetaylor/libbacktrace"


utf8_error:
	@echo "ERROR: Unable to find the Julia Lang utf8lib"
	@echo "       This is a pre-requisite!"
	@echo "  See https://julialang.org/utf8proc"

notarm_error:
	@echo "ERROR: You may only use the ANDROID=moto4g option on an ARM (armv7l) system"

notarm64_error:
	@echo "ERROR: You may only use the ANDROID=moto8g option on an ARM64 (aarch64) system"

# DO NOT DELETE THIS LINE -- make depend uses it

basic.o: basic.c estruct.h utf8.h edef.h efunc.h line.h
bind.o: bind.c estruct.h utf8.h edef.h efunc.h epath.h line.h util.h \
 idxsorter.h
buffer.o: buffer.c estruct.h utf8.h edef.h efunc.h line.h
crypt.o: crypt.c estruct.h utf8.h edef.h efunc.h
display.o: display.c estruct.h utf8.h edef.h efunc.h line.h version.h
eval.o: eval.c estruct.h utf8.h edef.h efunc.h evar.h line.h util.h \
 version.h idxsorter.h
exec.o: exec.c estruct.h utf8.h edef.h efunc.h line.h
file.o: file.c estruct.h utf8.h edef.h efunc.h line.h
fileio.o: fileio.c estruct.h utf8.h edef.h efunc.h line.h
globals.o: globals.c estruct.h utf8.h edef.h
idxsorter.o: idxsorter.c idxsorter.h
input.o: input.c estruct.h utf8.h edef.h efunc.h line.h
isearch.o: isearch.c estruct.h utf8.h edef.h efunc.h line.h
line.o: line.c line.h utf8.h estruct.h edef.h efunc.h
lock.o: lock.c estruct.h utf8.h edef.h efunc.h
main.o: main.c estruct.h utf8.h edef.h efunc.h ebind.h line.h version.h
names.o: names.c estruct.h utf8.h edef.h efunc.h line.h idxsorter.h
pklock.o: pklock.c estruct.h utf8.h edef.h efunc.h
posix.o: posix.c estruct.h utf8.h edef.h efunc.h
random.o: random.c estruct.h utf8.h edef.h efunc.h line.h charset.h
region.o: region.c estruct.h utf8.h edef.h efunc.h line.h
search.o: search.c estruct.h utf8.h edef.h efunc.h line.h
spawn.o: spawn.c estruct.h utf8.h edef.h efunc.h
tcap.o: tcap.c estruct.h utf8.h edef.h efunc.h
usage.o: usage.c usage.h
utf8.o: utf8.c estruct.h utf8.h edef.h efunc.h
version.o: version.c version.h
window.o: window.c estruct.h utf8.h edef.h efunc.h line.h
word.o: word.c estruct.h utf8.h edef.h efunc.h line.h
wrapper.o: wrapper.c usage.h

# DEPENDENCIES MUST END AT END OF FILE
# IF YOU PUT STUFF HERE IT WILL GO AWAY
# see make depend above
