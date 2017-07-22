/* idxsorter.c    Pointer sort routine
 */

#include <memory.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

#include "idxsorter.h"

/**********************************************************************
 * idxsort_fields
 *  Produce an index of the records that gives the sorted order
 *  of the defined fields.
 *  The ordering is done on a byte-by-byte basis.
 *  This is based on original character-field sorting code, but
 *  has been extended to handle integer fields (signed or unsigned 16, 32 or
 *  64-byte integers) and pointers (which are "just" unsigned integers).
 *  It can also handle NUL-terminated C-strings in memory - set the type to
 *  S and it will continue to dereference (returning NUL for short
 *  strings) for the longest actual string.
 *
 *  The code has also been altered so that the index array no longer needs
 *  1 more element than there are records, and the field descriptions are
 *  now passed in as a structure (more C-ish, and required for specifying
 *  the type of field).
 *
 *  The index array counts records from 1 while working (as 0 is end-of-chain
 *  and -ve marks a stable entry).
 *  Attempting to adjust to 0-based When the final index[] array is being
 *  set did not work!!! (Presumably a result of closed loops?).
 *  So an additional pass over the array to decrement each entry
 *  is provided as an option.  (I may revist the direct approach...).
 *
 *  NOTE that the code never actually compares pairs of records!
 *  It works by looking at the nth character of each record in turn and
 *  creating links in index[] to all records with the same character
 *  using the fl_ch structure data to do so.
 *  If a pass on the nth characters has nothing to do then the sort is
 *  completed (even if there are more characters then they can't affect
 *  the result).
 *
 *  The final result index is then created by reading through the index[]
 *  array in link order and replacing each entry with the count it took to
 *  get there (roughly...).
 *
 * The input parameters are:
 *  records         Records to sort
 *  index           Returned index array (must be rec_count long!)
 *  rec_length      Record length in **bytes**!
 *  rec_count       Number of records to sort
 *  field_count     Number of sorting fields
 *  fields          Field type and start/end columns (field_count entries)
 */

/* Set this to 1 for 0-based (C) result or 0 to leave as 1-based (FORTRAN)
 */
#define INDEX_ADJUST 1

/* get_ibyte:
 *  An internal, inline routine to get the specified byte for a field.
 *  Done this way as there has to be some logic as to which that byte is
 *  for an integer (has to handle endianness issues) and for a signed
 *  integer we need to map INTxx_MIN...INTxx_MAX => 0...UINTxx_MAX
 */
static inline unsigned char get_ibyte(
     unsigned char *records, int reclen, int link, int *cstr_done,
     int offset, int start, int width, char type) {

    unsigned char *ip = records + reclen*(link-1) + start;
    if (type == 'C' || type == 'S') width = 1;

/* The code to handle multi-byte integers is generic; derivable from the
 * size of the variable type.
 */
#define int_case(ivar, imin) \
    case sizeof(ivar): \
        memcpy(&ivar, ip, sizeof(ivar));    /* Ensure alignment */ \
        if (type == 'I') ivar -= imin; \
        ivar >>= 8*(sizeof(ivar) - 1 - offset); \
        return (unsigned char)(ivar & 0xff)

    switch(width) {
    uint64_t size8;
    uint32_t size4;
    uint16_t size2;
    int_case(size8, INT64_MIN);
    int_case(size4, INT32_MIN);
    int_case(size2, INT16_MIN);
    case 1:
        if (type == 'S') {  /* NUL-terminated C-string */
            char *fp;
            memcpy(&fp, ip, sizeof(char *));
            for (int ofs = 0; ofs <= offset; ofs++) {
                if (*(fp+ofs) == 0) return 0;
            }
            *cstr_done = 0; /* Reset terminator */
            return *(fp+offset);
        }
        return *(ip+offset);            /* Index into char array */
    default:
/* This is really an error, but there's no way to signal that...
 * ...also, it should never happen as we've already validated the fields
 */
        return 0;
    }
}

/* The ACTUAL FUNCTION!! */

int idxsort_fields(unsigned char *records, int index[],
     int rec_length, int rec_count, int field_count, struct fields *fields) {

    int done;           /* A binary flag */
    int link;           /* A link value */
    int new_link;       /* The logically next link value */
    int join;           /* Link to join at current chain_join */
    int record_number;  /* Physical number of the record */
    int chain_base;     /* Base of chain of inversions */
    int chain_join;     /* Chain joining point */
    int last_saved;     /* Last value of last[cur_ch] */
    int cur_ch;         /* Value of current char being considered */
    int lo_ch;          /* Lowest char encountered at this offset so far */
    int hi_ch;          /* Highest char ditto */
    struct fl_ch {
        int first;      /* Chain roots for individual character chains */
        int last;       /* Chain ends ditto */
    } fl_ch[256];       /* One per byte */

    int elem0;          /* The "0-element" of a "1-based" index[] */

/* Macros to get and set index - to handle element 0 as a special case */

#define get_index(i) \
  ({int _i = i; (_i == 0)? elem0: index[_i-1];})
#define set_index(i, val) \
  ({int _i = i; if (_i == 0) elem0 = val; else index[_i-1] = val;})

/*--------------------------------------------------------------------*/

    if (rec_count <= 0)     /* Refuse to sort non-+ve number of elements */
        return 1;
    if (field_count <= 0)   /* Refuse to sort non-+ve number of fields */
        return 2;

/* Validate field types */

    for (int i = 0; i < field_count; i++) {
        switch(fields[i].type) {
        case 'C':           /* Character */
        case 'S':           /* NUL-terminated string */
            continue;
        case 'I':           /* Signed int */
        case 'U':           /* Unsigned int */
        case 'P':           /* Pointer */
            switch(fields[i].len) {
            case 2:
            case 4:
            case 8:
                continue;   /* All OK */
            default:
                return 3;   /* Invalid size */
            }
        default:
            return 4;       /* Invalid field type */
        }
     }

/* Initialise the two internal "character chain" arrays. */

    memset(fl_ch, 0, sizeof(fl_ch));

/* Set up the initial index entries to reflect the physical order of records.
 * This guarantees the sort to be conservative, because order changes are
 * only done when actually required. The dummy zero element always points
 * at the lowest entry - hence it is always "pre-sorted", which is why it is
 * give a negative link. We *never* sort across negative links!
 */

    for (int i = 0; i < rec_count-1; i++)
        index[i] = i + 2;       /* Initial links reflect natural order */
    index[rec_count - 1] = 0;   /* Signals end of logical chain */
    elem0 = -1;                 /* Entry to chain stored here */
    done = 0;

/* Here comes the main double loop. The outer one steps through fields, the
 * inner one steps through characters within fields _ their combined effect
 * is that of a single loop, which processes the characters in the order
 * in which they are defined by the sorting fields. Note that we process
 * the nth char in *all* records, before even looking n+1st. The reason being
 * that further characters often cannot affect the order.
 */

    for (int fi = 0; fi < field_count; fi++) {

/* Each field is treated as a set of bytes, obtained and sorted in order */

        int curfld_start = fields[fi].offset;
        int curfld_width = fields[fi].len;
        char curfld_type = fields[fi].type;
        if (curfld_type == 'S') curfld_width = INT32_MAX;   /* Let loop run */

        int c_strings_done = 0;
        for (int this_co = 0; this_co < curfld_width; this_co++) {
            if (c_strings_done) continue;   /* S fields finished */
            if (curfld_type == 'S') c_strings_done = 1;
            if (done) goto we_are_done;
            done = 1;                       /* Assume records already ordered */
            chain_join = 0;                 /* The splice point for 1st chain */
            link = -get_index(chain_join);  /* 1st index always negative! */

/* Search for the first positive link - remember, negative ones denote
 * a stable ordering, while zero denotes end of chain!
 */

search_for_more:
            while ((new_link = get_index(link)) < 0) {  /* Skip -ve links */
                chain_join = link;
                link = -new_link;
            }
            if (!new_link) continue;    /* End of this pass through records */

/* Prepare the sub-chain for the correctly offset char in the 1st record.
 * Also, initialise the values of the highest and lowest encountered chars.
 */

            cur_ch = get_ibyte(records, rec_length, link, &c_strings_done,
                 this_co, curfld_start, curfld_width, curfld_type);
            fl_ch[cur_ch].first = link; /* First in this sub-chain */
            fl_ch[cur_ch].last = link;  /* and also the last one */
            lo_ch = cur_ch;
            hi_ch = cur_ch;
            link = new_link;
/*
 * Set up a positively-linked sub-chain for each distinct character
 * encountered more than once in the so-far equal strings.
 */
            while (link > 0) {
                cur_ch = get_ibyte(records, rec_length, link, &c_strings_done,
                     this_co, curfld_start, curfld_width, curfld_type);
                last_saved = fl_ch[cur_ch].last;    /* Preserve former value */
                fl_ch[cur_ch].last = link;      /* Stuff in the new last one */
                if (last_saved <= 0) {          /* Was there any originally? */
                    fl_ch[cur_ch].first = link; /* No. Init 1st too. */
                    if (cur_ch < lo_ch)         /* And highest/lowest */
                        lo_ch = cur_ch;
                    else if (cur_ch > hi_ch)
                        hi_ch = cur_ch;
                }
                else {
                    done = 0;                       /* Order not yet stable */
                    set_index(last_saved, link);    /* Append to sub-chain */
                }
                link = get_index(link);     /* On to logically next record */
            }

/* Now we have the unstable part of the chain snipped into sub-chain of
 * so-far equal records. Splice them together with negative links, because
 * the order on the boundaries *is* stable. Use the highest/lowest chars
 * to narrow the loop bounds.
 */

            for (cur_ch = lo_ch; cur_ch <= hi_ch; cur_ch++) {
                if (!(join = fl_ch[cur_ch].first))  /* Last sub-chain? */
                    continue;
                set_index(chain_join, -join);   /* Splice with stable link */
                chain_join = fl_ch[cur_ch].last;/* Remember new splice point */
                fl_ch[cur_ch].first = 0;        /* Zap the sub-chain entries */
                fl_ch[cur_ch].last = 0;
            }

/* Splice on the current link value and if not at end of record chain,
 * loop back to skip over any stable (negative) links to search for any
 * more positive (unstable) regions.
 */
            set_index(chain_join, link);
            if (link < 0) {
                link = -link;
                goto search_for_more;
            }
        }
    }

/* Now convert links to permutation reflecting the physical sorted order.
 * This is done by going through the chain in the logical order while
 * counting records off from 0 down (you'll see why in a moment!) and
 * replacing link values with this physical record number.
 */

we_are_done:
    link = -elem0;
    int fi = 0;
    while (link) {
        new_link = abs(get_index(link));
        set_index(link, --fi);
        link = new_link;
    }

/*
 * Finally, generate the reverse permutation (yes, in situ!). This looks
 * tricksy - and is! Negative values are those not yet converted.
 * Search for the next such from the current chain base and make *it* into the
 * new chain base. Chain on from there, inverting the permutation until either
 * all records processed or we arrive back to chain-base (it is perfectly
 * possible to have closed loop in this process - work out why for your home
 * exercise!). If the latter. search from the chain base for the next
 * unconverted region and repeat!
 */
    chain_base = 0;
#if INDEX_ADJUST
    int orig_rec_count = rec_count;
#endif
    while (rec_count) {
        while (get_index(++chain_base) > 0);    /* Look for 0 or -ve one */
        link = -get_index(chain_base);
        record_number = chain_base;
        while (1) {
            new_link = -get_index(link);
            set_index(link, record_number);
            record_number = link;
            link = new_link;
            rec_count--;
            if (record_number == chain_base) break;
        }
    }
#if INDEX_ADJUST
    while (orig_rec_count--) index[orig_rec_count]--;
#endif
    return 0;
}
