/* unsinged.h
 * 
 * Ensure that the compilation is using unsigned chars.
 */

#ifndef UNSIGNED_H_
#define UNSIGNED_H_

/* NOTE that this does need to be _Static_assert.
 * _static_assert or static_asserts fails.
 */
_Static_assert((char)-1 > 0, "nuEmacs must be built with -funsigned-char");

#endif
