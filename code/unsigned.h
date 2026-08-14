/* unsinged.h
 * 
 * Ensure that the compilation is using unsigned chars.
 */

#ifndef UNSIGNED_H_
#define UNSIGNED_H_

#if !__CHAR_UNSIGNED__
#error( "nuEmacs MUST be built with -funsigned-char");
#endif

#endif
