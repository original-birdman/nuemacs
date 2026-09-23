/* ttdrivers.c
 *
 * The code to send relevant escpae sequence and set terminal characteristics.
 * Hard-wires the hamdling with ANSI escape sequences, as that handles
 * all "terminals" likely to be used, which are:
 *
 *  xterm
 *  KDE konsole
 *  gome-terminal
 *  gome-console
 *  kmscon
 *    The login console screens on
 *  linux
 *  sun-solaris
 *  freebsd
 */

#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <termios.h>
#include <unistd.h>

#define TTDRIVER_C

#include "estruct.h"
#include "edef.h"
#include "efunc.h"
#include "utf8.h"

/* Since Mac OS X's termios.h doesn't have the following 2 macros, define them.
 * Neither does FreeBSD.
 */
#if defined(_DARWIN_C_SOURCE) || defined(_FREEBSD_C_SOURCE)
#define OLCUC 0000002
#define XCASE 0000004
#endif

/* None of these is defined in CygWin, so set them to 0.
 * Note that these are not actually POSIX, and not implemented in Linux
 */
#ifdef __CYGWIN__
#define XCASE 0
#define ECHOPRT 0
#define PENDIN 0
#endif

static int kbdflgs;             /* saved keyboard fd flags      */
static int kbdpoll;             /* in O_NDELAY mode             */

static struct termios otermios; /* original terminal characteristics */
static struct termios ntermios; /* charactoristics to use inside */

/* We only let read get up to RBUFSIZE bytes in one call.
 * But we set the input buffer to twice this.
 */

#define RBUFSIZE 32
#define INBUFSIZE 2*RBUFSIZE
static char tibuf[INBUFSIZE];    /* terminal input buffer */

/* Write a character to the display.
 */
void TTputc(unicode_t uc) {
    char utf8[6];
    int bytes;

    bytes = unicode_to_utf8(uc, utf8);
    size_t dnc __attribute__ ((unused)) =
         fwrite(utf8, (size_t)bytes, 1, stdout);
}
static void TTputstr(const char *str) {
    size_t dnc __attribute__ ((unused)) =
         fwrite(str, strlen(str), 1, stdout);
}

/* Flush terminal buffer. Does real work where the terminal output is buffered
 * up. A no-operation on systems where byte at a time terminal I/O is done.
 */
void TTflush(void) {

/* Add some terminal output success checking, sometimes an orphaned
 * process may be left looping on SunOS 4.1.
 *
 * How to recover here, or is it best just to exit and lose
 * everything?
 *
 * jph, 8-Oct-1993
 * Jani Jaakkola suggested using select after EAGAIN but let's just wait a bit
 *
 */
    int status;
    int count = 60;     /* Arbitrary linit */

    do {
        status = fflush(stdout);
        if ((status < 0) && ((errno == EAGAIN) || (errno == EINTR))) {
            sleep(1);
            continue;
        }
        else break;
    } while (count-- > 0);
/* If we can't flush the termninal, there's no point in posting an
 * error message!
 */
    if (status != 0) quit(1, ENOTTY);  /* Any better exit code? */
}

/* Read a character from the terminal, performing no editing and doing no
 * echo at all.
 * We expect the characters to come in as utf8 strings (i.e. a byte at
 * a time) and we convert these to unicode (with some special CSI handling).
 * We expect any multi-byte character produced by a keyboard to dump
 * all bytes in one go, but we do allow for a small delay in them
 * arriving for processing into one unicode character.
 */
#include <poll.h>
static struct pollfd ue_wait = { STDIN_FILENO, POLLIN, 0 };

/* The valid count for this buffer, pending_rch, is a global.
 * We set the size to INBUFSIZE (64), but only tell read about
 * RBUFSIZE (32) of them. so we have space to push things every pending
 * read.
 */
static char ibuffer[INBUFSIZE];

static unicode_t ttgetc(void) {

    unicode_t c;
    int count, bytes, expected;

    errno = 0;
    count = pending_rch;    /* So we don't update pending_rch on error */
    if (!count) {       /* so count is 0 */
        count = (int)read(STDIN_FILENO, ibuffer, RBUFSIZE);
        if (count <= 0) return 0;   /* SIGWINCH and EINTR */
        pending_rch = count;
    }

/* If we get here then we have something.
 * BUT, if any system calls return -ve value we assume that SIGWINCH
 * has been seen and errno == EINTR. At that point we just return
 * 0. Our caller might then map that to EUM_NOCHAR by checking errno.
 */

    c = ibuffer[0];
    bytes = 1;
    if (c < 0xc0)   /* ASCII or Latin-1(??) */
        goto done;

/* Work out how many bytes we expect in total for this unicode char */

    if (c < 0xe0)      expected = 2;
    else if (c < 0xf0) expected = 3;
    else               expected = 4;

/* Unicode character  - try to fill buffer.
 * In practice all chras seem to arrive at once and so will have been
 * retrieved by the read() call above.
 * So this is just belt and braces.
 */
    while (pending_rch < expected) {
        int chars_waiting = poll(&ue_wait, 1, 100);
        if (chars_waiting < 0) return 0;    /* SIGWINCH and EINTR */
        if (chars_waiting == 0) break;      /* Timed out. Nothing arrived */
        int nr = (int)read(STDIN_FILENO, ibuffer + pending_rch,
            sizeof(ibuffer) - (size_t)pending_rch);
        if (nr <= 0) return 0;              /* SIGWINCH and EINTR */
        pending_rch += nr;
    }

/* Note that if we have an "incomplete" byte sequence (e.g. 3 when 4
 * was expected) this will just take and return the 1st byte.
 */
    bytes = utf8_to_unicode(ibuffer, 0, pending_rch, &c);

done:
    pending_rch -= bytes;

/* We need to shuffle down any still-pending bytes.
 * Will not be many - so just use simple loop.
 */
    if (pending_rch > 0) {
        char *fp = ibuffer+bytes;
        char *tp = ibuffer;
        count = pending_rch;    /* So we don't update pending_rch!!! */
        while(count--) *tp++ = *fp++;
    }
    return c;
}

/* Assumes there is space....
 * Since TTgetc only tells read() about half of the buffer.
 */
void TTungetc(unicode_t c) {
    char temp[8];
    int blen = unicode_to_utf8(c, temp);    /* How many bytes to add? */
    int mi = blen;
    while(mi--) {           /* Shuffle any current ones out of the way */
        ibuffer[pending_rch+1] = ibuffer[pending_rch];
        pending_rch++;
    }
    while(blen--) ibuffer[blen] = temp[blen];
    return;
}

/* typahead:    Check to see if any characters are already in the
 *                keyboard buffer
 */
int TTtypahead(void) {
    int x;      /* Total of known-waiting chars */

    x = (pending_rch > 0);
#ifdef FIONREAD
    if (x == 0)
        if (ioctl(0, FIONREAD, &x) < 0) x = 0;
#endif
    return x;
}

struct terminal term = {
/* "Constants" (== variables that are set)
 * The next eight values are set dynamically at open/resize time.
 */
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
};

/* Define the functions that will use ANSI Escape sequences */

#define ESC "\x1b"
#define CSI ESC "["

/* Standard ANSI used by xterm*, linux and sun-color */

static const char *CM =  CSI "%d;%dH";
static const char *CE =  CSI "K";
static const char *SO =  CSI "7m";
static const char *CS =  CSI "%d;%dr";
static const char *SF =  "\x0A";
static const char *SR =  ESC "M";
static const char *DL =  CSI "M";
static const char *AL =  CSI "L";

/* These may be reset once we know TERM */

static const char *CL = CSI "H" CSI "2J";
static const char *SE = CSI "27m";
static const char *TI = CSI "?1049h" CSI "22;0;0t"; /* Alt screen, ?? */
static const char *TE = CSI "?1049l" CSI "23;0;0t"; /* Main screen, ?? */

/* Functions which can be (re)defined dynamically */

void (*TTscroll)(int, int, int);
unicode_t (*TTgetc)(void) = ttgetc;

void TTmove(int row, int col) { /* row and col are 0-based */
    char obuf[32];  /* More than enough */

    sprintf(obuf, CM, row+1, col+1);
    TTputstr(obuf);
}

/* CS is set up just like CM */
static void scrollregion(int top, int bot) {    /* top and bot are 0-based */
    char obuf[32];  /* More than enough */

    sprintf(obuf, CS, top+1, bot+1);
    TTputstr(obuf);
}

/* Move howmanylines lines starting at from to to - if CS is dedined */
static void scroll_reg(int from, int to, int howmanylines) {
    int i;
    if (to == from) return;
    if (to < from) {
        scrollregion(to, from + howmanylines - 1);
        TTmove(from + howmanylines - 1, 0);
        for (i = from - to; i > 0; i--) TTputstr(SF);
    }
    else {  /* from < to */
        scrollregion(from, to + howmanylines - 1);
        TTmove(from, 0);
        for (i = to - from; i > 0; i--) TTputstr(SR);
    }
    scrollregion(0, term.t_mbline);
}

/* Move howmanylines lines starting at from to to - if CS is not defined */
static void scroll_delins(int from, int to, int howmanylines) {
    int i;
    if (to == from) return;
    if (to < from) {
        TTmove(to, 0);
        for (i = from - to; i > 0; i--) TTputstr(DL);
        TTmove(to + howmanylines, 0);
        for (i = from - to; i > 0; i--) TTputstr(AL);
    }
    else {
        TTmove(from + howmanylines, 0);
        for (i = to - from; i > 0; i--) TTputstr(DL);
        TTmove(from, 0);
        for (i = to - from; i > 0; i--) TTputstr(AL);
    }
}

/* The TERM value, and associated termcap entries, are not going to
 * change, so only get them once.
 */
char *termval = NULL;
void TTinit(void) {
    int int_col, int_row;

    if (!termval) {     /* We only do the term values setup once */
        if ((termval = getenv("TERM")) == NULL) {
            TTputstr("Environment variable TERM not defined!");
            exit(1);
        }

/* Set TERM-specific escape sequences and values */

        if (0 == strncmp(termval, "sun-", 4)) {
            TTscroll = scroll_delins;   /* can't scroll region */
            CL = "\x0C";
            SE = CSI "m";
            TI = "";                    /* No initialize */
            TE = "";                    /* No exit */
            fake_narrow = 1;
        }
        else if (0 == strncmp(termval, "linux", 4)) {
            TI = "";                    /* No initialize */
            TE = "";                    /* No exit */
        }
        else {
            TTscroll = scroll_reg;
        }
    }

/* Get screen size from system, or else from termcap.  */

    getscreensize(&int_col, &int_row);
    if ((int_col <= 0) || (int_row <= 0)) {
        TTputstr("Cannot determine screen size!");
        exit(1);
    }
    newscreensize(int_row, int_col, 1);
    term.t_ncol = int_col;
    SET_t_nrow(int_row);
    set_scrarray_size(term.t_nrow, term.t_ncol);
}

void TTopen(void) {

/* Set the terminal in/ouput as we want it */

    tcgetattr(0, &otermios);        /* save old settings */

/* Base new settings on old ones - don't change things we don't know about
 */
    ntermios = otermios;

/* Raw CR/NL etc input handling, but keep ISTRIP if we're on a 7-bit line */

    ntermios.c_iflag &= ~(tcflag_t)(IGNBRK|BRKINT|IGNPAR|PARMRK
                        |INPCK|INLCR|IGNCR|ICRNL|IXON);

/* Raw CR/NR etc output handling */

    ntermios.c_oflag &= ~(tcflag_t)(OPOST|ONLCR|OLCUC|OCRNL|ONOCR|ONLRET);

/* No signal handling, no echo etc */

    ntermios.c_lflag &= ~(tcflag_t)(ISIG|ICANON|XCASE|ECHO|ECHOE|ECHOK|
         ECHONL|NOFLSH|TOSTOP|ECHOCTL|ECHOPRT|ECHOKE|FLUSHO|PENDIN|IEXTEN);

/* One character, no timeout */

    ntermios.c_cc[VMIN] = 1;
    ntermios.c_cc[VTIME] = 0;
    tcsetattr(0, TCSADRAIN, &ntermios); /* and activate them */

/* Provide a smaller terminal input buffer so that the type-ahead
 * detection works better (more often)
 */
    setbuffer(stdin, tibuf, INBUFSIZE);

    kbdflgs = fcntl(0, F_GETFL, 0);
    kbdpoll = FALSE;

/* on all screens we are not sure of the initial position of the cursor */
    ttrow = -1;
    ttcol = -1;

/* Initialize the screen */

    TTputstr(TI);
    TTflush();
    ttrow = -1;
    ttcol = -1;
    sgarbf = TRUE;
}

/* This function gets called just before we go back home to the command
 * interpreter.
 */
void TTclose(void) {
    TTputstr(TE);
    TTmove(term.t_mbline, 0);
    TTflush();
    tcsetattr(0, TCSADRAIN, &otermios); /* restore terminal settings */
}

void TTeeol(void) {
    TTputstr(CE);
}

void TTeeop(void) {
    TTputstr(CL);
}

/* Change highlight status
 *
 * @state: FALSE = normal video, TRUE = highlight video.
 */
void TThilite(int state) {

    if ((db_len(hifcolor) == 0) && (db_len(hibcolor) == 0)) {
        TTputstr((state)? SO: SE);
    }
    else {

/* Code to set the highlight colour.
 * Sets both in the same escape sequence.
 */
        char obuf[64];
        if (state) {
            const char *sep;
            if ((db_len(hifcolor) > 0) && (db_len(hibcolor) > 0))
                sep = ";";
            else
                sep = "";
            snprintf(obuf, 64, ESC "[%s%s%sm",
                 db_val(hifcolor), sep, db_val(hibcolor));
        }
        else {
            const char *fg = (db_len(glfcolor) > 0)? db_val(glfcolor): "39";
            snprintf(obuf, 64, ESC "[%s;49m", fg);

        }
        TTputstr(obuf);
    }
}

/* Code to set the foreground colour
 * Should only be called when the colour is changed, or the screen state
 * is unknown.
 * If called with an empty colour string it unsets colours.
 */
void TTforg(int set) {
    char obuf[64];
    const char *esq;
/* Esc[39;49m can reset colors for linux/xterm*, but Esc[0m works
 * on all tested systems.
 */
    if (!set || (db_len(glfcolor) == 0)) esq = "0";
    else esq = db_val(glfcolor);
    snprintf(obuf, 64, CSI "%sm", esq);
    TTputstr(obuf);
    if (set) {  /* Don't do this if we're not actually setting. */
        sgarbf = TRUE;
    }
}

#define BEL     0x07
void TTbeep(void) {
        TTputc(BEL);
}
