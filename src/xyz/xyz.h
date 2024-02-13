/**
 * Generic defines, helpers, and utilities.
 *
 * @file   xyz.h
 * @author Matthew Hagerty
 * @date   April 5, 2020
 *
 * Font check: 0O1lLi
 */

#ifndef XYZ_H_
#define XYZ_H_

#include <stdlib.h>     // malloc, calloc, realloc, free
#include <stdint.h>		// uintXX_t, intXX_t, UINTXX_MAX, INTXX_MAX, etc.
#include <stdbool.h>    // true, false

// Avoid C++ name-mangling for C functions.
#ifdef __cplusplus
extern "C" {
#endif


// Nice to have short-name types.  These types break the XYZ_ prefix rule in
// exchange for a shorter type name (which is half the reason for using them).
//
// The 'char' type in C is undefined as to whether it is signed or unsigned, and
// is left up to the implementation.  This is unfortunate, especially when
// interfacing with libc functions.

typedef char        c8;     ///< Implementation-defined 8-bit type.
typedef int8_t      s8;     ///< Signed 8-bit type.
typedef int16_t     s16;    ///< Signed 16-bit type.
typedef int32_t     s32;    ///< Signed 32-bit type.
typedef int64_t     s64;    ///< Signed 64-bit type.
typedef uint8_t     u8;     ///< Unsigned 8-bit type.
typedef uint16_t    u16;    ///< Unsigned 16-bit type.
typedef uint32_t    u32;    ///< Unsigned 32-bit type.
typedef uint64_t    u64;    ///< Unsigned 64-bit type.
typedef float       f32;    ///< 32-bit binary floating point type.
typedef double      f64;    ///< 64-bit binary floating point type.


/// Get the sizeof a structure member without having a structure variable.
#define sizeof_sm(s, m) sizeof((((s *)0)->m))

/// Error Block.
#define XYZ_EB_START do {
#define XYZ_EB_BREAK break;
#define XYZ_EB_RETRY continue;
#define XYZ_EB_END break; } while(1);

/// string terminator (to avoid confusion because NUL != NULL (look it up)).
#define XYZ_NTERM '\0'

/// Current File and Line.
#define XYZ_CFL xyz_basename(__FILE__,NULL), __LINE__


/// TODO placeholder, replace with allocator wrapper functions.
#define xyz_malloc(sz) malloc(sz)
#define xyz_calloc(num,sz) calloc(num,sz)
#define xyz_realloc(ptr,sz) realloc(ptr,sz)
#define xyz_free(ptr) free(ptr)


/// Copy a zero-terminated string, always terminating the destination.
size_t xyz_strlcpy(c8 *dst, const c8 *src, size_t dstsize);

/// Finds the base file name of a file path.
const c8 * xyz_basename(const c8 *ztp, size_t *len);

/// Wrapper to provide a consistent strerror_r() function.
void xyz_strerror(s32 errnum, c8 *buf, size_t len);


#ifdef __cplusplus
}
#endif
#endif /* XYZ_H_ */

/*
------------------------------------------------------------------------------
This software is available under 2 licenses -- choose whichever you prefer.
------------------------------------------------------------------------------
ALTERNATIVE A - MIT License
Copyright (c) 2020 Matthew Hagerty
Permission is hereby granted, free of charge, to any person obtaining a copy of
this software and associated documentation files (the "Software"), to deal in
the Software without restriction, including without limitation the rights to
use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies
of the Software, and to permit persons to whom the Software is furnished to do
so, subject to the following conditions:
The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.
THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
------------------------------------------------------------------------------
ALTERNATIVE B - Public Domain (www.unlicense.org)
This is free and unencumbered software released into the public domain.
Anyone is free to copy, modify, publish, use, compile, sell, or distribute this
software, either in source code form or as a compiled binary, for any purpose,
commercial or non-commercial, and by any means.
In jurisdictions that recognize copyright laws, the author or authors of this
software dedicate any and all copyright interest in the software to the public
domain. We make this dedication for the benefit of the public at large and to
the detriment of our heirs and successors. We intend this dedication to be an
overt act of relinquishment in perpetuity of all present and future rights to
this software under copyright law.
THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
------------------------------------------------------------------------------
*/
