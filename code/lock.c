/*      LOCK.C
 *
 *      File locking command routines
 *
 *      written by Daniel Lawrence
 */

#include "estruct.h"

#if FILOCK

#include <stdio.h>
#include <string.h>
#include "edef.h"
#include "efunc.h"

#include <errno.h>

static char *lname[NLOCKS];             /* names of all locked files */
static int numlocks;                    /* # of current locks active */

/*
 * lockchk:
 *      check a file for locking and add it to the list
 *
 * NOTE!
 *  There is currently no mechanism to remove a file from this list
 *  if a buffer is closed!
 *
 * char *fname;                 file to check for a lock
 */
int lockchk(char *fname) {
    int i;          /* loop indexes */
    int status;     /* return status */
/* GGR
 * Need a copy of the filename, to allow possible tilde and shell
 * expansion on the passed-in name.
 * Remember to free it in errors!
 */
    char *tmpname = (char *)Xmalloc(NFILEN);
    strcpy(tmpname, fname);
    fixup_fname(tmpname);

/* Check to see whether that file is already locked here */

    for (i = 0; i < numlocks; ++i)
        if (strcmp(tmpname, lname[i]) == 0) return TRUE;

/* If we have a full locking table, bitch and leave */

    if (numlocks == NLOCKS) {
        mlwrite_one("LOCK ERROR: Lock table full");
        status = ABORT;
    }
    else {          /* Next, try to lock it */
        status = lock(tmpname);
        if (status) {
/* Everything is cool, add it to the table */

            lname[++numlocks - 1] = tmpname;
            return TRUE;
        }
        if (status == FALSE)    /* locked, overriden, dont add to table */
            status = TRUE;
    }
    Xfree(tmpname);             /* Didn't add to table, so free it */
    return status;
}

/*
 * report a lock error
 *
 * char *errstr;        lock error string to print out
 */
static void lckerror(char *errstr) {
    char obuf[NSTRING];     /* output buffer for error message */

    strcpy(obuf, errstr);
    strcat(obuf, " - ");
    strcat(obuf, strerror(errno));
    mlwrite_one(obuf);
}

/*
 * unlock:
 *      Unlock a file
 *      this only warns the user if it fails
 *
 * char *fname;         file to unlock
 */
static int unlock(char *fname) {
    char *locker;   /* undolock return string */

/* Unlock and return */

    locker = undolock(fname);
    if (locker == NULL) return TRUE;

/* Report the error and come back */

    lckerror(locker);
    return FALSE;
}

/*
 * lockrel:
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

/*
 * lock:
 *      Check and lock a file from access by others
 *      returns TRUE = files was not locked and now is
 *              FALSE = file was locked and overridden
 *              ABORT = file was locked, abort command
 *
 * char *fname;         file name to lock
 */
int lock(char *fname) {
    char *locker;       /* lock error message */
    int status;         /* return status      */
    char msg[NSTRING];  /* message string     */

/* Attempt to lock the file */

    locker = dolock(fname);
    if (locker == NULL)     /* we win */
        return TRUE;

/* File failed...abort */

    if (strncmp(locker, "LOCK", 4) == 0) {
        lckerror(locker);
        return ABORT;
    }

/* Someone else has it....override? */

    strcpy(msg, "File in use by ");
    strcat(msg, locker);
    strcat(msg, ", override?");
    status = mlyesno(msg);  /* ask them */
    if (status == TRUE) return FALSE;
    else                return ABORT;
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

#endif
