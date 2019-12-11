#ifndef VERSION_H_
#define VERSION_H_

#define PROGRAM_NAME "uemacs"
#ifdef ANDROID
#define PROGRAM_NAME_LONG "nuEmacs-android"
#else
#define PROGRAM_NAME_LONG "nuEmacs"
#endif
#define VERSION "GGR4.145"

/* Print the version string. */
void version(void);

#endif  /* VERSION_H_ */
