/*      LOCK.C
 *
 *      File locking command routines
 *
 *      written by Daniel Lawrence
 */

#include "estruct.h"

#define LOCK_C

#include <stdio.h>
#include <string.h>
#include "edef.h"
#include "efunc.h"

#include <errno.h>

static char *lname[NLOCKS];     /* names of all locked files */
static int numlocks;            /* # of current locks active */

/* report a lock error
 *
 * char *errstr;        lock error string to print out
 * Now a #define
 */
#define lckerror(errstr) (mlwrite("%s - %s", errstr, strerror(errno)))

/* unlock:
 *      Unlock a file
 *      this only warns the user if it fails
 *
 * char *fname;         file to unlock
 */
static int unlock(char *fname) {
    const char *locker; /* undolock return string */

/* Unlock and return */

    locker = undolock(fname);
    if (locker == NULL) return TRUE;

/* Report the error and come back */

    lckerror(locker);
    return FALSE;
}

/* lock:
 *      Check and lock a file from access by others
 *      returns TRUE = files was not locked and now is
 *              FALSE = file was locked and overridden
 *              ABORT = file was locked, abort command
 *
 * char *fname;         file name to lock
 */
int lock(const char *fname) {
    const char *locker; /* lock error message */
    int status;         /* return status      */

/* Attempt to lock the file */

    locker = dolock(fname);
    if (locker == NULL) /* We win */
        return TRUE;

/* File failed...abort */

    if (strncmp(locker, "LOCK", 4) == 0) {
        lckerror(locker);
        return ABORT;
    }

/* Someone else has it....override? */

    db_set(glb_db, "File in use by ");
    db_append(glb_db, locker);
    db_append(glb_db, ", override?");
    status = mlyesno(db_val(glb_db));   /* Ask them */
    if (status == TRUE) return FALSE;
    else                return ABORT;
}

/* lockchk:
 *      check a file for locking and add it to the list
 *
 * NOTE!
 *  There is currently no mechanism to remove a file from this list
 *  if a buffer is closed!
 *
 * char *fname;                 file to check for a lock
 */
int lockchk(const char *fname) {
    int i;          /* loop indexes */
    int status;     /* return status */
/* GGR
 * Need a copy of the filename, to allow possible tilde and shell
 * expansion on the passed-in name.
 * Remember to free it in errors!
 */
    char *tmpname = Xstrdup(fixup_fname(fname));

/* Check to see whether that file is already locked here */

    for (i = 0; i < numlocks; ++i)
        if (strcmp(tmpname, lname[i]) == 0) {
            status = TRUE;
            goto exit;
        }

/* If we have a full locking table, bitch and leave */

    if (numlocks == NLOCKS) {
        mlwrite_one("LOCK ERROR: Lock table full");
        status = ABORT;
    }
    else {          /* Next, try to lock it */
        status = lock(tmpname);
        if (status) {
/* Everything is cool, add *just the name* to the table.
 * Then exit - do not free!
 */
            lname[++numlocks - 1] = tmpname;
            return TRUE;
        }
        if (status == FALSE)    /* locked, overriden, dont add to table */
            status = TRUE;
    }
exit:
    Xfree(tmpname);         /* Didn't add to table, so free it */
    return status;
}

/* lockrel:
 *      release all the file locks so others may edit
 */
int lockrel(void) {
    int i;          /* loop index */
    int status;     /* status of locks */
    int s;          /* status of one unlock */

    status = TRUE;
    if (numlocks > 0)
        for (i = 0; i < numlocks; ++i) {
            if ((s = unlock(lname[i])) != TRUE) status = s;
            Xfree(lname[i]);
        }
    numlocks = 0;
    return status;
}

#ifdef DO_FREE
/* Add a call to allow free() of normally-unfreed items here for, e.g,
 * valgrind usage.
 */
void free_lock(void) {
    for (int i = 0; i < numlocks; ++i) Xfree(lname[i]);
    return;
}
#endif
