/**
 * @file console_ui.h
 *
 * @author Matthew Hagerty
 * @date April 23, 2023
 */

#ifndef SRC_CONSOLE_UI_H_
#define SRC_CONSOLE_UI_H_

#include "log.h"        // interface with standard logging
#include "cidx.h"       // circular index for line list
#include "cbuf.h"       // circular buffer for character data
#include "xyz.h"        // short types
#include "SDL.h"        // SDL_mutex


/// Max console buffer size in bytes, approximately 10000 80-byte lines.
#define CONS_BUF_SIZE (1024 * 1024)

/// Max number of console lines.
#define CONS_LINES_MAX 10000

// Avoid C++ name-mangling to allow calling from C.
#ifdef __cplusplus
extern "C" {
#endif

/// Console log line.
typedef struct t_consline_s
{
    u32 pos;    ///< Position in the buffer where the line starts.
    u32 len;    ///< Length of the line.
} consline_s;


/// Console structure.
typedef struct t_cons_s {
    cbuf_s      lb;            ///< Line buffer.
    cidx_s      llm;           ///< Line list manager.
    consline_s *linelist;      ///< List of lines.
    SDL_mutex  *mutex;         ///< Mutex to make the console thread safe.
    u32         lockfailures;  ///< Number of times a mutex lock failed.
} cons_s;


/// Console write function as a log call-back (see log.h:t_log_output_fn)
void console_write(const char *buf, void *cdata, int32_t len);

/// Draw the console UI window-control.
void console_ui_window(progdata_s *pd);


#ifdef __cplusplus
}
#endif
#endif /* SRC_CONSOLE_UI_H_ */

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
