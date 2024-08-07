/*      CRYPT.C
 *
 *      Encryption routines
 *
 *      written by Dana Hoggatt and Daniel Lawrence
 */

#include <stdio.h>
#include "estruct.h"

#define CRYPT_C

#include "edef.h"
#include "efunc.h"

/*  Apparently "The mathematical MOD does not match the computer MOD"
 */
static int mod95(int val) {

/*  Yes, what I do here may look strange, but it gets the
 * job done, and portably at that.
 */
        while (val >= 9500) val -= 9500;
        while (val >= 950)  val -= 950;
        while (val >= 95)   val -= 95;
        while (val < 0)     val += 95;
        return val;
}

/**********
 *
 *      myencrypt - in place encryption/decryption of a buffer
 *
 *      (C) Copyright 1986, Dana L. Hoggatt
 *      1216, Beck Lane, Lafayette, IN
 *
 *      When consulting directly with the author of this routine,
 *      please refer to this routine as the "DLH-POLY-86-B CIPHER".
 *
 *      This routine was written for Dan Lawrence, for use in V3.8 of
 *      MicroEMACS, a public domain text/program editor.
 *
 *      I kept the following goals in mind when preparing this function:
 *
 *          1.  All printable characters were to be encrypted back
 *              into the printable range, control characters and
 *              high-bit characters were to remain unaffected.  this
 *              way, encrypted would still be just as cheap to
 *              transmit down a 7-bit data path as they were before.
 *
 *          2.  The encryption had to be portable.  The encrypted
 *              file from one computer should be able to be decrypted
 *              on another computer.
 *
 *          3.  The encryption had to be inexpensive, both in terms
 *              of speed and space.
 *
 *          4.  The system needed to be secure against all but the
 *              most determined of attackers.
 *
 *      For encryption of a block of data, one calls myencrypt passing
 *      a pointer to the data block and its length. The data block is
 *      encrypted in place, that is, the encrypted output overwrites
 *      the input.  Decryption is totally isomorphic, and is performed
 *      in the same manner by the same routine.
 *
 *      Before using this routine for encrypting data, you are expected
 *      to specify an encryption key.  This key is an arbitrary string,
 *      to be supplied by the user.  To set the key takes two calls to
 *      myencrypt().  First, you call
 *
 *              myencrypt(NULL, vector)
 *
 *      This resets all internal control information.  Typically (and
 *      specifically in the case on MICRO-emacs) you would use a "vector"
 *      of 0.  Other values can be used to customize your editor to be
 *      "incompatible" with the normally distributed version.  For
 *      this purpose, the best results will be obtained by avoiding
 *      multiples of 95.
 *
 *      Then, you "encrypt" your password by calling
 *
 *              myencrypt(pass, strlen(pass))
 *
 *      where "pass" is your password string.  Myencrypt() will destroy
 *      the original copy of the password (it becomes encrypted),
 *      which is good.  You do not want someone on a multiuser system
 *      to peruse your memory space and bump into your password.
 *      Still, it is a better idea to erase the password buffer to
 *      defeat memory perusal by a more technical snooper.
 *
 *      For the interest of cryptologists, at the heart of this
 *      function is a Beaufort Cipher.  The cipher alphabet is the
 *      range of printable characters (' ' to '~'), all "control"
 *      and "high-bit" characters are left unaltered.
 *
 *      The key is a variant autokey, derived from a weighted sum
 *      of all the previous clear text and cipher text.  A counter
 *      is used as salt to obliterate any simple cyclic behavior
 *      from the clear text, and key feedback is used to assure
 *      that the entire message is based on the original key,
 *      preventing attacks on the last part of the message as if
 *      it were a pure autokey system.
 *
 *      Overall security of encrypted data depends upon three
 *      factors:  the fundamental cryptographic system must be
 *      difficult to compromise; exhaustive searching of the key
 *      space must be computationally expensive; keys and plaintext
 *      must remain out of sight.  This system satisfies this set
 *      of conditions to within the degree desired for MicroEMACS.
 *
 *      Though direct methods of attack (against systems such as
 *      this) do exist, they are not well known and will consume
 *      considerable amounts of computing time.  An exhaustive
 *      search requires over a billion investigations, on average.
 *
 *      The choice, entry, storage, manipulation, alteration,
 *      protection and security of the keys themselves are the
 *      responsibility of the user.
 *
 *
 * char *bptr;          buffer of characters to be encrypted
 * unsigned len;        number of characters in the buffer
 *
 **********/
void myencrypt(char *bptr, unsigned len) {
    int cc; /* current character being considered */

    static int key = 0;     /* 29 bit encipherment key */
    static int salt = 0;    /* salt to spice up key with */

    if (!bptr) {            /* is there anything here to encrypt? */
        key = len;          /* set the new key */
        salt = len;         /* set the new salt */
        return;
    }

    int printing_only = (crypt_mode & CRYPT_ONLYP);
    int do_mod95 = (crypt_mode & CRYPT_MOD95);

/* We go through every *byte* in the buffer.
 * If CRYPT_ONLYP we use the old code that leaves anything below space
 * alone and map the rest into their own range, which also means we can
 * never generate a newline, which would (apparently) have really messed
 * things up! So the range is 32(' ')->255 == 224 bytes.
 * We now read/write in a block, so can en/decrypt any byte.
 */
    while (len--) {
        cc = ch_as_uc(*bptr);   /* Get the next char - unsigned */
        if (!printing_only || ((cc >= ' ') && (cc <= '~'))) {

/* Feed the upper few bits of the key back into itself.
 * This ensures that the starting key affects the entire message.
 * We also ensure that the key only occupies the lower 29-bits at most.
 * This is so that the arithmetic calculation later which implements
 * our autokey, won't overflow, making the key go negative.
 * Machine behavior in these cases does not tend to be portable.
 */
            key = (key & 0x1FFFFFFF) ^ ((key >> 29) & 0x03);

            if (do_mod95) {
/* Down-bias the character, perform a Beaufort encipherment, and
 * up-bias the character again.  We want key to be positive
 * so that the left shift here will be more portable and the
 * mod95() faster
 */
                cc = mod95((key % 95) - (cc - ' ')) + ' ';
            }
            else {
/* Perform a Beaufort encipherment.
 * Just pick up the final 8-bits (we may have gone -ve here).
 */
                cc = ((key & 0xff) - cc) & 0xff;
            }

/* The salt will spice up the key a little bit, helping to obscure any
 * patterns in the clear text, particularly when all the characters (or
 * long sequences of them) are the same.
 * We do not want the salt to go negative, or it will affect the key
 * too radically.
 * It is always a good idea to chop off cyclics to prime values.
 */
            if (++salt >= 20857) salt = 0;  /* prime modulus */

/* Our autokey (a special case of the running key) is being generated
 * by a weighted checksum of cipher text, (unsigned) clear text and salt.
 */
            key = key + key + (cc ^ ch_as_uc(*bptr)) + salt;
        }
        *bptr++ = cc;   /* put character back into buffer */
    }

    return;
}

/* reset encryption key of current buffer
 *
 * int f;               default flag
 * int n;               numeric argument
 */
int set_encryption_key(int f, int n) {
    UNUSED(f); UNUSED(n);
    int status;             /* return status */
    int odisinp;            /* original value of disinp */

/* Is it enabled at all? */

    if (crypt_mode == 0) {
        mlforce("Crypt is not enabled. Set $crypt_mode");
        return FALSE;
    }

/* Turn command input echo off */
    odisinp = disinp;
    disinp = FALSE;

/* Get the string to use as an encryption string */
    char prompt[64];
    char *type, *ukey;
    int *klenp;
    if (crypt_mode & CRYPT_GLOBAL) {
        type = "Global";
        ukey = gl_enc_key;
        klenp = &gl_enc_len;
    }
    else {
        type = "Buffer";
        if (!curbp->b_key) curbp->b_key = Xmalloc(NKEY);
        ukey = curbp->b_key;
        klenp = &curbp->b_keylen;
    }
    sprintf(prompt, "%s encryption string: ", type);
    db_strdef(given);
    status = mlreply(prompt, &given, CMPLT_NONE);
    mlwrite_one(" ");       /* clear it off the bottom line */
    disinp = odisinp;
    if (status != TRUE) return status;

/* We only allow up to NKEY bytes to be used - so copy
 * at most NKEY to ukey (including the NUL).
 */
    int clen = (db_len(given) >= NKEY)? NKEY -1: db_len(given);
    strncpy(ukey, db_val(given), clen);
    terminate_str(ukey+clen);
    db_free(given);

    switch(crypt_mode & ~CRYPT_MOD95) {
/* Encrypt it.
 * However, we now encrypt all bytes, so the result here could contain
 * a NUL byte. Hence we need to get (and store) the length first, and
 * remember to use that in any copying (including elsewhere in uemacs).
 * Also, we may repeat the string such that it fills more of the buffer.
 * Without this !!!! 1111 AAAA QQQQ aaaa qqqq all produce the same result.
 */
    case CRYPT_RAW:     /* Do nothing */
        break;
    case CRYPT_FILL63: {
        char keycop[NKEY];      /* GGR */
        int lcop = strlen(ukey);
        strcpy(keycop, ukey);
        while (lcop < 63) {
            strcpy(keycop, ukey);   /* Can't strcat to itself, so... */
            strcat(ukey, keycop);
            lcop += lcop;
        }
        break; }
    }

/* Set the length (this will update either the global or the buffer
 * value, according to the *klenp setting above) and encrypt the key
 * on itself.
 */
    *klenp = strlen(ukey);
    myencrypt(NULL, 0);
    myencrypt(ukey, *klenp);
    return TRUE;
}
