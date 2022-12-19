/**
 * @file xyz.c
 * @date Apr 30, 2021
 * @author Matthew Hagerty
 */


#include <stdint.h>     // uintXX_t, intXX_t, UINTXX_MAX, INTXX_MAX, etc.
#include <stdbool.h>    // true, false
#include "xyz.h"


/**
 * Finds the last segment of a string given a separator.
 *
 * A typical use would be for getting the last part of a path.
 *
 * Examples for a separator of '/':
 *  /                  -> /
 *  ./                 -> ./
 *  /usr/local/bin/    -> bin/
 *  /usr/local/bin     -> bin
 *  /usr/local/hello.c -> hello.c
 *  /program           -> program
 *  somefile.txt       -> somefile.txt
 *
 * @param[in] nt_str Pointer to terminated string.
 * @param[in] sep    Separator character.
 *
 * @return Pointer to the last segment of the string.
 */
const c8 *
xyz_str_lastseg(const c8 *nt_str, c8 sep)
{
   const c8 *c   = nt_str + 1;
   const c8 *cp  = nt_str;
   const c8 *seg = nt_str;

   if ( seg != NULL && *c != XYZ_NTERM && *cp != XYZ_NTERM )
   {
      for ( ; *c != XYZ_NTERM ; cp = c, c++ )
      {
         if ( *cp == sep && *c != sep ) {
            seg = c;
         }
      }
   }

   return seg;
}
// xyz_str_lastseg()


/**
 * Tries to find the last part of a path.
 *
 * Surprisingly difficult to do for multi-platform support since Windows
 * can have '/' in a filename, and Unix uses '\' for escaping characters
 * in a path.
 *
 * This function takes the least-effort approach and tries finding the last
 * segment of the input using both common path separators, and assumes the
 * shortest result is the right one.
 *
 * @param filepath
 *
 * @return The last segment of the path.
 */
const c8 *
xyz_path_lastpart(const c8 *filepath)
{
   const c8 *str1 = xyz_str_lastseg(filepath, XYZ_UNIX_PSEP);
   const c8 *str2 = xyz_str_lastseg(filepath, XYZ_WIN_PSEP);
   return (str1 > str2) ? str1 : str2;
}
// xyz_path_lastpart()


// ==========================================================================
//
// Single reader-writer lock-free ring buffer access manager (RBAM)
//
// ==========================================================================

// The actual data buffer itself is not contained, encapsulated, allocated,
// freed, or otherwise managed by this code.  Only indexes for reading and
// writing are managed, and whether the buffer is full or empty.  It is up
// to the calling code to behave.
//
// The rd and wr fields are the indexes of where data should be read and
// written, respectively.
//
// To keep the management lock-free and simple, the full-condition is true when
// there are size-1 elements in the buffer.  For example, a 100-element buffer
// can only ever contain 99 elements when it is full.


// Define XYZ_RBAM_EN_STATS to enable the used and free calculations.
#define XYZ_RBAM_EN_STATS 1


/**
 * Initialize an RBAM structure for first use.
 *
 * @param[in]   rbam    Pointer to the xyz_rbam structure for the RBAM.
 * @param[in]   dim     The number of elements in the buffer being managed, must be >= 2.
 *
 * @return true if dim >= 2, otherwise false.
 */
bool
xyz_rbam_init(xyz_rbam *rbam, uint32_t dim)
{
    if ( dim < 2 ) { return false; }
    rbam->rd = 0;
    rbam->wr = 0;
    rbam->next = 1;
    rbam->dim = dim;
    rbam->used = 0;
    rbam->free = dim - 1;

    return true;
}
// xyz_rbam_init()


/**
 * Call to indicate the current element has been written.
 *
 * If the buffer is not full, then the write index (wr) points to the current
 * data element to be written.  Data should be written to the wr index, then
 * this function called to make the data available for reading.  The wr index
 * will be updated to the next available index for writing.
 *
 * Example use:
 *
 * if ( RBAM_FREE(rbam) ) {
 *   // Write data.
 *   data[rbam->wr].field = 1;
 *
 *   // Indicate the data has been written.
 *   rbam_write(rbam);
 * }
 *
 * @param[in]   rbam    Pointer to the xyz_rbam structure for the RBAM.
 */
void
xyz_rbam_write(xyz_rbam *rbam)
{
    if ( XYZ_RBAM_FREE(rbam) )
    {
        // The buffer is not full, so the location pointed to by 'next' is
        // available for writing.
        rbam->wr = rbam->next;
        rbam->next = (rbam->next >= (rbam->dim - 1) ? 0 : rbam->next + 1);
    }

    // Calculate the buffer use.
    #ifdef XYZ_RBAM_EN_STATS
    u32 rd = rbam->rd;
    rbam->used = (rbam->wr >= rd ? rbam->wr - rd : (rbam->dim - rd) + rbam->wr);
    rbam->free = rbam->dim - rbam->used - 1;
    #endif

    return;
}
// xyz_rbam_write()


/**
 * Call to indicate that the current element has been read (past-tense).
 *
 * If the buffer is not empty, then the current read index (rd) points to
 * the current element to read.  Use (i.e. read) the data, then call this
 * function to indicate that the data has been consumed.  In other words,
 * calling this function is a post-reading activity.  If the buffer is not
 * empty then rd will be updated to the next available index for reading.
 *
 * Example use:
 *
 * while ( RBAM_MORE(rbam) ) {
 *   // Display data.
 *   printf("%d\n", data[rbam->rd].field);
 *
 *   // Done reading, consume the current element.
 *   rbam_read(rbam);
 * }
 *
 * @param[in]   rbam    Pointer to the xyz_rbam structure for the RBAM.
 */
void
xyz_rbam_read(xyz_rbam *rbam)
{
    if ( XYZ_RBAM_MORE(rbam) ) {
        // The buffer is not empty, update the read index.
        rbam->rd = (rbam->rd >= (rbam->dim - 1) ? 0 : rbam->rd + 1);
    }

    // Calculate the buffer use.
    #ifdef XYZ_RBAM_EN_STATS
    u32 wr = rbam->wr;
    rbam->used = (wr >= rbam->rd ? wr - rbam->rd : (rbam->dim - rbam->rd) + wr);
    rbam->free = rbam->dim - rbam->used - 1;
    #endif

    return;
}
// xyz_rbam_read()


/**
 * Drains the buffer (makes the buffer empty).
 *
 * @param[in]   rbam    Pointer to the xyz_rbam structure for the RBAM.
 *
 * @return The number of elements drained.
 */
u32
xyz_rbam_drain(xyz_rbam *rbam)
{
    uint32_t wr = rbam->wr;
    uint32_t drained = (wr >= rbam->rd ? wr - rbam->rd : (rbam->dim - rbam->rd) + wr);
    rbam->rd = rbam->wr; // empty condition.
    rbam->used = 0;
    rbam->free = rbam->dim - 1;

    return drained;
}
// xyz_rbam_drain()



// TODO Finish this basic idea, or dump it...
#if 0
/**
 * Initialize a meta data type.
 *
 * If the type requires malloc allocation
 *
 *
 * @param mt
 * @param type
 * @param alloc
 *
 * @return
 */
s32 xyz_meta_init(xyz_meta *mt, u16 type, u16 alloc, u32 dim, void *value)
{
   s32 rtn = XYZ_ERR;

   switch ( type )
   {
   case XYZ_META_T_ASCII_CHAR :    ///< ASCII padded character.
   case XYZ_META_T_ASCII_VARCHAR : ///< ASCII variable character.
   case XYZ_META_T_UTF8_CHAR :     ///< UTF8 padded character.
   case XYZ_META_T_UTF8_VARCHAR :  ///< UTF8 variable character.

      mt->format = XYZ_META_F_POINTER;
      mt->type = type;
      mt->byte_dim = dim;
      mt->unit_dim = dim; // this is wrong for anything other than ASCII.

      if ( alloc == XYZ_META_P_STATIC )
      {
         if ( value != NULL ) {
            mt->buf.vp = value;
         } else {
            // TODO error.
            break;
         }
      }

      else if ( alloc == XYZ_META_P_DYNAMIC )
      {

      }


      rtn = XYZ_OK;
      break;

   default :
      // TODO unknown.
      break;
   }

   return rtn;
}
// xyz_meta_init()
#endif


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
