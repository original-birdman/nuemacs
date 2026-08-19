/*      posix.c
 *
 *      The functions in this file negotiate with the operating system for
 *      characters, and write characters in a barely buffered fashion on the
 *      display. All operating systems.
 *
 *      modified by Petri Kutvonen
 *
 *      based on termio.c, with all the old cruft removed, and
 *      fixed for termios rather than the old termio.. Linus Torvalds
 */

#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <termios.h>
#include <unistd.h>

#define POSIX_C

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

#define TBUFSIZ 128
static char tobuf[TBUFSIZ];     /* terminal output buffer */


/* This function is called once to set up the terminal device streams.
 */
void ttopen(void) {
    tcgetattr(0, &otermios);        /* save old settings */

/* Base new settings on old ones - don't change things we don't know about
 */
    ntermios = otermios;

/* Raw CR/NL etc input handling, but keep ISTRIP if we're on a 7-bit line */
#if XONXOFF
    ntermios.c_iflag &= ~(tcflag_t)(IGNBRK | BRKINT | IGNPAR | PARMRK
                         | INPCK | INLCR | IGNCR | ICRNL);
#else
    ntermios.c_iflag &= ~(tcflag_t)(IGNBRK | BRKINT | IGNPAR | PARMRK
                         | INPCK | INLCR | IGNCR | ICRNL | IXON);
#endif

/* Raw CR/NR etc output handling */
    ntermios.c_oflag &= ~(tcflag_t)(OPOST | ONLCR | OLCUC | OCRNL | ONOCR | ONLRET);

/* No signal handling, no echo etc */

    ntermios.c_lflag &= ~(tcflag_t)(ISIG | ICANON | XCASE | ECHO | ECHOE | ECHOK
                         | ECHONL | NOFLSH | TOSTOP | ECHOCTL |
                           ECHOPRT | ECHOKE | FLUSHO | PENDIN | IEXTEN);

/* One character, no timeout */
    ntermios.c_cc[VMIN] = 1;
    ntermios.c_cc[VTIME] = 0;
    tcsetattr(0, TCSADRAIN, &ntermios); /* and activate them */

/* Provide a smaller terminal output buffer so that the type-ahead
 * detection works better (more often)
 */
    setbuffer(stdout, tobuf, TBUFSIZ);

    kbdflgs = fcntl(0, F_GETFL, 0);
    kbdpoll = FALSE;

/* on all screens we are not sure of the initial position of the cursor */
    ttrow = -1;
    ttcol = -1;
}

/* This function gets called just before we go back home to the command
 * interpreter.
 */
void ttclose(void) {
    tcsetattr(0, TCSADRAIN, &otermios); /* restore terminal settings */
}

/* Write a character to the display.
 */
int ttputc(int c) {
    char utf8[6];
    int bytes;

    bytes = unicode_to_utf8(c, utf8);
    fwrite(utf8, 1, (size_t)bytes, stdout);
    return 0;
}

/* Flush terminal buffer. Does real work where the terminal output is buffered
 * up. A no-operation on systems where byte at a time terminal I/O is done.
 */
void ttflush(void) {

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
        if ((status < 0) && (errno == EAGAIN)) {
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

static char buffer[32];
static int pending = 0;

enum pfx_seen { NOPFX, ESCPFX, CTLXPFX };
static enum pfx_seen pfx_state = NOPFX;

int ttgetc(void) {

    unicode_t c;
    int count, bytes, expected;

    errno = 0;
    count = pending;    /* So we don't update pending on error */
    if (!count) {       /* so count is 0 */
        count = (int)read(STDIN_FILENO, buffer, sizeof(buffer));
        if (count <= 0) return 0;   /* SIGWINCH and EINTR */
        pending = count;
    }

/* If we get here then we have something.
 * BUT, if any system calls return -ve value we assume that SIGWINCH
 * has been seen and errno == EINTR. At that point we just return
 * 0. Our caller might then map that to EUM_NOCHAR by checking errno.
 */

    c = buffer[0];
    bytes = 1;

/* Display CtlX or Esc if there are no other pending chars?
 * NOTE that the state of GGR_MLPFX cannot change between a Ctlx and Esc
 * being typed consecutively.
 */
    if ((pending <= 1) && (ggr_opts & GGR_MLPFX)) {
        switch (c) {
        case 0x18:          /* Ctlx */
            switch(pfx_state) {
            case NOPFX:
                mlwrite_one("Ctlx");
                pfx_state = CTLXPFX;
                break;
            default:        /* No other state */
                pfx_state = NOPFX;
            }
            break;
        case 0x1b:          /* Esc */
            switch(pfx_state) {
            case NOPFX:
                mlwrite_one("Esc");
                pfx_state = ESCPFX;
                break;
            case CTLXPFX:   /* No state beyond this */
                mlwrite_one("CtlxEsc");
                /* Falls through */
            default:        /* No other state */
                pfx_state = NOPFX;
            }
            break;
        default:
            pfx_state = NOPFX;
        }
    }

    if (c < 0xc0)   /* ASCII or Latin-1(??) */
        goto done;

/* Work out how many bytes we expect in total for this unicode char */

    if (c < 0xe0)      expected = 2;    /* Caters for 0x1b as well */
    else if (c < 0xf0) expected = 3;
    else               expected = 4;

/* Unicode character  - try to fill buffer.
 * In practice all chras seem to arrive at once and so will have been
 * retrieved by the read() call above.
 * So this is just belt and braces.
 */
    while (pending < expected) {
        int chars_waiting = poll(&ue_wait, 1, 100);
        if (chars_waiting < 0) return 0;    /* SIGWINCH and EINTR */
        if (chars_waiting == 0) break;      /* Timed out. Nothing arrived */
        int nr = (int)read(STDIN_FILENO, buffer + pending,
            sizeof(buffer) - (size_t)pending);
        if (nr <= 0) return 0;              /* SIGWINCH and EINTR */
        pending += nr;
    }

/* Note that if we have an "incomplete" byte sequence (e.g. 3 when 4
 * was expected) this will just take and return the 1st byte.
 */
    bytes = utf8_to_unicode(buffer, 0, pending, &c);

done:
    pending -= bytes;

/* We need to shuffle down any still-pending bytes.
 * Will not be many - so just use simple loop.
 */
    if (pending > 0) {
        char *fp = buffer+bytes;
        char *tp = buffer;
        count = pending;    /* So we don't update pending!!! */
        while(count--) *tp++ = *fp++;
    }
    return c;
}

/* typahead:    Check to see if any characters are already in the
 *                keyboard buffer
 */

int typahead(void) {
    int x;                  /* holds # of pending chars */

    x = (pending > 0);
#ifdef FIONREAD
    if (x == 0)
        if (ioctl(0, FIONREAD, &x) < 0) x = 0;
#endif
    return x;
}
