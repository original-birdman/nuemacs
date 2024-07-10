#ifndef VERSION_H_
#define VERSION_H_

/* Stringify macros... */
#define xstr(s) str(s)
#define str(s) #s

#define PROGRAM_NAME "uemacs"

#define PROGRAM_NAME_LONG "nuEmacs"

#define VERSION "GGR4.182"

/* Print the version string. */
void version(void);

#endif  /* VERSION_H_ */
