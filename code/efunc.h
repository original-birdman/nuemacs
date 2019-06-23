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
extern int wrapword(int f, int n);
extern int backword(int f, int n);
extern int forwword(int f, int n);
extern int upperword(int f, int n);
extern int lowerword(int f, int n);
extern int capword(int f, int n);
extern int delfword(int f, int n);
extern int delbword(int f, int n);
extern int inword(void);
extern int fillpara(int f, int n);
extern int justpara(int f, int n);
extern int killpara(int f, int n);
extern int wordcount(int f, int n);
extern int makelist_region(int, int);
extern int numberlist_region(int, int);

extern int fillwhole(int, int);
extern int eos_chars(int f, int n);
void ensure_case(int);

/* window.c */
extern int reposition(int f, int n);
extern int redraw(int f, int n);
extern int nextwind(int f, int n);
extern int prevwind(int f, int n);
extern int mvdnwind(int f, int n);
extern int mvupwind(int f, int n);
extern int onlywind(int f, int n);
extern int delwind(int f, int n);
extern int splitwind(int f, int n);
extern int enlargewind(int f, int n);
extern int shrinkwind(int f, int n);
extern int resize(int f, int n);
extern int scrnextup(int f, int n);
extern int scrnextdw(int f, int n);
extern int savewnd(int f, int n);
extern int restwnd(int f, int n);
extern int newsize(int f, int n);
extern int newwidth(int f, int n);
extern int getwpos(void);
extern void cknewwindow(void);
extern struct window *wpopup(void);  /* Pop up window creation. */


/* basic.c */
extern int gotobol(int f, int n);
extern int back_grapheme(int);
extern int backchar(int, int);
extern int gotoeol(int f, int n);
extern int forw_grapheme(int);
extern int forwchar(int, int);
extern int gotoline(int f, int n);
extern int gotobob(int f, int n);
extern int gotoeob(int f, int n);
extern int forwline(int f, int n);
extern int backline(int f, int n);
extern int gotobop(int f, int n);
extern int gotoeop(int f, int n);
extern int forwpage(int f, int n);
extern int backpage(int f, int n);
extern int setmark(int f, int n);
extern int swapmark(int f, int n);

/* random.c */
extern int setfillcol(int f, int n);
extern int showcpos(int f, int n);
extern int getcline(void);
extern int getccol(int bflg);
extern int setccol(int pos);
extern int twiddle(int f, int n);
extern int quote(int f, int n);
extern int typetab(int f, int n);
extern int insert_tab(int f, int n);
extern int detab(int f, int n);
extern int entab(int f, int n);
extern int trim(int f, int n);
extern int openline(int f, int n);
extern int insert_newline(int f, int n);
extern int insbrace(int n, int c);
extern int inspound(void);
extern int deblank(int f, int n);
extern int indent(int f, int n);
extern int forwdel(int f, int n);
extern int backdel(int f, int n);
extern int killtext(int f, int n);
extern int setemode(int f, int n);
extern int delmode(int f, int n);
extern int setgmode(int f, int n);
extern int delgmode(int f, int n);
extern int clrmes(int f, int n);
extern int writemsg(int f, int n);
extern int getfence(int f, int n);
extern int fmatch(int ch);
extern int istring(int f, int n);
extern int rawstring(int f, int n);
extern int ovstring(int f, int n);
extern int leaveone(int f, int n);
extern int whitedelete(int f, int n);
extern int quotedcount(int f, int n);
extern int ggr_style (int f, int n);

/* main.c */
extern com_arg *multiplier_check(int);
extern int addto_kbdmacro(char *, int, int);
extern void addchar_kbdmacro(char);
extern int macro_helper(int, int);
extern void dumpdir_tidy(void);
extern void edinit(char *bname);
extern int execute(int c, int f, int n);
extern int quickexit(int f, int n);
extern int quit(int f, int n);
extern int ctlxlp(int f, int n);
extern int ctlxrp(int f, int n);
extern int ctlxe(int f, int n);
extern int ctrlg(int f, int n);
extern int rdonly(void);
extern int resterr(void);
extern int not_in_mb_error(int, int);
extern int nullproc(int f, int n);
extern int metafn(int f, int n);
extern int cex(int f, int n);
extern int unarg(int f, int n);
extern int cexit(int status);
extern int reexecute(int, int);
extern void extend_keytab(int);

/* display.c */
extern void vtinit(void);
extern void vtfree(void);
extern void vttidy(void);
extern void vtmove(int row, int col);
extern int upscreen(int f, int n);
extern int update(int force);
extern void updpos(void);
extern void upddex(void);
extern int updupd(int force);
extern void upmode(void);
extern void movecursor(int row, int col);
extern void mlerase(void);
extern void mlwrite(const char *fmt, ...);
extern void mlforce(const char *fmt, ...);
extern void mlputs(char *s);
extern void getscreensize(int *widthp, int *heightp);
extern void sizesignal(int signr);
extern int newscreensize(int, int, int);

extern  int ttput1c(char);
extern void mberase(void);
extern void mbupdate(void);
extern void movecursor(int, int);

/* region.c */
extern int killregion(int f, int n);
extern int copyregion(int f, int n);
extern int lowerregion(int f, int n);
extern int upperregion(int f, int n);
extern int getregion(struct region *rp);

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
extern int mlyesno(char *prompt);
extern int mlreply(char *prompt, char *buf, int nbuf);
extern int mlreplyt(char *prompt, char *buf, int nbuf, int eolchar);
extern int mlreplyall(char *, char *, int);
extern int ectoc(int c);
extern int ctoec(int c);
extern struct name_bind *getname(char *);
extern int tgetc(void);
extern int get1key(void);
extern int getcmd(void);
extern int getstring(char *prompt, char *buf, int nbuf, int eolchar);

/* bind.c */
extern int help(int f, int n);
extern int deskey(int f, int n);
extern struct key_tab *getbind(int);
extern int bindtokey(int f, int n);
extern int unbindkey(int f, int n);
extern int desbind(int f, int n);
extern int apro(int f, int n);
extern int startup(char *sfname);
extern void set_pathname(char *);
#define ONPATH 1
#define INTABLE 2
extern char *flook(char *, int, int);
extern void cmdstr(int c, char *seq);
extern char *transbind(char *skey);
extern int buffertokey(int, int);

/* buffer.c */
extern int usebuffer(int f, int n);
extern int nextbuffer(int f, int n);
extern int swbuffer(struct buffer *, int);
extern int killbuffer(int f, int n);
extern int zotbuf(struct buffer *bp);
extern int namebuffer(int f, int n);
extern int listbuffers(int f, int n);
extern int anycb(void);
extern int bclear(struct buffer *bp);
extern int unmark(int f, int n);
/* Lookup a buffer by name. */
extern struct buffer *bfind(const char *bname, int cflag, int bflag);

/* file.c */
extern int fileread(int f, int n);
extern int insfile(int f, int n);
extern int filefind(int f, int n);
extern int viewfile(int f, int n);
extern int getfile(char *fname, int lockfl);
extern int readin(char *fname, int lockfl);
extern void makename(char *bname, char *fname);
extern void unqname(char *name);
extern int filewrite(int f, int n);
extern int filesave(int f, int n);
extern int writeout(char *fn);
extern int filename(int f, int n);

/* fileio.c */
extern int ffropen(char *fn);
extern int ffwopen(char *fn);
extern int ffclose(void);
extern int ffputline(char *buf, int nbuf);
extern int ffgetline(void);
extern int fexist(char *fname);
extern void fixup_fname(char *);

/* exec.c */
extern int namedcmd(int f, int n);
extern int execcmd(int f, int n);
extern char *token(char *src, char *tok, int size);
extern int macarg(char *tok);
extern int nextarg(char *prompt, char *buffer, int size, int terminator);
extern int storemac(int f, int n);
extern void ptt_free(struct buffer *);
extern int storepttable(int, int);
extern int set_pttable(int, int);
extern int next_pttable(int, int);
extern int toggle_ptmode(int, int);
extern int ptt_handler(int);
extern int storeproc(int f, int n);
extern int execproc(int f, int n);
extern int execbuf(int f, int n);
extern int dobuf(struct buffer *bp);
extern int execfile(int f, int n);
extern int dofile(char *fname);
extern int cbuf(int f, int n, int bufnum);
extern int cbuf1(int f, int n);
extern int cbuf2(int f, int n);
extern int cbuf3(int f, int n);
extern int cbuf4(int f, int n);
extern int cbuf5(int f, int n);
extern int cbuf6(int f, int n);
extern int cbuf7(int f, int n);
extern int cbuf8(int f, int n);
extern int cbuf9(int f, int n);
extern int cbuf10(int f, int n);
extern int cbuf11(int f, int n);
extern int cbuf12(int f, int n);
extern int cbuf13(int f, int n);
extern int cbuf14(int f, int n);
extern int cbuf15(int f, int n);
extern int cbuf16(int f, int n);
extern int cbuf17(int f, int n);
extern int cbuf18(int f, int n);
extern int cbuf19(int f, int n);
extern int cbuf20(int f, int n);
extern int cbuf21(int f, int n);
extern int cbuf22(int f, int n);
extern int cbuf23(int f, int n);
extern int cbuf24(int f, int n);
extern int cbuf25(int f, int n);
extern int cbuf26(int f, int n);
extern int cbuf27(int f, int n);
extern int cbuf28(int f, int n);
extern int cbuf29(int f, int n);
extern int cbuf30(int f, int n);
extern int cbuf31(int f, int n);
extern int cbuf32(int f, int n);
extern int cbuf33(int f, int n);
extern int cbuf34(int f, int n);
extern int cbuf35(int f, int n);
extern int cbuf36(int f, int n);
extern int cbuf37(int f, int n);
extern int cbuf38(int f, int n);
extern int cbuf39(int f, int n);
extern int cbuf40(int f, int n);

/* spawn.c */
extern int spawncli(int f, int n);
extern int bktoshell(int f, int n);
extern void rtfrmshell(void);
extern int spawn(int f, int n);
extern int execprg(int f, int n);
extern int pipecmd(int f, int n);
extern int filter_buffer(int f, int n);

/* search.c */

/* next_sstr, prev_sstr and select_sstr must *not* be made bindable
 * in names.c!!!
 * They are remapped from nextwind/prevwind when in the search mini-buffer
 */
extern void init_search_ringbuffers(void);
extern void new_prompt(char *);
extern int next_sstr(int, int);
extern int prev_sstr(int, int);
extern int select_sstr(int, int);

extern int forwsearch(int f, int n);
extern int forwhunt(int f, int n);
extern int backsearch(int f, int n);
extern int backhunt(int f, int n);
extern int scanmore(char *, int);
extern void setpattern(const char[], const char[]);
extern int eq(unsigned char bc, unsigned char pc);
extern void rvstrcpy(char *rvstr, char *str);
extern int sreplace(int f, int n);
extern int qreplace(int f, int n);
extern int expandp(char *srcstr, char *deststr, int maxlength);
extern int boundry(struct line *curline, int curoff, int dir);
extern void mcclear(void);

/* isearch.c */
extern int risearch(int f, int n);
extern int fisearch(int f, int n);
extern int isearch(int f, int n);

/* eval.c */
extern void varinit(void);
extern int stol(char *);
extern int setvar(int, int);
extern char *ue_itoa(int);
extern int gettyp(char *);
extern char *getval(char *);

/* crypt.c */
extern int set_encryption_key(int f, int n);
extern void myencrypt(char *bptr, unsigned len);

/* lock.c */
extern int lockchk(char *fname);
extern int lockrel(void);
extern int lock(char *fname);

/* pklock.c */
extern char *dolock(char *fname);
extern char *undolock(char *fname);

/* GGR */
/* complet.c */
extern int comp_file(char *, char *);
extern int comp_buffer(char *, char *);

/* names.c */
extern void init_namelookup(void);
extern struct name_bind *func_info(fn_t);
extern struct name_bind *name_info(char *);
extern int nxti_name_info(int);

/* wrapper.c */

extern void *Xmalloc(size_t);
extern void *Xrealloc(void *, size_t);

#endif
