/* Name to function binding table.
 *
 * This table gives the names of all the bindable functions
 * their C function address and additional property flags.
 * These are used for the bind-to-key function.
*/

#define NAMES_C

#include "estruct.h"
#include "edef.h"
#include "efunc.h"
#include "line.h"
#include "util.h"

struct name_bind names[] = {
    {"abort-command", ctrlg, {0, 0, 0, 0, 0, 0}, CFNONE},
    {"add-mode", setemode, {0, 0, 0, 1, 0, 0}, CFALL},
    {"add-global-mode", setgmode, {0, 0, 0, 1, 0, 0}, CFALL},
    {"apropos", apro, {0, 1, 0, 0, 0, 0}, CFALL},
    {"args-in-reexec", re_args_exec, {0, 0, 0, 1, 0, 0}, CFALL},
    {"back-to-pin", back_to_pin, {0, 1, 1, 0, 0, 0}, CFNONE},
    {"backward-character", backchar, {0, 0, 0, 0, 0, 0}, CFNONE},
    {"begin-macro", ctlxlp, {0, 0, 0, 0, 0, 0}, CFNONE},
    {"beginning-of-file", gotobob, {0, 0, 0, 0, 0, 0}, CFNONE},
    {"beginning-of-line", gotobol, {0, 0, 0, 0, 0, 0}, CFNONE},
    {"bind-to-key", bindtokey, {0, 0, 0, 1, 0, 0}, CFALL},
    {"buffer-position", showcpos, {0, 0, 0, 1, 0, 0}, CFALL},
    {"buffer-to-key", buffertokey, {0, 0, 0, 1, 0, 0}, CFALL},
    {"case-region-lower", lowerregion, {0, 0, 0, 0, 0, 0}, CFNONE},
    {"case-region-upper", upperregion, {0, 0, 0, 0, 0, 0}, CFNONE},
    {"case-word-capitalize", capword, {0, 0, 0, 0, 0, 0}, CFNONE},
    {"case-word-lower", lowerword, {0, 0, 0, 0, 0, 0}, CFNONE},
    {"case-word-upper", upperword, {0, 0, 0, 0, 0, 0}, CFNONE},
    {"change-file-name", filename, {0, 1, 0, 1, 0, 0}, CFALL},
    {"char-replace", char_replace, {0, 0, 0, 0, 0, 0}, CFNONE},
    {"clear-and-redraw", redraw, {0, 0, 0, 1, 0, 0}, CFALL},
    {"clear-message-line", clrmes, {0, 0, 0, 1, 0, 0}, CFNONE},
    {"copy-region", copyregion, {0, 0, 0, 1, 0, 0}, CFCPCN|CFKILL},
    {"count-words", wordcount, {0, 1, 0, 1, 0, 0}, CFNONE},
    {"ctlx-prefix", cex, {0, 0, 0, 0, 0, 0}, CFNONE},
    {"delete-blank-lines", deblank, {0, 0, 0, 0, 0, 0}, CFNONE},
    {"delete-buffer", killbuffer, {0, 0, 0, 0, 0, 0}, CFNONE},
    {"delete-mode", delmode, {0, 0, 0, 0, 0, 0}, CFNONE},
    {"delete-global-mode", delgmode, {0, 0, 0, 0, 0, 0}, CFNONE},
    {"delete-next-character", forwdel, {0, 0, 0, 0, 0, 0}, CFKILL},
    {"delete-next-word", delfword, {0, 0, 0, 0, 0, 0}, CFKILL},
    {"delete-other-windows", onlywind, {0, 1, 0, 0, 0, 0}, CFNONE},
    {"delete-previous-character", backdel, {0, 0, 0, 0, 0, 0}, CFKILL},
    {"delete-previous-word", delbword, {0, 0, 0, 0, 0, 0}, CFKILL},
    {"delete-var", delvar, {0, 0, 0, 0, 0, 0}, CFNONE},
    {"delete-window", delwind, {0, 1, 0, 0, 0, 0}, CFNONE},
    {"describe-bindings", desbind, {0, 1, 0, 0, 0, 0}, CFNONE},
    {"describe-key", deskey, {0, 0, 0, 1, 0, 0}, CFALL},
    {"detab-line", detab, {0, 0, 0, 0, 0, 0}, CFNONE},
    {"drop-pin", drop_pin, {0, 1, 1, 0, 0, 0}, CFALL},
    {"end-macro", ctlxrp, {1, 0, 0, 0, 0, 0}, CFNONE},
    {"end-of-file", gotoeob, {0, 0, 0, 0, 0, 0}, CFNONE},
    {"end-of-line", gotoeol, {0, 0, 0, 0, 0, 0}, CFNONE},
    {"eos-chars", eos_chars, {0, 0, 0, 1, 0, 0}, CFALL},
    {"entab-line", entab, {0, 0, 0, 0, 0, 0}, CFNONE},
    {"exchange-point-and-mark", swapmark, {0, 0, 0, 0, 0, 0}, CFNONE},
    {"execute-buffer", execbuf, {0, 0, 0, 0, 0, 0}, CFALL},
    {"execute-command-line", execcmd, {0, 0, 0, 0, 0, 0}, CFALL},
    {"execute-file", execfile, {0, 0, 0, 0, 0, 0}, CFALL},
    {"execute-macro", ctlxe, {1, 0, 0, 0, 0, 0}, CFALL},
    {"execute-macro-1", cbuf1, {0, 0, 0, 0, 0, 0}, CFALL},
    {"execute-macro-2", cbuf2, {0, 0, 0, 0, 0, 0}, CFALL},
    {"execute-macro-3", cbuf3, {0, 0, 0, 0, 0, 0}, CFALL},
    {"execute-macro-4", cbuf4, {0, 0, 0, 0, 0, 0}, CFALL},
    {"execute-macro-5", cbuf5, {0, 0, 0, 0, 0, 0}, CFALL},
    {"execute-macro-6", cbuf6, {0, 0, 0, 0, 0, 0}, CFALL},
    {"execute-macro-7", cbuf7, {0, 0, 0, 0, 0, 0}, CFALL},
    {"execute-macro-8", cbuf8, {0, 0, 0, 0, 0, 0}, CFALL},
    {"execute-macro-9", cbuf9, {0, 0, 0, 0, 0, 0}, CFALL},
    {"execute-macro-10", cbuf10, {0, 0, 0, 0, 0, 0}, CFALL},
    {"execute-macro-11", cbuf11, {0, 0, 0, 0, 0, 0}, CFALL},
    {"execute-macro-12", cbuf12, {0, 0, 0, 0, 0, 0}, CFALL},
    {"execute-macro-13", cbuf13, {0, 0, 0, 0, 0, 0}, CFALL},
    {"execute-macro-14", cbuf14, {0, 0, 0, 0, 0, 0}, CFALL},
    {"execute-macro-15", cbuf15, {0, 0, 0, 0, 0, 0}, CFALL},
    {"execute-macro-16", cbuf16, {0, 0, 0, 0, 0, 0}, CFALL},
    {"execute-macro-17", cbuf17, {0, 0, 0, 0, 0, 0}, CFALL},
    {"execute-macro-18", cbuf18, {0, 0, 0, 0, 0, 0}, CFALL},
    {"execute-macro-19", cbuf19, {0, 0, 0, 0, 0, 0}, CFALL},
    {"execute-macro-20", cbuf20, {0, 0, 0, 0, 0, 0}, CFALL},
    {"execute-macro-21", cbuf21, {0, 0, 0, 0, 0, 0}, CFALL},
    {"execute-macro-22", cbuf22, {0, 0, 0, 0, 0, 0}, CFALL},
    {"execute-macro-23", cbuf23, {0, 0, 0, 0, 0, 0}, CFALL},
    {"execute-macro-24", cbuf24, {0, 0, 0, 0, 0, 0}, CFALL},
    {"execute-macro-25", cbuf25, {0, 0, 0, 0, 0, 0}, CFALL},
    {"execute-macro-26", cbuf26, {0, 0, 0, 0, 0, 0}, CFALL},
    {"execute-macro-27", cbuf27, {0, 0, 0, 0, 0, 0}, CFALL},
    {"execute-macro-28", cbuf28, {0, 0, 0, 0, 0, 0}, CFALL},
    {"execute-macro-29", cbuf29, {0, 0, 0, 0, 0, 0}, CFALL},
    {"execute-macro-30", cbuf30, {0, 0, 0, 0, 0, 0}, CFALL},
    {"execute-macro-31", cbuf31, {0, 0, 0, 0, 0, 0}, CFALL},
    {"execute-macro-32", cbuf32, {0, 0, 0, 0, 0, 0}, CFALL},
    {"execute-macro-33", cbuf33, {0, 0, 0, 0, 0, 0}, CFALL},
    {"execute-macro-34", cbuf34, {0, 0, 0, 0, 0, 0}, CFALL},
    {"execute-macro-35", cbuf35, {0, 0, 0, 0, 0, 0}, CFALL},
    {"execute-macro-36", cbuf36, {0, 0, 0, 0, 0, 0}, CFALL},
    {"execute-macro-37", cbuf37, {0, 0, 0, 0, 0, 0}, CFALL},
    {"execute-macro-38", cbuf38, {0, 0, 0, 0, 0, 0}, CFALL},
    {"execute-macro-39", cbuf39, {0, 0, 0, 0, 0, 0}, CFALL},
    {"execute-macro-40", cbuf40, {0, 0, 0, 0, 0, 0}, CFALL},
    {"execute-named-command", namedcmd, {1, 0, 0, 1, 0, 0}, CFALL},
    {"execute-procedure", execproc, {0, 0, 0, 0, 0, 0}, CFALL},
    {"execute-program", execprg, {0, 1, 0, 0, 0, 0}, CFNONE},
    {"exit-emacs", quit, {0, 1, 0, 0, 0, 0}, CFNONE},
    {"fill-paragraph", fillpara, {0, 0, 0, 0, 0, 0}, CFNONE},
    {"fill-all-paragraphs", fillwhole, {0, 0, 0, 0, 0, 0}, CFNONE},
    {"filter-buffer", filter_buffer, {0, 0, 0, 0, 0, 0}, CFNONE},
    {"find-file", filefind, {0, 0, 0, 0, 0, 0}, CFNONE},
    {"forward-character", forwchar, {0, 0, 0, 0, 0, 0}, CFNONE},
    {"ggr-style", ggr_style, {0, 0, 0, 1, 0, 0}, CFNONE},
    {"goto-line", gotoline, {0, 0, 0, 0, 0, 0}, CFNONE},
    {"goto-matching-fence", getfence, {0, 0, 0, 0, 0, 0}, CFNONE},
    {"grow-window", enlargewind, {0, 1, 0, 1, 0, 0}, CFNONE},
    {"handle-tab", insert_tab, {0, 0, 0, 0, 0, 0}, CFNONE},
    {"help", help, {0, 1, 0, 1, 0, 0}, CFNONE},
/* Marked as non_moving for convenience - the flag is for them...*/
    {"hunt-forward", forwhunt, {0, 1, 1, 1, 0, 0}, CFNONE},
    {"hunt-backward", backhunt, {0, 1, 1, 1, 0, 0}, CFNONE},
    {"i-shell", spawncli, {0, 1, 0, 0, 0, 0}, CFNONE},
    {"incremental-search", fisearch, {0, 1, 0, 0, 0, 0}, CFNONE},
    {"insert-file", insfile, {0, 0, 0, 0, 0, 0}, CFNONE},
    {"insert-space", insspace, {0, 0, 0, 0, 0, 0}, CFNONE},
    {"insert-tokens", itokens, {0, 0, 0, 0, 0, 0}, CFNONE},
    {"insert-string", istring, {0, 0, 0, 0, 0, 0}, CFNONE},
    {"justify-paragraph", justpara, {0, 0, 0, 0, 0, 0}, CFNONE},
    {"kill-paragraph", killpara, {0, 0, 0, 0, 0, 0}, CFKILL},
    {"kill-region", killregion, {0, 0, 0, 0, 0, 0}, CFKILL},
    {"kill-to-end-of-line", killtext, {0, 0, 0, 0, 0, 0}, CFKILL},
    {"leave-one-white", leaveone, {0, 0, 0, 0, 0, 0}, CFNONE},
    {"list-buffers", listbuffers, {0, 1, 0, 1, 0, 0}, CFALL},
    {"macro-helper", macro_helper, {0, 1, 1, 0, 0, 0}, CFNONE},
    {"makelist-region", makelist_region, {0, 0, 0, 0, 0, 0}, CFNONE},
    {"numberlist-region", numberlist_region, {0, 0, 0, 0, 0, 0}, CFNONE},
    {"meta-prefix", metafn, {0, 0, 0, 0, 0, 0}, CFNONE},
    {"move-window-down", mvdnwind, {0, 1, 0, 0, 0, 0}, CFNONE},
    {"move-window-up", mvupwind, {0, 1, 0, 0, 0, 0}, CFNONE},
    {"name-buffer", namebuffer, {0, 1, 0, 1, 0, 0}, CFALL},
    {"narrow-to-region", narrow, {0, 1, 0, 0, 0, 0}, CFCPCN},
    {"newline", insert_newline, {0, 0, 0, 0, 0, 0}, CFNONE},
    {"newline-and-indent", indent, {0, 0, 0, 0, 0, 0}, CFNONE},
    {"next-buffer", nextbuffer, {0, 1, 0, 0, 0, 0}, CFNONE},
    {"next-line", forwline, {0, 0, 0, 0, 0, 0}, CFCPCN},
    {"next-page", forwpage, {0, 0, 0, 0, 0, 0}, CFNONE},
    {"next-paragraph", gotoeop, {0, 0, 0, 0, 0, 0}, CFNONE},
    {"next-pttable", next_pttable, {0, 0, 0, 0, 0, 0}, CFNONE},
    {"next-window", nextwind, {0, 1, 0, 0, 0, 0}, CFNONE},
    {"next-word", forwword, {0, 0, 0, 0, 0, 0}, CFNONE},
    {"nop", nullproc, {1, 0, 0, 1, 0, 0}, CFALL},
    {"open-parent", open_parent, {0, 1, 0, 0, 0, 0}, CFNONE},
    {"open-line", openline, {0, 0, 0, 0, 0, 0}, CFNONE},
    {"overwrite-string", ovstring, {0, 0, 0, 0, 0, 0}, CFNONE},
    {"pipe-command", pipecmd, {0, 1, 0, 0, 0, 0}, CFNONE},
    {"previous-line", backline, {0, 0, 0, 0, 0, 0}, CFCPCN},
    {"previous-page", backpage, {0, 0, 0, 0, 0, 0}, CFNONE},
    {"previous-paragraph", gotobop, {0, 0, 0, 0, 0, 0}, CFNONE},
    {"previous-window", prevwind, {0, 1, 0, 0, 0, 0}, CFNONE},
    {"previous-word", backword, {0, 0, 0, 0, 0, 0}, CFNONE},
    {"query-replace-string", qreplace, {0, 1, 0, 0, 0, 0}, CFNONE},
    {"quick-exit", quickexit, {0, 1, 0, 0, 0, 0}, CFNONE},
    {"quote-character", quote, {1, 0, 0, 0, 0, 0}, CFNONE},
    {"quoted-count", quotedcount, {0, 0, 0, 0, 0, 0}, CFNONE},
    {"read-file", fileread, {0, 0, 0, 0, 0, 0}, CFNONE},
    {"redraw-display", reposition, {0, 0, 0, 1, 0, 0}, CFALL},
/* Marked as non_moving for convenience. Original command will be correct */
    {"reexecute", reexecute, {0, 0, 0, 1, 0, 0}, CFALL},
    {"resize-window", resize, {0, 1, 0, 1, 0, 0}, CFNONE},
    {"restore-window", restwnd, {0, 1, 0, 0, 0, 0}, CFNONE},
    {"replace-string", sreplace, {0, 0, 0, 0, 0, 0}, CFNONE},
    {"reverse-incremental-search", risearch, {0, 1, 0, 0, 0, 0}, CFNONE},
    {"run", execproc, {0, 0, 0, 0, 0, 0}, CFNONE},
    {"save-file", filesave, {0, 0, 0, 1, 0, 0}, CFALL},
    {"save-window", savewnd, {0, 1, 0, 1, 0, 0}, CFALL},
    {"scroll-next-up", scrnextup, {0, 1, 0, 1, 0, 0}, CFNONE},
    {"scroll-next-down", scrnextdw, {0, 1, 0, 1, 0, 0}, CFNONE},
/* Marked as non_moving for convenience - the flag is for them...*/
    {"search-forward", forwsearch, {0, 0, 0, 1, 0, 0}, CFNONE},
    {"search-reverse", backsearch, {0, 0, 0, 1, 0, 0}, CFNONE},
    {"select-buffer", usebuffer, {0, 1, 0, 0, 0, 0}, CFNONE},
    {"set", setvar, {0, 0, 0, 1, 0, 0}, CFALL},
    {"set-encryption-key", set_encryption_key, {0, 1, 0, 0, 0, 0}, CFALL},
    {"set-fill-column", setfillcol, {0, 1, 0, 1, 0, 0}, CFALL},
    {"set-force-mode", setforcemode, {0, 1, 0, 1, 0, 0}, CFALL},
    {"set-mark", setmark, {0, 0, 0, 1, 0, 0}, CFALL},
    {"set-pttable", set_pttable, {0, 0, 0, 1, 0, 0}, CFALL},
    {"shell-command", spawn, {0, 1, 0, 0, 0, 0}, CFNONE},
    {"shrink-window", shrinkwind, {0, 1, 0, 1, 0, 0}, CFALL},
    {"simulate", simulate, {0, 1, 1, 1, 0, 1}, CFNONE},
    {"simulate-incr", simulate_incr, {0, 1, 1, 1, 0, 1}, CFNONE},
    {"split-current-window", splitwind, {0, 1, 0, 1, 0, 0}, CFNONE},
    {"store-macro", storemac, {0, 0, 0, 1, 0, 0}, CFNONE},
    {"store-procedure", storeproc, {0, 0, 1, 1, 0, 0}, CFNONE},
    {"store-pttable", storepttable, {0, 0, 0, 1, 0, 0}, CFNONE},
    {"suspend-emacs", bktoshell, {0, 0, 0, 1, 0, 0}, CFNONE},
    {"switch-internal", switch_internal,  {0, 0, 0, 0, 0, 0}, CFNONE},
    {"switch-with-pin", switch_with_pin, {0, 1, 1, 0, 0, 0}, CFNONE},
    {"toggle-ptmode", toggle_ptmode, {0, 0, 0, 1, 0, 0}, CFALL},
    {"transpose-characters", twiddle, {0, 0, 0, 0, 0, 0}, CFNONE},
    {"trim-line", trim, {0, 0, 0, 0, 0, 0}, CFNONE},
    {"type-tab", typetab, {0, 0, 0, 0, 0, 0}, CFNONE},
    {"unbind-key", unbindkey, {0, 0, 0, 1, 0, 0}, CFALL},
    {"universal-argument", unarg, {0, 0, 0, 0, 0, 0}, CFALL},
    {"unmark-buffer", unmark, {0, 1, 0, 1, 0, 0}, CFALL},
    {"update-screen", upscreen, {0, 0, 0, 1, 0, 0}, CFALL},
    {"view-file", viewfile, {0, 0, 0, 0, 0, 0}, CFNONE},
    {"white-delete", whitedelete, {0, 0, 0, 0, 0, 0}, CFNONE},
    {"widen-from-region", widen, {0, 1, 0, 0, 0, 0}, CFNONE},
    {"wrap-word", wrapword, {0, 0, 0, 0, 0, 0}, CFNONE},
    {"write-file", filewrite, {0, 0, 0, 0, 0, 0}, CFNONE},
    {"write-message", writemsg, {0, 0, 0, 0, 0, 0}, CFNONE},
    {"yank", yank, {0, 0, 0, 0, 0, 0}, CFYANK},
    {"yank-minibuffer", yankmb, {0, 0, 0, 0, 0, 0}, CFYANK},
    {"yank-replace", yank_replace, {0, 0, 0, 0, 0, 0}, CFYANK},
/* Must be last!!!
 * getname() does a serial search and stops at a NULL function pointer
 */
    {"", NULL, {0, 0, 0, 0, 0, 0}, CFNONE},
};

/* Routine to produce an array index for names sorted by:
 *      a) function call (addr)
 *      b) function name
 * To be called from main() at start-up time.
 */

#include <stddef.h>
#include "idxsorter.h"

/* We can ignore the final NULL entry for the index searching */
static int needed = ARRAY_SIZE(names) - 1;
static int *func_index = NULL;
static int *name_index = NULL;
static int *next_name_index = NULL;

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

#ifdef DO_FREE
/* Add a call to allow free() of normally-unfreed items here for, e.g,
 * valgrind usage.
 */
void free_names(void) {
    if (func_index) Xfree(func_index);
    if (name_index) Xfree(name_index);
    if (next_name_index) Xfree(next_name_index);
    return;
}
#endif
