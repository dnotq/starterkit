/**
 * @file xyz.c
 * @date Apr 30, 2021
 * @author Matthew Hagerty
 */


#include "xyz.h"


/**
 * Finds the last segment of a path.
 *
 * Examples:
 *  /                  -> /
 *  /usr/local/bin/    -> bin/
 *  /usr/local/bin     -> bin
 *  /usr/local/hello.c -> hello.c
 *  somefile.txt       -> somefile.txt
 *
 * @param[in] filepath Pointer to terminated path/file string.
 *
 * @return Pointer to the last segment of the path.
 */
const c8 *
xyz_path_lastpart(const c8 *filepath)
{
   const c8 *f_start = filepath;
   const c8 *f_prev = filepath;
   const c8 *f_end = filepath;

   for ( ; *f_end != XYZ_NTERM ; f_end++ )
   {
      if ( *f_end == '/' || *f_end == '\\' ) {
         f_prev = f_start;
         f_start = f_end;
      }
   }

   // Advance past any previous separator.
   if ( f_prev < f_start && (*f_prev == '/' || *f_prev == '\\') ) {
      f_prev++;
   }

   if ( *f_start == '/' || *f_start == '\\' ) {
      f_start++;
   }

   // Use the previous segment if the last character is a separator.
   if ( f_start >= f_end && f_prev < f_start ) {
      f_start = f_prev;
   }

   return f_start;
}
// xyz_path_lastpart()


// ==========================================================================
//
// Single reader-writer lock-free ring buffer access manager (RBAM)
//
// ==========================================================================

// The actual data buffer itself is not contained, encapsulated, allocated,
// freed, or otherwise managed by this code.  Only access to reading or
// writing is managed, and whether the buffer is full or empty.  It is up
// to the calling code to behave.
//
// The rd and wr fields are the indexes of where data should be read and
// written, respectively.
//
// To keep the management lock-free and simple, the full-condition is true when
// there are size-1 elements in the buffer.  For example, a 100-element buffer
// can only ever contain 99 elements when it is full.


/**
 * Initialize an RBAM structure for first use.
 *
 * @param[in] rbam  pointer to the xyz_rbam structure for the RBAM.
 * @param[in] size  the number of elements in the data structure being
 *                  managed by the RBAM, must be >= 2.
 *
 * @return XYZ_TRUE if initialization succeeded, otherwise XYZ_FALSE if size
 *         is less than 2.
 */
u32
xyz_rbam_init(xyz_rbam *rbam, u32 dim)
{
   if ( dim < 2 ) { return XYZ_FALSE; }
   rbam->dim = dim;
   rbam->rd = 0;
   rbam->wr = 0;
   rbam->next = 1;
   rbam->used = 0;
   rbam->free = dim - 1;

   return XYZ_TRUE;
}
// xyz_rbam_init()


/**
 * Get the buffer index value that comes after the specified index.
 *
 * @param[in] rbam  pointer to the xyz_rbam structure for the RBAM.
 * @param index     specified known index value.
 *
 * @return The index value after the specified index.
 */
u32
xyz_rbam_next(xyz_rbam *rbam, u32 index)
{
   return (index >= (rbam->dim - 1) ? 0 : index + 1);
}
// xyz_rbam_next()


/**
 * Get the buffer index value that comes before the specified index.
 *
 * @param[in] rbam  pointer to the xyz_rbam structure for the RBAM.
 * @param index     specified known index value.
 *
 * @return The index value prior-to the specified index.
 */
u32
xyz_rbam_prev(xyz_rbam *rbam, u32 index)
{
   return (index == 0 ? (rbam->dim - 1) : index - 1);
}
// xyz_rbam_prev()


//
// Writer Functions
//


/**
 * Checks if a buffer is full.
 *
 * @param[in] rbam  pointer to the xyz_rbam structure for the RBAM.
 *
 * @return XYZ_TRUE if the buffer is full, otherwise XYZ_FALSE.
 */
u32
xyz_rbam_is_full(xyz_rbam *rbam)
{
   return (rbam->next == rbam->rd ? XYZ_TRUE : XYZ_FALSE);
}
// xyz_rbam_is_full()


/**
 * Call to indicate the current element has been written.
 *
 * @param[in] rbam  pointer to the xyz_rbam structure for the RBAM.
 *
 * If the buffer is not full, then the current write location (wr) points to
 * the current element to be written.  Data should be written to the wr index,
 * then call this function to make the data available for reading.  This is a
 * post-write activity.
 *
 * Example use:
 *
 * if ( xyz_rbam_is_full(rbam) == XYZ_FALSE ) {
 *   // Write data.
 *   data[rbam->wr].field = 1;
 *
 *   // Indicate the data has been written.
 *   xyz_rbam_write(rbam);
 * }
 *
 * @return XYZ_TRUE if the write was valid, XYZ_FALSE if the buffer is full.
 */
u32
xyz_rbam_write(xyz_rbam *rbam)
{
   if ( xyz_rbam_is_full(rbam) == XYZ_TRUE ) { return XYZ_FALSE; }

   // The buffer is not full, so the location pointed to by 'next' is
   // available for writing.
   rbam->wr = rbam->next;
   rbam->next = xyz_rbam_next(rbam, rbam->next);

   // Calculate the buffer use.
   u32 rd = rbam->rd;
   rbam->used = (rbam->wr >= rd ? rbam->wr - rd : (rbam->dim - rd) + rbam->wr);
   rbam->free = rbam->dim - rbam->used - 1;

   return XYZ_TRUE;
}
// xyz_rbam_write()


//
// Reader Functions
//


/**
 * Checks if a buffer is empty.
 *
 * @param[in] rbam  pointer to the xyz_rbam structure for the RBAM.
 *
 * @return XYZ_TRUE if the buffer is empty, otherwise XYZ_FALSE.
 */
u32
xyz_rbam_is_empty(xyz_rbam *rbam)
{
   return (rbam->rd == rbam->wr ? XYZ_TRUE : XYZ_FALSE);
}
// xyz_rbam_is_empty()


/**
 * Call to indicate that the current element has been read (past-tense).
 *
 * If the buffer is not empty, then the current read location (rd) points to
 * the current element to read.  Use (i.e. read) the data, then call this
 * function to indicate that the data has been consumed.  In other words,
 * calling this function is a post-reading activity.  If the buffer is not
 * empty then rd will be updated to the next available entry for reading.
 *
 * Example use:
 *
 * while ( xyz_rbam_is_empty(rbam) == XYZ_FALSE ) {
 *   // Display data.
 *   printf("%d\n", data[rbam->rd].field);
 *
 *   // Done reading, consume the current element.
 *   xyz_rbam_read(rbam);
 * }
 *
 * @param[in] rbam  pointer to the xyz_rbam structure for the RBAM.
 *
 * @return XYZ_TRUE if there is more data in the buffer, XYZ_FALSE if the
 *         buffer is empty.
 */
u32
xyz_rbam_read(xyz_rbam *rbam)
{
   if ( xyz_rbam_is_empty(rbam) == XYZ_TRUE ) { return XYZ_FALSE; }

   // The buffer is not empty, update the read index.
   rbam->rd = xyz_rbam_next(rbam, rbam->rd);

   // Calculate the buffer use.
   u32 wr = rbam->wr;
   rbam->used = (wr >= rbam->rd ? wr - rbam->rd : (rbam->dim - rbam->rd) + wr);
   rbam->free = rbam->dim - rbam->used - 1;

   return XYZ_TRUE;
}
// xyz_rbam_read()


/**
 * Drains the buffer (makes the buffer empty).
 *
 * @param[in] rbam  pointer to the xyz_rbam structure for the RBAM.
 *
 * @return The number of elements drained.
 */
u32
xyz_rbam_drain(xyz_rbam *rbam)
{
   u32 wr = rbam->wr;
   u32 drained = (wr >= rbam->rd ? wr - rbam->rd : (rbam->dim - rbam->rd) + wr);
   rbam->rd = rbam->wr;
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
