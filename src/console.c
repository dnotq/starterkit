/**
 * console.c
 *
 * @author Matthew Hagerty
 * @date April 23, 2023
 */

#include <stdbool.h>       // true, false

#include "program.h"       // progdata_s
#include "console.h"       // cons_s
#include "xyz.h"           // short types


// TODO
#if 0
    // Initialize the application console.
    pd->cons.buf = (c8 *)xyz_malloc(CONS_BUF_DIM);
    pd->cons.linelist = (consline_s *)xyz_calloc(CONS_LINELIST_DIM, sizeof(consline_s));
    pd->cons.mutex = SDL_CreateMutex();
    pd->cons.lockfailures = 0;

    if ( NULL == pd->cons.buf
    ||   NULL == pd->cons.linelist
    ||   NULL == pd->cons.mutex ) {
        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, pd->prg_name,
                "Cannot continue: The Console message buffer could not be allocated.", NULL);
        logfmt("Error: %s:%d The Console message buffer could not be allocated.\n", XYZ_CFL);
        XYZ_EB_BREAK
    }

    pd->cons.bufdim = CONS_BUF_DIM;
    xyz_rbam_init(&(pd->cons.rbam), CONS_LINELIST_DIM);
    pd->cons.out = out_cons;


    // Old cleanup code.
    if ( 0 != pd->cons.lockfailures ) {
        logfmt("Warning: Console mutex lock failure count: %u\n", pd->cons.lockfailures);
    }

    if ( NULL != pd->cons.buf ) {
        xyz_free(pd->cons.buf);
    }

    if ( NULL != pd->cons.linelist ) {
        xyz_free(pd->cons.linelist);
    }

    if ( NULL != pd->cons.mutex ) {
        SDL_DestroyMutex(pd->cons.mutex);
    }

#endif

// TODO
// Add pre and post function calls in log when setting a custom
// function.  Use for mutex and to know when a single call has
// started and stopped.
//
// Make the buffer writer look for newlines and write lines into
// the line-list based on newlines.
//
// If a log call does not end in a newline, the line and data can
// still be committed, but the next write will just append the
// current line instead of make a new line number.
//
// Reading should be interesting.
void
console_write(const char *buf, void *cdata, int32_t len) {

    if ( NULL == buf || NULL == cdata || 0 >= len ) {
        return;
    }
}


/**
 * Write to the console buffer.
 *
 * TODO Handle UTF8.
 *
 * @param[in] arg    Pointer to the program data structure.
 * @param[in] text   The text to write.
 * @param[in] len    The length of text.
 *
 * @return The value of len on success, otherwise less than len.
 */
u32
console_write(void *arg, const c8 *text, u32 len) {

    progdata_s *pd = (progdata_s *)arg;

    s32 locked = -1;

XYZ_EB_START

    if ( 0 == len || NULL == text || XYZ_NTERM === *text ) {
        len = 0;
        XYZ_EB_BREAK
    }

    // This is a console log and should be quick, so refuse lines longer than 4x
    // the maximum single line length.  If support for huge blobs of text is
    // desired, this is not the implementation to use.
    if ( (CONS_MAX_LINE * 4) < len ) {
        len = CONS_MAX_LINE * 4;
    }

    // The design is a ring-buffer list of line positions and lengths, and a
    // byte buffer holding the line data.
    //
    // Both the line list and buffer are fixed arrays, so which ever buffer
    // fills up first determines how many lines there are for display.


    locked = SDL_TryLockMutex(pd->cons.mutex);
    if ( 0 != locked )
    {
        pd->cons.lockfailures++;

        // Yield and wait the minimum time, and try the lock once more.
        SDL_Delay(1);
        locked = SDL_TryLockMutex(pd->cons.mutex);

        if ( 0 != locked ) {
            pd->cons.lockfailures++;
            XYZ_EB_BREAK
        }
    }


    u32 idx = 0;
    u32 linelen = 0;
    u32 start_idx = 0;
    while ( idx < len )
    {
        // Set up for the next line.
        linelen = 0;
        start_idx = idx;

        while ( linelen < CONS_MAX_LINE )
        {
            if ( text[idx] == XYZ_NTERM ) {
                // A terminator will end the input even if the length was specified
                // as being longer.  This is not a binary-safe buffer.
                len = idx;
                break;
            }

            if ( text[idx] == '\n' || text[idx] == '\r' )
            {
                // Include the EOL byte.
                idx++;
                linelen++;

                if ( idx < len && text[idx - 1] == '\r' && text[idx] == '\n' ) {
                // Check for and include a CRLF pair.
                idx++;
                linelen++;
                }

                break;
            }

            idx++;
            linelen++;
        }

        // A terminator will be added to the line to prevent buffer overflows
        // and make it compatible with other functions that expect / need
        // terminated buffers.
        linelen += 1;

        // Sanity.
        if ( linelen > pd->cons.bufdim ) {
            continue;
        }

        // Convenience.
        consline_s *list = pd->cons.linelist;
        u32 *rd = &(pd->cons.rbam.rd);
        u32 *wr = &(pd->cons.rbam.wr);
        xyz_rbam *rbam = &(pd->cons.rbam);

        // If the line list is full, make room for the new line.
        if ( XYZ_RBAM_FULL(rbam) ) { xyz_rbam_read(rbam); }

        // Calculate the position of the end of the line in the buffer, which is
        // also the "new position" for the line after this one.
        u32 newpos = list[*wr].pos + linelen;

        // Clear any top lines that would be overwritten by the new line.
        while ( list[*rd].pos >= list[*wr].pos &&
                list[*rd].pos < newpos &&
                XYZ_RBAM_MORE(rbam) )
        { xyz_rbam_read(rbam); }

        // Adjust the new starting location if out of bounds.
        if ( newpos > pd->cons.bufdim )
        {
            // The new line will not fit in the remainder of the buffer,
            // so restart at position 0.
            list[*wr].pos = 0;
            newpos = linelen;

            // Clear any lines that will be overwritten by the new line.
            while ( list[*rd].pos < newpos && XYZ_RBAM_MORE(rbam) )
            { xyz_rbam_read(rbam); }
        }

        // The line has been checked to be shorter than the size of the buffer.
        list[*wr].len = linelen;


        // The line data is (linelen - 1), since it was increased to account for
        // the terminator that will be written in the buffer.
        memcpy(pd->cons.buf + list[*wr].pos, text + start_idx, linelen - 1);

        // Set the last reserved byte to the terminator.
        pd->cons.buf[newpos - 1] = XYZ_NTERM;

        // Indicate that a new line has been written.
        xyz_rbam_write(rbam);

        // If the line list is now full, read at least one line so the new
        // position can be stored.
        if ( XYZ_RBAM_FULL(rbam) ) { xyz_rbam_read(rbam); }

        // Prepare for the next line.
        list[*wr].pos = newpos;
        list[*wr].len = 0;
    }

XYZ_EB_END

    if ( locked == 0 ) {
        SDL_UnlockMutex(pd->cons.mutex);
    }

    return len;
}
// console_write()


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
