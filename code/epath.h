/*      EPATH.H
 *
 *      This file contains certain info needed to locate the
 *      initialization (etc) files on a system dependent basis
 *
 *      modified by Petri Kutvonen
 */
#ifndef EPATH_H_
#define EPATH_H_

/*      possible names and paths of help files under different OSs
 * NOTE that there must be at least one!!
 */
#ifndef DFLT_PATH
#if     MSDOS
#define DFLT_PATH "\\sys\\public\\", "\\usr\\bin\\", "\\bin\\", "\\", ""
#endif

#if BSD | USG
#define DFLT_PATH "/opt/local/etc/", "/usr/local/etc/"
#endif

#endif

static struct init_files {
    char *startup;
    char *help;
} init_files = {"uemacs.rc", "uemacs.hlp"};

static char *pathname[] = { DFLT_PATH , NULL};

#endif  /* EPATH_H_ */
