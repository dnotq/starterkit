/**
 * @file xyz.c
 * @date Apr 30, 2021
 * @author Matthew Hagerty
 */


#include <stdio.h>      // strerror_r
#include <string.h>     // strerror_r
#include <stdint.h>     // uintXX_t, intXX_t, UINTXX_MAX, INTXX_MAX, etc.
#include <stdbool.h>    // true, false
#include <errno.h>      // errno
#include "xyz.h"        // short types
#include "log.h"


/**
 * Copies a zero-terminated string, always terminating the destination.
 *
 * Similar to strncpy, however at most dstsize-1 bytes are copied to dst, always
 * zero-terminates dst, and does not pad dst in the case where dst is longer
 * than src.
 *
 * The length of src is returned so truncation can be easily determined.
 *
 * @param dst       Destination buffer.
 * @param src       Source buffer.
 * @param dstsize   The size of the destination buffer.
 *
 * @return The length of src.
 */
size_t
xyz_strlcpy(c8 *dst, const c8 *src, size_t dstsize) {

    size_t rtn = 0;

    if ( NULL == src ) {
        return rtn;
    }

    // If not destination, the length of src must be returned.
    if ( NULL != dst && 0 < dstsize ) {
        // Copy src to dst until end of src or dst.
        for ( ; rtn < (dstsize - 1) && XYZ_NTERM != src[rtn] ; rtn++ ) {
            dst[rtn] = src[rtn];
        }

        // Always zero-terminate dst.
        dst[rtn] = XYZ_NTERM;
    }

    // Must return the length of src.
    while ( XYZ_NTERM != src[rtn] ) {
        rtn++;
    }

    return rtn;
}
// xyz_strlcpy()


/**
 * Finds the base file name of a file path.
 *
 * Similar to:
 *
 *  basename = strrchr(path, '/') + 1
 *
 * However, this function also:
 *
 *  Considers both '/' and '\' path separators.
 *  Performs the '+ 1' adjustment internally.
 *  Optionally returns the length of the full path.
 *
 * Upon return basename will point to the filename part of the path, and
 * len will contain the length of the full path string.
 *
 *    basename = xyz_basename(path, &len);
 *
 * Same as above without returning the full path string length.
 *
 *    basename = xyz_basename(path, NULL);
 *
 * Examples:
 *  /                  -> null string, len=1
 *  ./                 -> null string, len=2
 *  /usr/local/bin/    -> null string, len=15
 *  /usr/local/bin     -> bin, len=14
 *  /usr/local/hello.c -> hello.c, len=18
 *  /program           -> program, len=8
 *  somefile.txt       -> somefile.txt, len=12
 *
 * @param[in]  ztp  Pointer to a zero terminated file path.
 * @param[out] len  Modified to the length of the full file path.
 *
 * @return Pointer to the base name of the file path, len is updated
 *         to the length of the path string.
 */
const c8 *
xyz_basename(const c8 *ztp, size_t *len) {

    const c8 *c   = ztp;    // current character
    const c8 *sep = ztp;    // last known separator

    if ( NULL != len ) {
        *len = 0;
    }

    if ( NULL == ztp || XYZ_NTERM == *c ) {
        return ztp;
    }

    // Step through the string until the terminator is found.
    for ( ; XYZ_NTERM != *c ; c++ ) {
        if ( '/' == *c || '\\' == *c ) {
            sep = c + 1;
        }
    }

    if ( NULL != len ) {
        *len = c - ztp;
    }

    return sep;
}
// xyz_basename()


/**
 * Wrapper to provide a consistent strerror_r() function.
 *
 * Wraps strerror_r() to work regardless of which type, XSI or GNU, is being
 * used by the system or compiler flags.  This function cannot fail and will
 * always properly return a zero-terminated buffer within the specified length.
 *
 * @see https://ae1020.github.io/fixing-strerror_r-posix-debacle/
 *
 * @param[in]    errnum System errno value to get a message for.
 * @param[inout] buf    Buffer to hold the errno message.
 * @param[in]    len    Length of message buffer.
 */
void
xyz_strerror(s32 errnum, c8 *buf, size_t len) {

    if ( NULL == buf || 0 == len ) {
        return;
    }

    // Set a sentinel that is not valid in UTF-8.
    buf[0] = -1; // The buffer is signed, -1 == 255.

    // Depending on the strerror_r in use, errno might be modified which totally
    // defeats the usability of strerror_r to begin with.
    // intptr_r is used as a common type that can deal with the XSI POSIX (int)
    // and GNU (char *) return values.
    s32 old_errno = errno;
    #if defined(__MINGW32__) || defined(__MINGW64__) || defined(_WIN32) || defined(_WIN64)
    intptr_t rtn = (intptr_t)strerror_s(buf, len, errnum);
    #else
    intptr_t rtn = (intptr_t)strerror_r(errnum, buf, len);
    #endif
    s32 new_errno = errno;

    // Decode what actually happened.
    //
    // XSI:
    //   0 on success
    //  >0 on error, positive "errno" value (since glibc 2.13)
    //  -1 on error, sets errno (prior to glibc 2.13)
    //     EINVAL - errno is invalid.
    //     ERANGE - buffer is not large enough for the error message.
    //
    // GNU: always successful
    //  pointer to buf, message up to len, zero-terminated
    //  pointer to system-static string (buf is unused)
    //

    // Common success cases first.

    // The GNU call never fails and thus can't return NULL, so a 0 (NULL) return
    // means a successful XSI call was made.
    if ( 0 == rtn ) {
        buf[len - 1] = XYZ_NTERM;
    }

    // If the buf address was returned, then GNU success.
    else if ( (intptr_t)buf == rtn ) {
        // Ensure the buffer was written.
        if ( -1 == buf[0] ) {
            snlogfmt(buf, len, "strerror_r() did not write an error message.");
        }
    }

    // A value other than the XSI error numbers, then GNU success.
    else if ( 0 < rtn && EINVAL < rtn && ERANGE < rtn ) {
        // GNU returned a pointer to the static message, so copy into buf to
        // provide a consistent caller interface.
        xyz_strlcpy(buf, (const c8 *)rtn, len);
    }

    // XSI error conditions.

    else if ( -1 == rtn || new_errno != old_errno ) {
        snlogfmt(buf, len, "strerror_r() failed and changed errno: %d", new_errno);
    }

    else if ( EINVAL == rtn ) {
        snlogfmt(buf, len, "strerror_r() invalid errno: %d", errnum);
    }

    else if ( ERANGE == rtn ) {
        snlogfmt(buf, len, "strerror_r() buf too small for errno message.");
    }

    else {
        s32 err = (s32)rtn;
        snlogfmt(buf, len, "strerror_r() unknown XSI return code: %d", err);
    }

    return;
}
// xyz_strerror()


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
