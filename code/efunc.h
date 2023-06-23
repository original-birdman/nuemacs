/*      efunc.h
 *
 *      Function declarations and names.
 *
 *      This file list all the C code functions used and the names to use
 *      to bind keys to them. To add functions, declare it here in both the
 *      extern function list and the name binding table.
 *
 *      modified by Petri Kutvonen
 */
#ifndef EFUNC_H_
#define EFUNC_H_

/* External function declarations. */

/* word.c */
#define inword_classes "LN"
#define at_wspace_classes "ZC"
#define inword(wp) class_check(wp, inword_classes, FALSE)
#define at_abreak(wp) class_check(wp, at_wspace_classes, TRUE)
#define at_wspace(wp) class_check(wp, at_wspace_classes, FALSE)
extern int class_check(struct inwbuf *, char *, int);

extern int backword(int, int);
extern int forwword(int, int);
extern void ensure_case(int);
extern int upperword(int, int);
extern int lowerword(int, int);
extern int capword(int, int);
extern int delfword(int, int);
extern int delbword(int, int);

extern int killpara(int, int);
extern int wordcount(int, int);

extern int wrapword(int, int);
extern int eos_chars(int, int);
extern int fillpara(int, int);
extern int justpara(int, int);
extern int fillwhole(int, int);
extern int makelist_region(int, int);
extern int numberlist_region(int, int);


/* window.c */
extern int reposition(int, int);
extern int redraw(int, int);
extern int nextwind(int, int);
extern int prevwind(int, int);
extern int mvdnwind(int, int);
extern int mvupwind(int, int);
extern int onlywind(int, int);
extern int delwind(int, int);
extern int splitwind(int, int);
extern int enlargewind(int, int);
extern int shrinkwind(int, int);
extern int resize(int, int);
extern int scrnextup(int, int);
extern int scrnextdw(int, int);
extern int savewnd(int, int);
extern int restwnd(int, int);
extern int newsize(int);
extern int newwidth(int);
extern int getwpos(void);
extern void cknewwindow(void);
extern struct window *wpopup(void);  /* Pop up window creation. */


/* basic.c */
extern int gotobol(int, int);
extern int back_grapheme(int);
extern int backchar(int, int);
extern int gotoeol(int, int);
extern int forw_grapheme(int);
extern int forwchar(int, int);
extern int gotoline(int, int);
extern int gotobob(int, int);
extern int gotoeob(int, int);
extern int forwline(int, int);
extern int backline(int, int);
extern int gotobop(int, int);
extern int gotoeop(int, int);
extern int forwpage(int, int);
extern int backpage(int, int);
extern int setmark(int, int);
extern int swapmark(int, int);

/* random.c */
extern int setfillcol(int, int);
extern int showcpos(int, int);
extern int getcline(void);
extern int getccol(void);
extern int setccol(int pos);
extern int twiddle(int, int);
extern int quote(int, int);
extern int typetab(int, int);
extern int insert_tab(int, int);
extern int detab(int, int);
extern int entab(int, int);
extern int trim(int, int);
extern int openline(int, int);
extern int insert_newline(int, int);
extern int insbrace(int, int);
extern int inspound(void);
extern int deblank(int, int);
extern int indent(int, int);
extern int forwdel(int, int);
extern int backdel(int, int);
extern int killtext(int, int);
extern int setemode(int, int);
extern int delmode(int, int);
extern int setgmode(int, int);
extern int delgmode(int, int);
extern int clrmes(int, int);
extern int writemsg(int, int);
extern int getfence(int, int);
extern int fmatch(int);
extern int itokens(int, int);
extern int istring(int, int);
extern int ovstring(int, int);
extern int leaveone(int, int);
extern int whitedelete(int, int);
extern int quotedcount(int, int);
extern int ggr_style(int, int);
extern int re_args_exec(int, int);

/* main.c */
extern com_arg *multiplier_check(int);
extern int addto_kbdmacro(char *, int, int);
extern void addchar_kbdmacro(char);
extern int macro_helper(int, int);
extern void dumpdir_tidy(void);
extern void edinit(char *);
extern int execute(int c, int, int);
extern int quickexit(int, int);
extern int quit(int, int);
extern int ctlxlp(int, int);
extern int ctlxrp(int, int);
extern int ctlxe(int, int);
extern int ctrlg(int, int);
extern int rdonly(void);
extern int resterr(void);
extern int not_in_mb_error(int, int);
extern int not_interactive(int, int);
extern int nullproc(int, int);
extern int metafn(int, int);
extern int cex(int, int);
extern int unarg(int, int);
extern int cexit(int status);
extern int reexecute(int, int);
extern void extend_keytab(int);

/* display.c */
extern void vtinit(void);
extern void vtfree(void);
extern void vttidy(void);
extern void vtmove(int, int);
extern int upscreen(int, int);
extern int update(int);
extern void updpos(void);
extern void upddex(void);
extern int updupd(int);
extern void upmode(void);
extern void movecursor(int, int);
extern void force_movecursor(int, int);
extern void mlerase(void);
extern void mlwrite(const char *, ...);
extern void mlforce(const char *, ...);
extern void mlwrite_one(const char *);
extern void mlforce_one(const char *);
extern void mlputs(char *);
extern void getscreensize(int *, int *);
extern void sizesignal(int);
extern void set_scrarray_size(int, int);
extern int newscreensize(int, int, int);

extern int ttput1c(char);
extern void mberase(void);
extern void mbupdate(void);
extern void movecursor(int, int);

/* region.c */
extern int killregion(int, int);
extern int copyregion(int, int);
extern int lowerregion(int, int);
extern int upperregion(int, int);
extern int getregion(struct region *);

extern int narrow(int, int);
extern int widen(int, int);

/* posix.c */
extern void ttopen(void);
extern void ttclose(void);
extern int ttputc(int c);
extern void ttflush(void);
extern int ttgetc(void);
extern int typahead(void);

/* input.c */
extern int mlyesno(char *);
extern int mlreply(char *, char *, int, enum cmplt_type);
extern struct name_bind *getname(char *, int);
extern int tgetc(void);
extern int get1key(void);
extern int getcmd(void);
extern int getstring(char *, char *, int , enum cmplt_type);

/* bind.c */
extern int help(int, int);
extern int deskey(int, int);
extern struct key_tab *getbyfnc(fn_t);
extern struct key_tab *getbind(int);
extern int buffertokey(int, int);
extern int switch_internal(int, int);
extern int bindtokey(int, int);
extern int unbindkey(int, int);
extern int desbind(int, int);
extern int apro(int, int);
extern int startup(char *);
extern void set_pathname(char *);
#define ONPATH 1
#define INTABLE 2
extern char *flook(char *, int, int);
extern void cmdstr(int, char *);
extern char *transbind(char *);

/* buffer.c */
extern int usebuffer(int, int);
extern int nextbuffer(int, int);
extern int swbuffer(struct buffer *, int);
extern int killbuffer(int, int);
extern int zotbuf(struct buffer *);
extern int namebuffer(int, int);
extern int listbuffers(int, int);
extern int anycb(void);
extern int bclear(struct buffer *);
extern int unmark(int, int);
/* Lookup a buffer by name. */
extern struct buffer *bfind(const char *, int, int);
extern char do_force_mode(char *);
extern int setforcemode(int, int);

/* file.c */
extern int fileread(int, int);
extern int insfile(int, int);
extern int filefind(int, int);
extern int viewfile(int, int);
extern int showdir_handled(char *);
extern int getfile(char *, int, int);
extern int readin(char *, int);
extern void makename(char *, char *, int);
extern int filewrite(int, int);
extern int filesave(int, int);
extern int writeout(char *);
extern int filename(int, int);

/* fileio.c */
extern int ffropen(char *);
extern int ffwopen(char *);
extern int ffclose(void);
extern int ffputline(char *, int);
extern int ffgetline(void);
extern int fexist(char *);
extern void fixup_fname(char *);
extern void fixup_full(char *);

/* exec.c */
extern int namedcmd(int, int);
extern int execcmd(int, int);
extern char *token(char *, char *, int);
extern int macarg(char *);
extern int nextarg(char *, char *, int, enum cmplt_type);
extern int storemac(int, int);
extern void ptt_free(struct buffer *);
extern int storepttable(int, int);
extern int set_pttable(int, int);
extern int next_pttable(int, int);
extern int toggle_ptmode(int, int);
extern int ptt_handler(int);
extern int storeproc(int, int);
extern int run_user_proc(char *, int, int);
extern int execproc(int, int);
extern int execbuf(int, int);
extern int dobuf(struct buffer *);
extern int execfile(int, int);
extern int dofile(char *);
extern int cbuf(int, int, int bufnum);
extern int cbuf1(int, int);
extern int cbuf2(int, int);
extern int cbuf3(int, int);
extern int cbuf4(int, int);
extern int cbuf5(int, int);
extern int cbuf6(int, int);
extern int cbuf7(int, int);
extern int cbuf8(int, int);
extern int cbuf9(int, int);
extern int cbuf10(int, int);
extern int cbuf11(int, int);
extern int cbuf12(int, int);
extern int cbuf13(int, int);
extern int cbuf14(int, int);
extern int cbuf15(int, int);
extern int cbuf16(int, int);
extern int cbuf17(int, int);
extern int cbuf18(int, int);
extern int cbuf19(int, int);
extern int cbuf20(int, int);
extern int cbuf21(int, int);
extern int cbuf22(int, int);
extern int cbuf23(int, int);
extern int cbuf24(int, int);
extern int cbuf25(int, int);
extern int cbuf26(int, int);
extern int cbuf27(int, int);
extern int cbuf28(int, int);
extern int cbuf29(int, int);
extern int cbuf30(int, int);
extern int cbuf31(int, int);
extern int cbuf32(int, int);
extern int cbuf33(int, int);
extern int cbuf34(int, int);
extern int cbuf35(int, int);
extern int cbuf36(int, int);
extern int cbuf37(int, int);
extern int cbuf38(int, int);
extern int cbuf39(int, int);
extern int cbuf40(int, int);

/* spawn.c */
extern int spawncli(int, int);
extern int bktoshell(int, int);
extern void rtfrmshell(void);
extern int spawn(int, int);
extern int execprg(int, int);
extern int pipecmd(int, int);
extern int filter_buffer(int, int);

/* search.c */

extern void init_search_ringbuffers(void);
extern void new_prompt(char *);
extern void rotate_sstr(int);
extern void select_sstr(void);

extern int forwsearch(int, int);
extern int forwhunt(int, int);
extern int backsearch(int, int);
extern int backhunt(int, int);
extern int scanmore(char *, int, int, int);
extern void setpattern(const char[], const char[]);
extern int unicode_eq(unsigned int, unsigned);
extern int asceq(unsigned char, unsigned char);
extern void rvstrcpy(char *, char *);
extern int sreplace(int, int);
extern int qreplace(int, int);
extern int expandp(const char *, char *, int);
extern int boundry(struct line *, int, int);
extern char *group_match(int);

/* isearch.c */
extern int incremental_debug_check(int);
extern void incremental_debug_cleanup(void);
extern int risearch(int, int);
extern int fisearch(int, int);

/* eval.c */
extern void varinit(void);
extern void init_envvar_index(void);
extern int nxti_envvar(int);
extern void sort_user_var(void);
extern int nxti_usrvar(int);
extern int stol(char *);
extern int setvar(int, int);
extern int delvar(int, int);
extern char *ue_itoa(int);
extern int gettyp(char *);
extern char *getval(char *);

/* crypt.c */
extern int set_encryption_key(int, int);
extern void myencrypt(char *, unsigned int);

/* lock.c */
extern int lockchk(char *);
extern int lockrel(void);
extern int lock(char *);

/* pklock.c */
extern char *dolock(char *);
extern char *undolock(char *);

/* names.c */
extern void init_namelookup(void);
extern struct name_bind *func_info(fn_t);
extern struct name_bind *name_info(char *);
extern int nxti_name_info(int);

/* wrapper.c */

extern void *Xfree(void *);

#if __GCC__ >= 11
#define MALLOC_ATTR __attribute__ ((malloc, malloc (Xfree, 1), returns_nonnull))
#elif __GCC__ >= 4
#define MALLOC_ATTR __attribute__ ((malloc, returns_nonnull))
#else
#define MALLOC_ATTR __attribute__ ((malloc))
#endif
extern void *Xmalloc(size_t) MALLOC_ATTR;
#undef MALLOC_ATTR

#if __GCC__ > 4
#define REALLOC_ATTR __attribute__ (returns_nonnull))
#else
#define REALLOC_ATTR
#endif
extern void *Xrealloc(void *, size_t) REALLOC_ATTR;
#undef REALLOC_ATTR

#if __GCC__ >= 11
#define STRDUP_ATTR __attribute__((malloc (Xfree), returns_nonnull))
#elif __GCC__ > 4
#define STRDUP_ATTR __attribute__((malloc, returns_nonnull))
#else
#define STRDUP_ATTR __attribute__((malloc))
#endif
extern char *Xstrdup (const char *) STRDUP_ATTR;
#undef STRDUP_ATTR

#ifdef DO_FREE
/* Calls to allow free()s when compiing in debug mode */
extern void free_bind(void);
extern void free_buffer(void);
extern void free_display(void);
extern void free_eval(void);
extern void free_exec(void);
extern void free_input(void);
extern void free_line(void);
extern void free_names(void);
extern void free_search(void);
extern void free_utf8(void);

#if FILOCK && (BSD | SVR4)
extern void free_lock(void);
#endif

#endif

#endif
