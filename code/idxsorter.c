/* idxsorter.c    Pointer sort routine
 */

#include <memory.h>
#include <stdlib.h>

#include "idxsorter.h"

#define IS_BIG_ENDIAN (__BYTE_ORDER == __BIG_ENDIAN)

/**********************************************************************
 * idxsort_fields
 *      Produce an index of the fields in the given sort order.
 *      Sorts on the given character (byte) fields, so can be used
 *      as a generic sort for anything.
 * NOTE! NOTE! NOTE! NOTE! NOTE! NOTE! NOTE! NOTE! NOTE! NOTE!
 *  The index result is 1-based, and the results start at index[1].
 *  So, to get the nth item using the index you need to use:
 *     rec[index[n]-1]
 *  which means that for C 0-based indexing in a 0 -> n-1 loop you need:
 *     rec[index[n+1]-1]
 *
 * records          Records to sort
 * index            Returned index (must be rec_count + 1 long!)
 * rec_length       Record length in **bytes**!
 * rec_count        Number of records to sort
 * field_count      Number of sorting fields
 * fields           Field start/end columns (pairs)
 */

int idxsort_fields(unsigned char *records, int index[],
     int rec_length, int rec_count, int field_count, struct fields *fields) {

    int icount;         /* Loop counter */
    int char_offset;    /* Character offset from start of record */
    int done;           /* A binary flag */
    int link;           /* A link value */
    int new_link;       /* The logically next link value */
    int join;           /* Link to join at current chain_join */
    int record_number;  /* Physical number of the record */
    int chain_base;     /* Base of chain of inversions */
    int chain_join;     /* Chain joining point */
    int last_saved;     /* Last value of last[current_char] */
    int current_char;   /* Value of current char being considered */
    int lowest_char;    /* Lowest char encountered at this offset so far */
    int highest_char;   /* Highest char ditto */
    int first[256];     /* Chain roots for individual character chains */
    int last[256];      /* Chain ends ditto */

/*--------------------------------------------------------------------*/

    if (rec_count <= 0)  /* Refuse to sort non-positive number of elements */
        return 1;

/* Initialise the two internal "character chain" arrays. */

    (void)memset(first, 0, 256*sizeof(int));
    (void)memset(last, 0, 256*sizeof(int));

/* Set up the initial index entries to reflect the physical order of records.
 * This guarantees the sort to be conservative, because order changes are
 * only done when actually required. The dummy zero element always points
 * at the lowest entry - hence it is always "pre-sorted", which is why it is
 * give a negative link. We *never* sort across negative links!
 */

    for (icount = 1; icount < rec_count; icount++)
        index[icount] = icount + 1; /* Initial links reflect natural order */
    index[rec_count] = 0;           /* Signals end of logical chain */
    index[0] = -1;                  /* Entry to chain stored here */
    done = 0;

/* Here comes the main double loop. The outer one steps through fields, the
 * inner one steps through characters within fields _ their combined effect
 * is that of a single loop, which processes the characters in the order
 * in which they are defined by the sorting fields. Note that we process
 * the nth char in *all* records, before even looking n+1st. The reason being
 * that further characters often cannot affect the order.
 */

    for (icount = 0; icount < field_count; icount++) {

/* We need to handle different field type differently.
 * The last_offset is set to the first bytes we *don't* need to test.
 */

        int first_offset, last_offset, change_offset;
        if (fields[icount].type == 'C' || IS_BIG_ENDIAN) {
            first_offset = fields[icount].offset;
            last_offset = fields[icount].offset + fields[icount].len;
            change_offset = 1;
        }
        else {      /* little-endian numeric */
            first_offset = fields[icount].offset + fields[icount].len - 1;
            last_offset = fields[icount].offset - 1;
            change_offset = -1;
	}

/* The termination test needs to cater for either direction */

        for (char_offset = first_offset;
             char_offset != last_offset; char_offset += change_offset) {
            if (done) goto we_are_done;
            done = 1;                      /* Assume records already ordered */
            chain_join = 0;                /* The splice point for 1st chain */
            link = -index[chain_join];     /* 1st index always negative! */

/* Search for the first positive link - remember, negative ones denote
 * a stable ordering, while zero denotes end of chain!
 */

search_for_more:
            while ((new_link = index[link]) < 0) {  /* Skip negative links */
                chain_join = link;
                link = -new_link;
            }
            if (!new_link)  continue;   /* End of this pass through records */

/* Prepare the sub-chain for the correctly offset char in the 1st record.
 * Also, initialise the values of the highest and lowest encountered chars.
 */

            current_char = *(records + (link - 1) * rec_length + char_offset);
            first[current_char] = link; /* It's the first in this sub-chain */
            last[current_char] = link;  /* And also the last one */
            lowest_char = current_char;
            highest_char = current_char;
            link = new_link;
/*
 * Set up a positively-linked sub-chain for each distinct character
 * encountered more than once in the so-far equal strings.
 */
            while (link > 0) {
                current_char = *(records + (link - 1)*rec_length + char_offset);
                last_saved = last[current_char]; /* Preserve former value */
                last[current_char] = link;  /* Stuff in the new last one */
                if (last_saved <= 0) {      /* Was there any originally? */
                    first[current_char] = link;     /* No. Init 1st too. */
                    if (current_char < lowest_char) /* And highest/lowest */
                        lowest_char = current_char;
                    else if (current_char > highest_char)
                        highest_char = current_char;
                }
                else {
                    done = 0;       /* Else note that order not yet stable */
                    index[last_saved] = link;   /* Append this to sub-chain */
                }
                link = index[link]; /* Proceed to the logically next record */
            }

/* Now we have the unstable part of the chain snipped into sub-chain of
 * so-far equal records. Splice them together with negative links, because
 * the order on the boundaries *is* stable. Use the highest/lowest chars
 * to narrow the loop bounds.
 */

            for (current_char = lowest_char;  current_char <= highest_char;
                 current_char++) {
                if (!(join = first[current_char]))  /* Last sub-chain? */
                    continue;
                index[chain_join] = -join;      /* Splice with stable link */
                chain_join = last[current_char];/* Remember new splice point */
                first[current_char] = 0;        /* Zap the sub-chain entries */
                last[current_char] = 0;
            }

/* Splice on the current link value and if not at end of record chain,
 * loop back to skip over any stable (negative) links to search for any
 * more positive (unstable) regions.
 */
            index[chain_join] = link;
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
    link = -index[0];
    icount = 0;
    while (link) {
        new_link = abs(index[link]);
        index[link] = --icount;
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
    while (rec_count) {
        while (index[++chain_base] > 0);    /* Look for 0 or -ve one */
        link = -index[chain_base];
        record_number = chain_base;
        while (1) {
            new_link = -index[link];
            index[link] = record_number;
            record_number = link;
            link = new_link;
            rec_count--;
            if (record_number == chain_base) break;
        }
    }
    return 0;
}
