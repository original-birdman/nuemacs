/* Name to function binding table.
 *
 * This table gives the names of all the bindable functions
 * their C function address and additional property flags.
 * These are used for the bind-to-key function.
*/

#include "estruct.h"
#include "edef.h"
#include "efunc.h"
#include "line.h"

struct name_bind names[] = {
    {"abort-command", ctrlg, {0, 0}},
    {"add-mode", setemode, {0, 0}},
    {"add-global-mode", setgmode, {0, 0}},
    {"apropos", apro, {0, 1}},
    {"args-in-reexec", re_args_exec, {0, 0}},
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
    {"change-file-name", filename, {0, 1}},
    {"change-screen-size", newsize, {0, 1}},
    {"change-screen-width", newwidth, {0, 1}},
    {"char-replace", char_replace, {0, 0}},     /* GGR */
    {"clear-and-redraw", redraw, {0, 0}},
    {"clear-message-line", clrmes, {0, 0}},
    {"copy-region", copyregion, {0, 0}},
    {"count-words", wordcount, {0, 1}},
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
    {"detab-line", detab, {0, 0}},
    {"end-macro", ctlxrp, {1, 0}},
    {"end-of-file", gotoeob, {0, 0}},
    {"end-of-line", gotoeol, {0, 0}},
    {"eos-chars", eos_chars, {0, 0}},           /* GGR */
    {"entab-line", entab, {0, 0}},
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
    {"execute-procedure", execproc, {0, 0}},
    {"execute-program", execprg, {0, 1}},
    {"exit-emacs", quit, {0, 1}},
    {"fill-paragraph", fillpara, {0, 0}},
    {"fill-all-paragraphs", fillwhole, {0, 0}}, /* GGR */
    {"filter-buffer", filter_buffer, {0, 0}},
    {"find-file", filefind, {0, 0}},
    {"forward-character", forwchar, {0, 0}},
    {"ggr-style", ggr_style, {0, 0}},           /* GGR */
    {"goto-line", gotoline, {0, 0}},
    {"goto-matching-fence", getfence, {0, 0}},
    {"grow-window", enlargewind, {0, 1}},
    {"handle-tab", insert_tab, {0, 0}},
    {"help", help, {0, 1}},
    {"i-shell", spawncli, {0, 1}},
    {"incremental-search", fisearch, {0, 1}},
    {"insert-file", insfile, {0, 0}},
    {"insert-space", insspace, {0, 0}},
    {"insert-string", istring, {0, 0}},
    {"insert-raw-string", rawstring, {0, 0}},
    {"justify-paragraph", justpara, {0, 0}},
    {"kill-paragraph", killpara, {0, 0}},
    {"kill-region", killregion, {0, 0}},
    {"kill-to-end-of-line", killtext, {0, 0}},
    {"leave-one-white", leaveone, {0, 0}},      /* GGR */
    {"list-buffers", listbuffers, {0, 1}},
    {"macro-helper", macro_helper, {0, 1}},     /* GGR */
    {"makelist-region", makelist_region, {0, 0}},       /* GGR */
    {"numberlist-region", numberlist_region, {0, 0}},   /* GGR */
    {"meta-prefix", metafn, {0, 0}},
    {"move-window-down", mvdnwind, {0, 1}},
    {"move-window-up", mvupwind, {0, 1}},
    {"name-buffer", namebuffer, {0, 1}},
    {"narrow-to-region", narrow, {0, 1}},       /* GGR */
    {"newline", insert_newline, {0, 0}},
    {"newline-and-indent", indent, {0, 0}},
    {"next-buffer", nextbuffer, {0, 1}},
    {"next-line", forwline, {0, 0}},
    {"next-page", forwpage, {0, 0}},
    {"next-paragraph", gotoeop, {0, 0}},
    {"next-pttable", next_pttable, {0, 0}},     /* GGR */
    {"next-window", nextwind, {0, 1}},
    {"next-word", forwword, {0, 0}},
    {"nop", nullproc, {1, 0}},
    {"open-line", openline, {0, 0}},
    {"overwrite-string", ovstring, {0, 0}},
    {"pipe-command", pipecmd, {0, 1}},
    {"previous-line", backline, {0, 0}},
    {"previous-page", backpage, {0, 0}},
    {"previous-paragraph", gotobop, {0, 0}},
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
    {"restore-window", restwnd, {0, 1}},
    {"replace-string", sreplace, {0, 0}},
    {"reverse-incremental-search", risearch, {0, 1}},
    {"run", execproc, {0, 0}},
    {"save-file", filesave, {0, 0}},
    {"save-window", savewnd, {0, 1}},
    {"scroll-next-up", scrnextup, {0, 1}},
    {"scroll-next-down", scrnextdw, {0, 1}},
    {"search-forward", forwsearch, {0, 0}},
    {"search-reverse", backsearch, {0, 0}},
    {"select-buffer", usebuffer, {0, 1}},
    {"set", setvar, {0, 0}},
    {"set-encryption-key", set_encryption_key, {0, 1}},
    {"set-fill-column", setfillcol, {0, 1}},
    {"set-mark", setmark, {0, 0}},
    {"set-pttable", set_pttable, {0, 0}},       /* GGR */
    {"shell-command", spawn, {0, 1}},
    {"shrink-window", shrinkwind, {0, 1}},
    {"split-current-window", splitwind, {0, 1}},
    {"store-macro", storemac, {0, 0}},
    {"store-procedure", storeproc, {0, 0}},
    {"store-pttable", storepttable, {0, 0}},    /* GGR */
#if BSD | __hpux | SVR4
    {"suspend-emacs", bktoshell, {0, 0}},
#endif
    {"toggle-ptmode", toggle_ptmode, {0, 0}},   /* GGR */
    {"transpose-characters", twiddle, {0, 0}},
    {"trim-line", trim, {0, 0}},
    {"type-tab", typetab, {0, 0}},              /* GGR */
    {"unbind-key", unbindkey, {0, 0}},
    {"universal-argument", unarg, {0, 0}},
    {"unmark-buffer", unmark, {0, 1}},
    {"update-screen", upscreen, {0, 0}},
    {"view-file", viewfile, {0, 0}},
    {"white-delete", whitedelete, {0, 0}},      /* GGR */
    {"widen-from-region", widen, {0, 1}},       /* GGR */
    {"wrap-word", wrapword, {0, 0}},
    {"write-file", filewrite, {0, 0}},
    {"write-message", writemsg, {0, 0}},
    {"yank", yank, {0, 0}},
    {"yank-minibuffer", yankmb, {0, 0}},        /* GGR */
    {"yank-replace", yank_replace, {0, 0}},
/* Must be last!!!
 * getname() does a serial search and stops at a NULL function pointer
 */
    {"", NULL, {0, 0}},
};

/* Routine to produce an array index for names sorted by:
 *      a) function call (addr)
 *      b) function name
 * To be called from main() at start-up time.
 */

#include <stddef.h>
#include "idxsorter.h"

/* We can ignore the final NULL entry for the index searching */
static int needed = sizeof(names)/sizeof(struct name_bind) - 1;
static int *func_index;
static int *name_index;
static int *next_name_index;

void init_namelookup(void) {
    struct fields fdef;

    fdef.offset = offsetof(struct name_bind, n_func);
    fdef.type = 'P';
    fdef.len = sizeof(fn_t);
    func_index = Xmalloc((needed+1)*sizeof(int));
    idxsort_fields((unsigned char *)names, func_index,
          sizeof(struct name_bind), needed, 1, &fdef);

    fdef.offset = offsetof(struct name_bind, n_name);
    fdef.type = 'S';
    fdef.len = 0;
    name_index = Xmalloc((needed+1)*sizeof(int));
    idxsort_fields((unsigned char *)names, name_index,
          sizeof(struct name_bind), needed, 1, &fdef);

/* We want to step through this one, so need a next index too */
    next_name_index = Xmalloc((needed+1)*sizeof(int));
    make_next_idx(name_index, next_name_index, needed);
    return;
}

/* Lookup by function call address.
 * NOTE: that we use a binary chop that ensures we find the first
 * entry of any multiple ones.
 */
struct name_bind *func_info(fn_t func) {
    int first = 0;
    int last = needed - 1;
    int middle;

    while (first != last) {
        middle = (first + last)/2;
/* middle is too low, so try from middle + 1 */
        if ((void*)names[func_index[middle]].n_func < (void*)func)
             first = middle + 1;
/* middle is at or beyond start, so set last here */
        else last = middle;
    }
    if (names[func_index[first]].n_func != func) return NULL;
    return &names[func_index[first]];
}

/* Lookup by function name.
 * NOTE: that we don't need a binary chop that ensures we find the first
 * entry of any multiple ones, as there can't be such entries!
 */
struct name_bind *name_info(char *name) {
    int first = 0;
    int last = needed - 1;

    int middle;
    while (first <= last) {
        middle = (first + last)/2;
        int res = strcmp(names[name_index[middle]].n_name, name);
        if (res < 0) first = middle + 1;
        else if (res == 0) break;
        else last = middle - 1;
    }
    if (first > last) return NULL;
    return &names[name_index[middle]];
}

/* A function to allow you to step through the index in order.
 * For each input index, it returns the next one.
 * If given -1, it will return the first item and when there are no
 * further entries it will return -1.
 * If the index is out of range it will return -2.
 */
int nxti_name_info(int ci) {
    if (ci == -1) return name_index[0];
    if ((ci >= 0) && (ci < needed)) {
        int ni = next_name_index[ci];
        if (ni >= 0) return ni;
        return -1;
    }
    return -2;
}
