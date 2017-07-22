/* Name to function binding table.
 *
 * This table gives the names of all the bindable functions
 * end their C function address. These are used for the bind-to-key
 * function.
*/

#include "estruct.h"
#include "edef.h"
#include "efunc.h"
#include "line.h"

struct name_bind names[] = {
        {"abort-command", ctrlg, {0, 0}},
        {"add-mode", setemode, {0, 0}},
        {"add-global-mode", setgmode, {0, 0}},
#if     APROP
        {"apropos", apro, {0, 1}},
#endif
        {"backward-character", backchar, {0, 0}},
        {"begin-macro", ctlxlp, {0, 0}},
        {"beginning-of-file", gotobob, {0, 0}},
        {"beginning-of-line", gotobol, {0, 0}},
        {"bind-to-key", bindtokey, {0, 0}},
        {"buffer-position", showcpos, {0, 0}},
        {"buffer-to-key", buffertokey, {0, 0}},     /* GGR */
        {"case-region-lower", lowerregion, {0, 0}},
        {"case-region-upper", upperregion, {0, 0}},
        {"case-word-capitalize", capword, {0, 0}},
        {"case-word-lower", lowerword, {0, 0}},
        {"case-word-upper", upperword, {0, 0}},
        {"change-file-name", filename, {0, 0}},
        {"change-screen-size", newsize, {0, 0}},
        {"change-screen-width", newwidth, {0, 0}},
        {"char-replace", char_replace, {0, 0}},     /* GGR */
        {"clear-and-redraw", redraw, {0, 0}},
        {"clear-message-line", clrmes, {0, 0}},
        {"copy-region", copyregion, {0, 0}},
#if     WORDPRO
        {"count-words", wordcount, {0, 0}},
#endif
        {"ctlx-prefix", cex, {0, 0}},
        {"delete-blank-lines", deblank, {0, 0}},
        {"delete-buffer", killbuffer, {0, 0}},
        {"delete-mode", delmode, {0, 0}},
        {"delete-global-mode", delgmode, {0, 0}},
        {"delete-next-character", forwdel, {0, 0}},
        {"delete-next-word", delfword, {0, 0}},
        {"delete-other-windows", onlywind, {0, 1}},
        {"delete-previous-character", backdel, {0, 0}},
        {"delete-previous-word", delbword, {0, 0}},
        {"delete-window", delwind, {0, 1}},
        {"describe-bindings", desbind, {0, 1}},
        {"describe-key", deskey, {0, 0}},
#if     AEDIT
        {"detab-line", detab, {0, 0}},
#endif
        {"end-macro", ctlxrp, {0, 0}},
        {"end-of-file", gotoeob, {0, 0}},
        {"end-of-line", gotoeol, {0, 0}},
        {"eos-chars", eos_chars, {0, 0}},           /* GGR */
#if     AEDIT
        {"entab-line", entab, {0, 0}},
#endif
        {"exchange-point-and-mark", swapmark, {0, 0}},
        {"execute-buffer", execbuf, {0, 0}},
        {"execute-command-line", execcmd, {0, 0}},
        {"execute-file", execfile, {0, 0}},
        {"execute-macro", ctlxe, {1, 0}},
        {"execute-macro-1", cbuf1, {0, 0}},
        {"execute-macro-2", cbuf2, {0, 0}},
        {"execute-macro-3", cbuf3, {0, 0}},
        {"execute-macro-4", cbuf4, {0, 0}},
        {"execute-macro-5", cbuf5, {0, 0}},
        {"execute-macro-6", cbuf6, {0, 0}},
        {"execute-macro-7", cbuf7, {0, 0}},
        {"execute-macro-8", cbuf8, {0, 0}},
        {"execute-macro-9", cbuf9, {0, 0}},
        {"execute-macro-10", cbuf10, {0, 0}},
        {"execute-macro-11", cbuf11, {0, 0}},
        {"execute-macro-12", cbuf12, {0, 0}},
        {"execute-macro-13", cbuf13, {0, 0}},
        {"execute-macro-14", cbuf14, {0, 0}},
        {"execute-macro-15", cbuf15, {0, 0}},
        {"execute-macro-16", cbuf16, {0, 0}},
        {"execute-macro-17", cbuf17, {0, 0}},
        {"execute-macro-18", cbuf18, {0, 0}},
        {"execute-macro-19", cbuf19, {0, 0}},
        {"execute-macro-20", cbuf20, {0, 0}},
        {"execute-macro-21", cbuf21, {0, 0}},
        {"execute-macro-22", cbuf22, {0, 0}},
        {"execute-macro-23", cbuf23, {0, 0}},
        {"execute-macro-24", cbuf24, {0, 0}},
        {"execute-macro-25", cbuf25, {0, 0}},
        {"execute-macro-26", cbuf26, {0, 0}},
        {"execute-macro-27", cbuf27, {0, 0}},
        {"execute-macro-28", cbuf28, {0, 0}},
        {"execute-macro-29", cbuf29, {0, 0}},
        {"execute-macro-30", cbuf30, {0, 0}},
        {"execute-macro-31", cbuf31, {0, 0}},
        {"execute-macro-32", cbuf32, {0, 0}},
        {"execute-macro-33", cbuf33, {0, 0}},
        {"execute-macro-34", cbuf34, {0, 0}},
        {"execute-macro-35", cbuf35, {0, 0}},
        {"execute-macro-36", cbuf36, {0, 0}},
        {"execute-macro-37", cbuf37, {0, 0}},
        {"execute-macro-38", cbuf38, {0, 0}},
        {"execute-macro-39", cbuf39, {0, 0}},
        {"execute-macro-40", cbuf40, {0, 0}},
        {"execute-named-command", namedcmd, {1, 0}},
#if     PROC
        {"execute-procedure", execproc, {0, 1}},
#endif
        {"execute-program", execprg, {0, 1}},
        {"exit-emacs", quit, {0, 1}},
#if     WORDPRO
        {"fill-paragraph", fillpara, {0, 0}},
        {"fill-all-paragraphs", fillwhole, {0, 0}}, /* GGR */
#endif
        {"filter-buffer", filter_buffer, {0, 0}},
        {"find-file", filefind, {0, 0}},
        {"forward-character", forwchar, {0, 0}},
        {"ggr-style", ggr_style, {0, 0}},           /* GGR */
        {"goto-line", gotoline, {0, 0}},
#if     CFENCE
        {"goto-matching-fence", getfence, {0, 0}},
#endif
        {"grow-window", enlargewind, {0, 1}},
        {"handle-tab", insert_tab, {0, 0}},
        {"hunt-forward", forwhunt, {0, 0}},
        {"hunt-backward", backhunt, {0, 0}},
        {"help", help, {0, 1}},
        {"i-shell", spawncli, {0, 1}},
#if     ISRCH
        {"incremental-search", fisearch, {0, 1}},
#endif
        {"insert-file", insfile, {0, 0}},
        {"insert-space", insspace, {0, 0}},
        {"insert-string", istring, {0, 0}},
        {"insert-raw-string", rawstring, {0, 0}},
#if     WORDPRO
        {"justify-paragraph", justpara, {0, 0}},
        {"kill-paragraph", killpara, {0, 0}},
#endif
        {"kill-region", killregion, {0, 0}},
        {"kill-to-end-of-line", killtext, {0, 0}},
        {"leave-one-white", leaveone, {0, 0}},      /* GGR */
        {"list-buffers", listbuffers, {0, 1}},
        {"macro-helper", macro_helper, {0, 1}},     /* GGR */
#if     WORDPRO
        {"makelist-region", makelist_region, {0, 0}},       /* GGR */
        {"numberlist-region", numberlist_region, {0, 0}},   /* GGR */
#endif
        {"meta-prefix", metafn, {0, 0}},
        {"move-window-down", mvdnwind, {0, 0}},
        {"move-window-up", mvupwind, {0, 0}},
        {"name-buffer", namebuffer, {0, 0}},
        {"narrow-to-region", narrow, {0, 1}},       /* GGR */
        {"newline", insert_newline, {0, 0}},
        {"newline-and-indent", indent, {0, 0}},
        {"next-buffer", nextbuffer, {0, 0}},
        {"next-line", forwline, {0, 0}},
        {"next-page", forwpage, {0, 0}},
#if     WORDPRO
        {"next-paragraph", gotoeop, {0, 0}},
#endif
#if PROC
        {"next-pttable", next_pttable, {0, 0}},     /* GGR */
#endif
        {"next-window", nextwind, {0, 1}},
        {"next-word", forwword, {0, 0}},
        {"nop", nullproc, {1, 0}},
        {"open-line", openline, {0, 0}},
        {"overwrite-string", ovstring, {0, 0}},
        {"pipe-command", pipecmd, {0, 1}},
        {"previous-line", backline, {0, 0}},
        {"previous-page", backpage, {0, 0}},
#if     WORDPRO
        {"previous-paragraph", gotobop, {0, 0}},
#endif
        {"previous-window", prevwind, {0, 1}},
        {"previous-word", backword, {0, 0}},
        {"query-replace-string", qreplace, {0, 1}},
        {"quick-exit", quickexit, {0, 1}},
        {"quote-character", quote, {1, 0}},
        {"quoted-count", quotedcount, {0, 0}},      /* GGR */
        {"read-file", fileread, {0, 0}},
        {"redraw-display", reposition, {0, 0}},
        {"reexecute", reexecute, {0, 0}},           /* GGR */
        {"resize-window", resize, {0, 1}},
        {"restore-window", restwnd, {0, 0}},
        {"replace-string", sreplace, {0, 0}},
#if     ISRCH
        {"reverse-incremental-search", risearch, {0, 1}},
#endif
#if     PROC
        {"run", execproc, {0, 0}},
#endif
        {"save-file", filesave, {0, 0}},
        {"save-window", savewnd, {0, 0}},
        {"scroll-next-up", scrnextup, {0, 0}},
        {"scroll-next-down", scrnextdw, {0, 0}},
        {"search-forward", forwsearch, {0, 0}},
        {"search-reverse", backsearch, {0, 0}},
        {"select-buffer", usebuffer, {0, 0}},
        {"set", setvar, {0, 0}},
#if     CRYPT
        {"set-encryption-key", set_encryption_key, {0, 0}},
#endif
        {"set-fill-column", setfillcol, {0, 0}},
        {"set-mark", setmark, {0, 0}},
#if PROC
        {"set-pttable", set_pttable, {0, 0}},       /* GGR */
#endif
        {"shell-command", spawn, {0, 1}},
        {"shrink-window", shrinkwind, {0, 1}},
        {"split-current-window", splitwind, {0, 1}},
        {"store-macro", storemac, {0, 0}},
#if     PROC
        {"store-procedure", storeproc, {0, 0}},
        {"store-pttable", storepttable, {0, 0}},    /* GGR */
#endif
#if     BSD | __hpux | SVR4
        {"suspend-emacs", bktoshell, {0, 0}},
#endif
#if PROC
        {"toggle-ptmode", toggle_ptmode, {0, 0}},   /* GGR */
#endif
        {"transpose-characters", twiddle, {0, 0}},
#if     AEDIT
        {"trim-line", trim, {0, 0}},
#endif
        {"type-tab", typetab, {0, 0}},              /* GGR */
        {"unbind-key", unbindkey, {0, 0}},
        {"universal-argument", unarg, {0, 0}},
        {"unmark-buffer", unmark, {0, 0}},
        {"update-screen", upscreen, {0, 0}},
        {"view-file", viewfile, {0, 0}},
        {"white-delete", whitedelete, {0, 0}},      /* GGR */
        {"widen-from-region", widen, {0, 0}},       /* GGR */
        {"wrap-word", wrapword, {0, 0}},
        {"write-file", filewrite, {0, 0}},
        {"write-message", writemsg, {0, 0}},
        {"yank", yank, {0, 0}},
        {"yank-minibuffer", yankmb, {0, 0}},        /* GGR */

        {"", NULL}
};

/* Routine to produce an array index for names sorted by function call
 * To be called from main() at start-up time.
 */

#include <stddef.h>
#include "idxsorter.h"

static int *name_index;
static int needed = sizeof(names)/sizeof(struct name_bind);

void init_namelookup(void) {
    name_index = malloc((needed+1)*sizeof(int));
    struct fields fdef;
    fdef.offset = offsetof(struct name_bind, n_func);
    fdef.type = 'P';
    fdef.len = sizeof(fn_t);
    idxsort_fields((unsigned char *)names, name_index,
          sizeof(struct name_bind), needed, 1, &fdef);
    return;
}

struct name_bind *func_info(fn_t func) {
    int first = 1;
    int last = needed + 1;
    int middle = (first + last)/2;

    while (first <= last) {
        if (names[name_index[middle]-1].n_func < func) first = middle + 1;
        else if (names[name_index[middle]-1].n_func == func) break;
        else last = middle - 1;
        middle = (first + last)/2;
    }
    if (first > last) {
// For debugging - these need unistd.h + stdio.h
//        mlwrite("NOT FOUND!!");
//        sleep(3);
        return NULL;
    }
    return &names[name_index[middle]-1];
}
