#ifndef VERSION_H_
#define VERSION_H_

#define PROGRAM_NAME "uemacs"
#ifdef ANDROID

/* Stringify macros... */
#define xstr(s) str(s)
#define str(s) #s

#define PROGRAM_NAME_LONG "nuEmacs-" xstr(ANDROID)
#else
#define PROGRAM_NAME_LONG "nuEmacs"
#endif
#define VERSION "GGR4.165"

/* Print the version string. */
void version(void);

#endif  /* VERSION_H_ */
