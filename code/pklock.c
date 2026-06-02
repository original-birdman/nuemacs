/*      PKLOCK.C
 *
 *      locking routines as modified by Petri Kutvonen
 */

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stdio.h>
#include <fcntl.h>
#include <pwd.h>
#include <string.h>
#include <errno.h>

#define PKLOCK_C

#include "estruct.h"
#include "edef.h"
#include "efunc.h"


#define MAXLOCK 512
#define MAXNAME 128

/**********************
 *
 * if successful, returns NULL
 * if file locked, returns username of person locking the file
 * if other error, returns "LOCK ERROR: explanation"
 *
 *********************/
const char *dolock(const char *fname) {
    int fd, n;
    static char lname[MAXLOCK], locker[MAXNAME + 1];
    int mask;
    struct stat sbuf;

    if (snprintf(lname, sizeof(lname), "%s.lock~", fname)
        >= (int)sizeof(lname))
        return "LOCK ERROR: filename too long";

/* Open the lock file without following symlinks. This makes the
 * "check that it is a regular file" race-free: any pre-existing
 * symlink at this path causes open() to fail with ELOOP, and the
 * fstat() below then verifies that what we did open is a regular
 * file. There is no longer a window between an lstat() and the
 * open() during which an attacker could swap the entry.
 */

/* Lock file is readable by everyone (so other users can see who holds
 * the lock) but writable only by the owner. umask(0) is set so a
 * restrictive user umask doesn't strip the group/other read bits.
 */
    mask = umask(0);
    int oflags = O_RDWR | O_CREAT;
#ifdef O_NOFOLLOW
    oflags |= O_NOFOLLOW;
#endif
    fd = open(lname, oflags, 0644);
    umask(mask);
    if (fd < 0) {
        if (errno == EACCES) return NULL;
#ifdef EROFS
        if (errno == EROFS) return NULL;
#endif
#ifdef ELOOP
        if (errno == ELOOP) return "LOCK ERROR: lock path is a symlink";
#endif
        return "LOCK ERROR: cannot access lock file";
    }
/* Re-check the file type via the fd we actually opened. */
    if (fstat(fd, &sbuf) != 0) {
        close(fd);
        return "LOCK ERROR: cannot stat lock file";
    }
#if defined(S_ISREG)
    if (!S_ISREG(sbuf.st_mode))
#else
    if (!(((sbuf.st_mode) & 070000) == 0))  /* SysV R2 */
#endif
    {
        close(fd);
        return "LOCK ERROR: not a regular file";
    }
    if ((n = read(fd, locker, MAXNAME)) < 1) {
        lseek(fd, 0, SEEK_SET);
/* cuserid() is deprecated - its Linux man pages says, "Do not use cuserid()."
                cuserid(locker);
 */
#ifndef STANDALONE
        struct passwd *pwe = getpwuid(geteuid());
        const char *user = (pwe && pwe->pw_name) ? pwe->pw_name : "?";
#else
        const char *user = "You";
#endif
        char hostname[64];
        if (gethostname(hostname, sizeof(hostname)) != 0)
            hostname[0] = '\0';
/* gethostname() may not NUL-terminate on truncation. */
        hostname[sizeof(hostname) - 1] = '\0';
        snprintf(locker, sizeof(locker), "%s@%s", user, hostname);
        int dnc __attribute__ ((unused)) = write(fd, locker, strlen(locker));
        close(fd);
        return NULL;
    }
    locker[n > MAXNAME ? MAXNAME : n] = 0;
    return locker;
}


/*********************
 *
 * undolock -- unlock the file fname
 *
 * if successful, returns NULL
 * if other error, returns "LOCK ERROR: explanation"
 *
 *********************/

const char *undolock(const char *fname) {
    static char lname[MAXLOCK];

    if (snprintf(lname, sizeof(lname), "%s.lock~", fname)
        >= (int)sizeof(lname))
        return "LOCK ERROR: filename too long";
    if (unlink(lname) != 0) {
        if (errno == EACCES || errno == ENOENT) return NULL;
#ifdef EROFS
        if (errno == EROFS) return NULL;
#endif
        return "LOCK ERROR: cannot remove lock file";
    }
    return NULL;
}
