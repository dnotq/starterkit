/**
 * @file disco.h
 *
 * @author Matthew Hagerty
 * @date 2024 Feb 10
 */

#pragma once

#include <stdbool.h>    // true, false
#include "xyz.h"        // short types


// Avoid C++ name-mangling to allow calling from C.
#ifdef __cplusplus
extern "C" {
#endif


/// Program callback from disco.  Pointer to a function that takes a void * and
/// returns a signed 32-bit integer.
typedef s32 (disco_callback_fn)(void *arg);


/// Disco structure.  Set by the program prior to calling disco.
/// The winpos values specify the starting location and size, and will be
/// updated per frame.
/// The backgroud color can be updated by the draw callback per frame.
/// The minimized flag is updated every frame and can be used by the draw
/// callbacks to decide what to do.
typedef struct t_disco_s {

    const c8   *prg_name;               ///< Program name for the window title.
    s32         ver_major;              ///< Major version number.
    s32         ver_minor;              ///< Minor version number.

    c8         *imgui_ini_filename;     ///< NULL to disable, otherwise a filename.

    struct {
    s32         x;                      ///< Window x position.
    s32         y;                      ///< Window y position.
    s32         w;                      ///< Window width.
    s32         h;                      ///< Window height.
    } winpos;                           ///< Initial window position and size information.

    struct {
    bool        disable_screensaver;    ///< Disable the system screensaver (games may want True).
    bool        bypass_x11_compositor;  ///< Bypass/disable X11 compositor (games may want True).
    } hints;                            ///< SDL hints.

    struct {
    bool        minimized;              ///< True if the window is minimized, updated per frame.
    u64         frame_time_us;          ///< Full frame time (includes VSYNC) in microseconds.
    u64         render_time_us;         ///< Frame rendering time in microseconds.
    u64         render_counter;         ///< Increments for every frame rendered.
    u64         event_counter;          ///< Increments for every received event.
    } status;                           ///< Status updated by rendering thread every frame.

    struct {
    disco_callback_fn  *events;         ///< Event processing callback.
    disco_callback_fn  *draw_init;      ///< Pre render loop callback.
    disco_callback_fn  *draw_ui;        ///< Drawing UI layer callback.
    disco_callback_fn  *draw;           ///< Post UI drawing (no UI) callback.
    } callback;                         ///< Callback functions for disco.

} disco_s;


/// Runs the event loop and rendering thread.
void disco_run(disco_s *disco);

/// Check is disco is running.
bool disco_is_running(void);

/// Tells disco the main program is exiting.
void disco_program_exiting(void);


#ifdef __cplusplus
}
#endif

/*
------------------------------------------------------------------------------
This software is available under 2 licenses -- choose whichever you prefer.
------------------------------------------------------------------------------
ALTERNATIVE A - MIT License
Copyright (c) 2024 Matthew Hagerty
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
