/**
 * Starter Kit.
 *
 * @file   starterkit.h
 * @author Matthew Hagerty
 * @date   April 5, 2020
 */

#ifndef STARTERKIT_H_
#define STARTERKIT_H_

#include <stdint.h>         // uintXX_t, intXX_t, UINTXX_MAX, INTXX_MAX, etc.

// Non-system library dependencies.
#ifdef _MSC_VER
#pragma warning(push, 3)
#include "uv.h"
#pragma warning(pop)
#else
#include "uv.h"
#endif

#include "xyz_utils.h"

// Avoid C++ name-mangling for C functions.
#ifdef __cplusplus
extern "C" {
#endif


#define SK_APP_NAME             "Starter Kit"   ///< Application name.
#define SK_VER_MAJOR            1               ///< Major version number.
#define SK_VER_MINOR            0               ///< Minor version number.

#define SK_WINDOW_WIDTH         960             ///< Default window width.
#define SK_WINDOW_HEIGHT        860             ///< Default window height.


/// Function-type for a logging call-back to display the provided buffer.
/// @param data  Expected to be a pointer to the app structure.
typedef void (sk_log_f)(void *data, const char *buf, uint32_t len);


/// Log levels.
enum
{
    SK_LOG_LVL_DEBUG = 0,
    SK_LOG_LVL_INFO,
    SK_LOG_LVL_WARN,
    SK_LOG_LVL_CRIT
};


/// Application configuration and data.
typedef struct _app_s
{
    uv_loop_t       loop;                       ///< Application LibUV loop.
    uv_async_t      extsig;                     ///< Handle to signal the application loop.
    struct {
    int             running;                    ///< XYZ_TRUE if the application thread is running.
    } app;
    struct {
    int             running;                    ///< XYZ_TRUE if the GUI loop is running.
    } gui;
    uint32_t        loglevel;                   ///< Global logging level.
    sk_log_f       *logwrite_fn;                ///< Pointer to the log writing function.
} app_s;


// ==========================================================================
//
// Function declarations
//
// ==========================================================================

// The ## in front of __VA_ARGS__ is required to deal with the case where there
// are no arguments.

/// Logging helpers.
#define SK_LOG_DBG(app, fmt, ...)  (sk_log_ex(app, SK_LOG_LVL_DEBUG, __FILE__, __LINE__, fmt, ##__VA_ARGS__))
#define SK_LOG_INFO(app, fmt, ...) (sk_log_ex(app, SK_LOG_LVL_INFO,  __FILE__, __LINE__, fmt, ##__VA_ARGS__))
#define SK_LOG_WARN(app, fmt, ...) (sk_log_ex(app, SK_LOG_LVL_WARN,  __FILE__, __LINE__, fmt, ##__VA_ARGS__))
#define SK_LOG_CRIT(app, fmt, ...) (sk_log_ex(app, SK_LOG_LVL_CRIT,  __FILE__, __LINE__, fmt, ##__VA_ARGS__))

/// Log message.
int sk_log_ex(app_s *app, uint32_t level, const char *file, uint32_t linenum, const char *fmt, ...);

/// GUI function.
int sk_gui_run(app_s *app);

/// Signal the application from the GUI or other threads.
/// Multiple calls to this function are fine and will not stack; the
/// application will see a single combined signal on its next loop iteration.
void sk_app_ext_signal(app_s *app);


#ifdef __cplusplus
}
#endif
#endif /* STARTERKIT_H_ */

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
