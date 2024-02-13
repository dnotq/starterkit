/**
 * @file cidx.h
 * @date Apr 30, 2022
 * @author Matthew Hagerty
 *
 * Requires:
 *
 *   MSVC: -experimental:c11atomics and -std:c11 compiler options, until such
 *   a time that atomic differences are resolved between C and C++, and they
 *   are not considered "experimental" by Microsoft.
 *
 * Build options:
 *
 *   CIDX_IMPLEMENTATION
 *   MUST BE DEFINED prior to including cidx.h in ONE source file in a
 *   project.  All other code can just include the cidx.h as usual.
 *
 *   CIDX_NO_ATOMIC
 *   Define if the non-atomic version of the code is desired.  Must be defined
 *   in all source files prior to including cidx.h.  It is generally NOT
 *   recommended to define this and should only be considered in a system with
 *   one single-core CPU.
 *
 * ====
 *
 * Single-file header + source is a side effect of allowing the same header to
 * be used from C and C++ in the same code base.
 *
 * Circular index: manages single reader / writer access to an array of elements.
 *
 * Memory-consistency protection on a single-core CPU it is not needed.  On a
 * multi-core CPU or multi-processor system, some form of memory consistency is
 * still needed to ensure correct operation due to compilers and CPUs rearranging
 * memory accesses, CPU-local caches, etc..
 *
 * This code is NOT a library and is not intended to be used as a library.  It
 * is compiled into the host code and all the internals are available for
 * tampering with.
 *
 * C and C++ atomics are not implemented the same way and ended up not really
 * being compatible (add snarky remark here).  If the link still works, here
 * is a summary of the problem:
 *
 * @see https://www.open-std.org/JTC1/SC22/WG14/www/docs/n2741.htm
 *
 * This is all supposed to be fixed in C++23, but so far only Clang "just works",
 * g++ and msvc need hacks.
 */


#ifndef CIDX_H_
#define CIDX_H_

// Define CIDX_IMPLEMENTATION in a single C code compilation unit before
// including this header.

// Define CIDX_NO_ATOMIC in the calling code if memory protection is not
// required.  A single processes or a single thread that is both the reader and
// writer does not need to use the atomic variables.  If the reader and writer
// are in separate threads or the data structure is in shared memory, then
// leave the atomic option enabled.

#ifdef CIDX_NO_ATOMIC
#undef CIDX_USE_ATOMIC
#else
#define CIDX_USE_ATOMIC
#endif


#ifdef CIDX_USE_ATOMIC

#ifdef __cplusplus
#include <atomic>       // atomic types in C++
// This is required for C++ since all `atomic_*` type aliases are in
// namespace `std`, as in `std::atomic_bool`, for instance, instead of
// just `atomic_bool`.
// - clang++ "just works" without this hack (can just use the C include).
// - g++ requires this ifdef and hack.
// - msvc requires this ifdef aand hack, along with specific flags to not break the C side.
using atomic_size_t = std::atomic_size_t;
#else
// https://devblogs.microsoft.com/cppblog/c11-atomics-in-visual-studio-2022-version-17-5-preview-2/
// msvc as of June 2023 requires compiler flags: -experimental:c11atomics -std:c11
// or it will blow up trying to include this header.
#include <stdatomic.h>  // atomic types
#endif

#endif /* Using atomics */


#include <stdint.h>     // uintXX_t, intXX_t, UINTXX_MAX, INTXX_MAX, etc.
#include <stdbool.h>    // true, false
#include "xyz.h"


// Avoid C++ name-mangling for C functions.
#ifdef __cplusplus
extern "C" {
#endif


// ==========================================================================
//
// Circular index for single reader-writer.
//
// ==========================================================================


/// Circular index structure.
typedef struct t_cidx_s {
    size_t          wr;     ///< Index for writing elements.
    size_t          rd;     ///< Index for reading elements.
    #ifdef CIDX_USE_ATOMIC
    atomic_size_t   fre;    ///< Number of free elements.
    #else
    size_t          fre;    ///< Number of free elements.
    #endif
    size_t          max;    ///< Maximum number of elements.
    size_t          esz;    ///< Size of an element (in bytes).
    void           *ary;    ///< Pointer to the array of elements.
} cidx_s;


/**
 * Initialize a cidx_s structure for first use.
 *
 * @param[in]  ci       Pointer to the cidx_s structure.
 * @param[in]  esz      Size of an element (in bytes).
 * @param[in]  ary      Pointer to the array of elements.
 * @param[in]  max      Maximum number of elements the array can hold, must be >= 2.
 *
 * @return true if max >= 2, otherwise false.
 */
bool cidx_init(cidx_s *ci, size_t esz, void *ary, size_t max);

/**
 * Check if the array is full.
 *
 * @param[in] ci    Pointer to a circular index struct.
 *
 * @return true if the array is full, otherwise false.
 */
bool cidx_isfull(cidx_s *ci);

/**
 * Check if the array is empty.
 *
 * @param[in] ci    Pointer to a circular index struct.
 *
 * @return true if the array is empty, otherwise false.
 */
bool cidx_isempty(cidx_s *ci);

/**
 * The number of elements available for reading.
 *
 * @param[in] ci    Pointer to a circular index struct.
 *
 * @return The number of elements available for reading.
 */
size_t cidx_used(cidx_s *ci);

/**
 * The number of elements available for writing.
 *
 * @param[in] ci    Pointer to a circular index struct.
 *
 * @return The number of elements available for writing.
 */
size_t cidx_free(cidx_s *ci);

/**
 * Get the next index, accounting for index roll-over.
 *
 * Helpful for iterating the array of elements.  Does not effect reading,
 * writing, or adjust any internal values.
 *
 * @param[in] ci    Pointer to a circular index struct.
 * @param[in] idx   Current index value.
 *
 * @return The next index value.
 */
size_t cidx_next(cidx_s *ci, size_t idx);

/**
 * Get the previous index, accounting for index roll-over.
 *
 * Helpful for iterating the array of elements.  Does not effect reading,
 * writing, or adjust any internal values.
 *
 * @param[in] ci    Pointer to a circular index struct.
 * @param[in] idx   Current index value.
 *
 * @return The previous index value.
 */
size_t cidx_prev(cidx_s *ci, size_t idx);

/**
 * Current read element.
 *
 * @param[in] ci    Pointer to a circular index struct.
 *
 * @return A pointer to the current array element for reading.
 */
void * cidx_re(cidx_s *ci);

/**
 * Current write element.
 *
 * @param[in] ci    Pointer to a circular index struct.
 *
 * @return A pointer to the current array element for writing.
 */
void * cidx_we(cidx_s *ci);

/**
 * Commits an element in the array, adjusting the write index and making the
 * element available for reading.
 *
 * Should be used after the element has been written by the caller.
 *
 * if ( false == cidx_isfull(ci) ) {
 *    ele_s *ele = (ele_s *)cidx_we(ci);
 *    ele->x = 10;
 *    ele->y = 20;
 *    cidx_commit(ci);
 * }
 *
 * @param[in] ci    Pointer to a circular index struct.
 *
 * @return True if the element was committed, otherwise false if the array
 *         is full.
 */
bool cidx_commit(cidx_s *ci);

/**
 * Consumes an element in the array, adjusting the read index and making the
 * element available for over-writing.
 *
 * Should be used after the element has been read by the caller.
 *
 * while ( false == cidx_isempty(ci) ) {
 *    ele_s *ele = (ele_s *)cidx_re(ci);
 *    plot(ele->x, ele->y);
 *    cidx_consume(ci);
 * }
 *
 * @param[in] ci    Pointer to a circular index struct.
 *
 * @return True if the element was consumed, otherwise false if the array
 *         is empty.
 */
bool cidx_consume(cidx_s *ci);

/**
 * Effectively drains (marks as read) the array of all elements.
 *
 * @param[in] ci    Pointer to a circular index struct.
 *
 * @return size_t   The number of elements drained.
 */
size_t cidx_drain(cidx_s *ci);


#ifdef __cplusplus
}
#endif
#endif /* CIDX_H_ */

#ifdef CIDX_IMPLEMENTATION

// ==========================================================================
//
// Circular index for single reader-writer.
//
// ==========================================================================

// The actual array itself is not allocated or freed by this code, only indexes
// for reading and writing are managed, and whether the array is full or empty.
// It is up to the calling code to behave.


/**
 * Initialize a cidx_s structure for first use.
 *
 * @param[in]  ci       Pointer to the cidx_s structure.
 * @param[in]  esz      Size of an element (in bytes).
 * @param[in]  ary      Pointer to the array of elements.
 * @param[in]  max      Maximum number of elements the array can hold, must be >= 2.
 *
 * @return true if max >= 2, otherwise false.
 */
bool
cidx_init(cidx_s *ci, size_t esz, void *ary, size_t max) {

    if ( max < 2 ) {
        return false;
    }

    ci->wr = 0;
    ci->rd = 0;
    #ifdef CIDX_USE_ATOMIC
    atomic_store(&ci->fre, max);
    #else
    ci->fre = max;
    #endif
    ci->max = max;
    ci->esz = esz;
    ci->ary = ary;

    return true;
}
// cidx_init()


/**
 * Check if the array is full.
 *
 * @param[in] ci    Pointer to a circular index struct.
 *
 * @return true if the array is full, otherwise false.
 */
bool
cidx_isfull(cidx_s *ci) {
    #ifdef CIDX_USE_ATOMIC
    return 0 == atomic_load(&ci->fre) ? true : false;
    #else
    return 0 == ci->fre ? true : false;
    #endif
}
// cidx_isfull()


/**
 * Check if the array is empty.
 *
 * @param[in] ci    Pointer to a circular index struct.
 *
 * @return true if the array is empty, otherwise false.
 */
bool
cidx_isempty(cidx_s *ci) {
    #ifdef CIDX_USE_ATOMIC
    size_t fre = atomic_load(&ci->fre);
    return fre == ci->max ? true : false;
    #else
    return ci->fre == ci->max ? true : false;
    #endif
}
// cidx_isempty()


/**
 * The number of elements available for reading.
 *
 * @param[in] ci    Pointer to a circular index struct.
 *
 * @return The number of elements available for reading.
 */
size_t
cidx_used(cidx_s *ci) {
    #ifdef CIDX_USE_ATOMIC
    size_t fre = atomic_load(&ci->fre);
    return ci->max - fre;
    #else
    return ci->max - ci->fre;
    #endif
}
// cidx_used()


/**
 * The number of elements available for writing.
 *
 * @param[in] ci    Pointer to a circular index struct.
 *
 * @return The number of elements available for writing.
 */
size_t
cidx_free(cidx_s *ci) {
    #ifdef CIDX_USE_ATOMIC
    return atomic_load(&ci->fre);
    #else
    return ci->fre;
    #endif
}
// cidx_free()


/**
 * Get the next index, accounting for index roll-over.
 *
 * Helpful for iterating the array of elements.  Does not effect reading,
 * writing, or adjust any internal values.
 *
 * @param[in] ci    Pointer to a circular index struct.
 * @param[in] idx   Current index value.
 *
 * @return The next index value.
 */
size_t
cidx_next(cidx_s *ci, size_t idx) {
    return ((idx + 1) % ci->max);
}
// cidx_next()


/**
 * Get the previous index, accounting for index roll-over.
 *
 * Helpful for iterating the array of elements.  Does not effect reading,
 * writing, or adjust any internal values.
 *
 * @param[in] ci    Pointer to a circular index struct.
 * @param[in] idx   Current index value.
 *
 * @return The previous index value.
 */
size_t
cidx_prev(cidx_s *ci, size_t idx) {
    return ((0 == idx) ? ci->max - 1 : idx - 1);
}
// cidx_prev()


/**
 * Current read element.
 *
 * @param[in] ci    Pointer to a circular index struct.
 *
 * @return A pointer to the current array element for reading.
 */
void *
cidx_re(cidx_s *ci) {
    return (u8 *)ci->ary + (ci->rd * ci->esz);
}
// cidx_re()


/**
 * Current write element.
 *
 * @param[in] ci    Pointer to a circular index struct.
 *
 * @return A pointer to the current array element for writing.
 */
void *
cidx_we(cidx_s *ci) {
    return (u8 *)ci->ary + (ci->wr * ci->esz);
}
// cidx_we()


/**
 * Commits an element in the array, adjusting the write index and making the
 * element available for reading.
 *
 * Should be used after the element has been written by the caller.
 *
 * if ( false == cidx_isfull(ci) ) {
 *    ele_s *ele = (ele_s *)cidx_we(ci);
 *    ele->x = 10;
 *    ele->y = 20;
 *    cidx_commit(ci);
 * }
 *
 * @param[in] ci    Pointer to a circular index struct.
 *
 * @return True if the element was committed, otherwise false if the array
 *         is full.
 */
bool
cidx_commit(cidx_s *ci) {
    #ifdef CIDX_USE_ATOMIC
    if ( 0 == atomic_load(&ci->fre) ) {
        return false;
    }

    ci->wr = (ci->wr + 1) % ci->max;
    atomic_fetch_sub(&ci->fre, 1);

    #else
    if ( 0 == ci->fre ) {
        return false;
    }

    ci->wr = (ci->wr + 1) % ci->max;
    ci->fre--;
    #endif

    return true;
}
// cidx_commit()


/**
 * Consumes an element in the array, adjusting the read index and making the
 * element available for over-writing.
 *
 * Should be used after the element has been read by the caller.
 *
 * while ( false == cidx_isempty(ci) ) {
 *    ele_s *ele = (ele_s *)cidx_re(ci);
 *    plot(ele->x, ele->y);
 *    cidx_consume(ci);
 * }
 *
 * @param[in] ci    Pointer to a circular index struct.
 *
 * @return True if the element was consumed, otherwise false if the array
 *         is empty.
 */
bool
cidx_consume(cidx_s *ci) {
    #ifdef CIDX_USE_ATOMIC
    if ( ci->max == atomic_load(&ci->fre) ) {
        return false;
    }

    ci->rd = (ci->rd + 1) % ci->max;
    atomic_fetch_add(&ci->fre, 1);

    #else
    if ( ci->max == ci->fre ) {
        return false;
    }

    ci->rd = (ci->rd + 1) % ci->max;
    ci->fre++;
    #endif

    return true;
}
// cidx_consume()


/**
 * Effectively drains (consumes) the array of all elements.
 *
 * @note Called by the reader.
 *
 * @param[in] ci    Pointer to a circular index struct.
 *
 * @return size_t   The number of elements drained.
 */
size_t
cidx_drain(cidx_s *ci) {
    #ifdef CIDX_USE_ATOMIC
    size_t fre = atomic_exchange(&ci->fre, ci->max);
    size_t drained = ci->max - fre;
    #else
    size_t drained = ci->max - ci->fre;
    ci->fre = ci->max;
    #endif

    ci->rd = (ci->rd + drained) % ci->max;
    return drained;
}
// cidx_drain()

#endif /* CIDX_IMPLEMENTATION */

/*
------------------------------------------------------------------------------
This software is available under 2 licenses -- choose whichever you prefer.
------------------------------------------------------------------------------
ALTERNATIVE A - MIT License
Copyright (c) 2022 Matthew Hagerty
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
