/**
 * Program header.
 *
 * @file   program.h
 * @author Matthew Hagerty
 * @date   April 5, 2020
 */

#ifndef SRC_PROGRAM_H_
#define SRC_PROGRAM_H_

#include "xyz.h"           // Simplified unambiguous types and other helpers.
#include "SDL.h"           // SDL_Window

/// The dimension of the TTY line buffer.  Any single message larger than this
/// buffer will be truncated.
#define TTY_LINEBUF_DIM 2048

/// The dimension of the graphical console / log buffer.
#define CONS_BUF_DIM (1024 * 1024)


// The ## in front of __VA_ARGS__ is required to deal with the case where there
// are no arguments.

/// TTY formatted output (printf equivalent).
#define TTYF(pd, fmt, ...) {s32 len = stbsp_snprintf(pd->tty.buf, pd->tty.bufdim, fmt, ##__VA_ARGS__); \
   pd->tty.out(pd->tty.buf, len);}



// Avoid C++ name-mangling since the main source is C.
#ifdef __cplusplus
extern "C" {
#endif


/// Basic program callback.  Pointer to a function that takes a void * and
/// returns a signed 32-bit integer.  Can also be used to implement a thread.
typedef s32 (callback_fn)(void *arg);

/// Text logging, console, tty function prototype.
typedef s32(out_fn)(const c8 *text, u32 len);


/// Program Data Structure.
typedef struct unused_tag_progdata_s
{
   xyz_meta    prg_name;      ///< Program name for the window title.
   s32         ver_major;     ///< Major version number.
   s32         ver_minor;     ///< Minor version number.
   struct {
   s32 x;                     ///< Window x position.
   s32 y;                     ///< Window y position.
   s32 w;                     ///< Window width.
   s32 h;                     ///< Window height.
   } winpos;                  ///< Initial window position and size information.

   callback_fn *main_thread;  ///< Main program thread, optional.
   struct {
   callback_fn *event;        ///< Event call-back, optional.
   callback_fn *draw;         ///< Drawing / rendering call-back, optional.
   callback_fn *cleanup;      ///< Cleanup call-back, optional.
   } callback;                ///< Call-back functions.

   // ^^ Prorgram data above. ^^
   // --------------------------
   // vv Support data below.  vv

   struct {
   SDL_Window *window;                 ///< Pointer to the main application window.
   const c8   *glsl_version;           ///< GLSL version string.
   s32         running;                ///< Flag that disco is running.
   s32         render_thread_running;  ///< Flag that the rendering thread is running.
   u64         render_time_us;         ///< Frame rendering time in microseconds.
   u64         render_counter;         ///< Increments for every frame rendered.
   u64         event_counter;          ///< Increments for every received event.
   } disco;                            ///< DISCO (Display and IO).

   struct {
      out_fn  *out;              ///< Output function for the tty.
      c8      *buf;              ///< Output line buffer.
      u32      bufdim;           ///< Dimension of the output line buffer.
   } tty;                        ///< Output to stdout / terminal / shell.

   struct {
      out_fn  *out;              ///< Output function for the console.
      c8      *loc;              ///< Current location in the buffer.
      c8      *buf;              ///< Console buffer.
      u32      bufdim;           ///< Dimension of the buffer.
   } cons;                       ///< Internal console and log.

} progdata_s;


/// Main program initialization function.  Called by disco.
s32 program_init(progdata_s *pd);


#ifdef __cplusplus
}
#endif
#endif /* SRC_PROGRAM_H_ */

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
