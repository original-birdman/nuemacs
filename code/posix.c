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

#ifdef POSIX

#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <termios.h>
#include <unistd.h>

#include "estruct.h"
#include "edef.h"
#include "efunc.h"
#include "utf8.h"

/* Since Mac OS X's termios.h doesn't have the following 2 macros, define them.
 */
#if defined(SYSV) && (defined(_DARWIN_C_SOURCE) || defined(_FREEBSD_C_SOURCE))
#define OLCUC 0000002
#define XCASE 0000004
#endif

/* None of these is defiend in CygWin, so set them to 0.
 * Note that these are not actually POSIX, and not implemenetd in Linux
 */
#ifdef __CYGWIN__
#define XCASE 0
#define ECHOPRT 0
#define PENDIN 0
#endif

static int kbdflgs;                     /* saved keyboard fd flags      */
static int kbdpoll;                     /* in O_NDELAY mode             */

static struct termios otermios;         /* original terminal characteristics */
static struct termios ntermios;         /* charactoristics to use inside */

#define TBUFSIZ 128
static char tobuf[TBUFSIZ];             /* terminal output buffer */


/*
 * This function is called once to set up the terminal device streams.
 * On VMS, it translates TT until it finds the terminal, then assigns
 * a channel to it and sets it raw. On CPM it is a no-op.
 */
void ttopen(void)
{
        tcgetattr(0, &otermios);        /* save old settings */

        /*
         * base new settings on old ones - don't change things
         * we don't know about
         */
        ntermios = otermios;

        /* raw CR/NL etc input handling, but keep ISTRIP if we're on a 7-bit line */
#if XONXOFF
        ntermios.c_iflag &= ~(IGNBRK | BRKINT | IGNPAR | PARMRK
                              | INPCK | INLCR | IGNCR | ICRNL);
#else
        ntermios.c_iflag &= ~(IGNBRK | BRKINT | IGNPAR | PARMRK
                              | INPCK | INLCR | IGNCR | ICRNL | IXON);
#endif

        /* raw CR/NR etc output handling */
        ntermios.c_oflag &=
            ~(OPOST | ONLCR | OLCUC | OCRNL | ONOCR | ONLRET);

        /* No signal handling, no echo etc */

        ntermios.c_lflag &= ~(ISIG | ICANON | XCASE | ECHO | ECHOE | ECHOK
                              | ECHONL | NOFLSH | TOSTOP | ECHOCTL |
                              ECHOPRT | ECHOKE | FLUSHO | PENDIN | IEXTEN);

        /* one character, no timeout */
        ntermios.c_cc[VMIN] = 1;
        ntermios.c_cc[VTIME] = 0;
        tcsetattr(0, TCSADRAIN, &ntermios);     /* and activate them */

        /*
         * provide a smaller terminal output buffer so that
         * the type ahead detection works better (more often)
         */
        setbuffer(stdout, &tobuf[0], TBUFSIZ);

        kbdflgs = fcntl(0, F_GETFL, 0);
        kbdpoll = FALSE;

        /* on all screens we are not sure of the initial position
           of the cursor                                        */
        ttrow = 999;
        ttcol = 999;
}

/*
 * This function gets called just before we go back home to the command
 * interpreter. On VMS it puts the terminal back in a reasonable state.
 * Another no-operation on CPM.
 */
void ttclose(void)
{
        tcsetattr(0, TCSADRAIN, &otermios); /* restore terminal settings */
}

/*
 * Write a character to the display. On VMS, terminal output is buffered, and
 * we just put the characters in the big array, after checking for overflow.
 * On CPM terminal I/O unbuffered, so we just write the byte out. Ditto on
 * MS-DOS (use the very very raw console output routine).
 */
int ttputc(int c)
{
        char utf8[6];
        int bytes;

        bytes = unicode_to_utf8(c, utf8);
        fwrite(utf8, 1, bytes, stdout);
        return 0;
}

/*
 * Flush terminal buffer. Does real work where the terminal output is buffered
 * up. A no-operation on systems where byte at a time terminal I/O is done.
 */
void ttflush(void)
{
/*
 * Add some terminal output success checking, sometimes an orphaned
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

        status = fflush(stdout);
        while (status < 0 && errno == EAGAIN) {
                sleep(1);
                status = fflush(stdout);
        }
        if (status < 0)
                exit(15);
}

/*
 * Read a character from the terminal, performing no editing and doing no
 * echo at all.
 * We expect the characters to come in as utf8 strings (i.e. a byte at
 * a time) and we convert these to unicode (with soem special CSI handling).
 * We expect any multi-byte character produced by a keyboard to dump
 * all bytes in one go, but we do allow for a small delay in them
 * arriving for processign into one unicode character.
 */
#include <poll.h>
static struct pollfd ue_wait = { 0, POLLIN, 0 };

int ttgetc(void)
{
    static char buffer[32];
    static int pending;
    unicode_t c;
    int count, bytes = 1, expected;

    count = pending;
    if (!count) {
        count = read(0, buffer, sizeof(buffer));
        if (count <= 0) return 0;
        pending = count;
    }

    c = ch_as_uc(buffer[0]);
    if (c < 0xc0 && !(c == 0x1b))   /* ASCII or Latin-1(??) - Not Esc */
        goto done;

/* Work out how many bytes we expect in total for this unicode char */
    if (c == 0x1b)     expected = 2;
    else if (c < 0xe0) expected = 2;
    else if (c < 0xf0) expected = 3;
    else               expected = 4;

/* Special character - try to fill buffer */
    while (pending < expected) {
        int chars_waiting = poll(&ue_wait, 1, 100);
        if (chars_waiting <= 0) break;
        pending += read(0, buffer + count, sizeof(buffer) - count);
    }
    if (pending > 1) {
        char second = buffer[1];
        if (c == 0x1b && second == '[') { /* Turn ESC+'[' into CSI */
            bytes = 2;
            c = 0x9b;
            goto done;
        }
    }
    bytes = utf8_to_unicode(buffer, 0, pending, &c);

done:
    pending -= bytes;
    memmove(buffer, buffer+bytes, pending);
    return c;
}

/* typahead:    Check to see if any characters are already in the
                keyboard buffer
*/

int typahead(void)
{
        int x;                  /* holds # of pending chars */

#ifdef FIONREAD
        if (ioctl(0, FIONREAD, &x) < 0)
                x = 0;
#else
        x = 0;
#endif
        return x;
}

#endif                          /* POSIX */
