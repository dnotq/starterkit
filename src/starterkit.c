/**
 * Starter Kit.
 *
 * @file   starterkit.c
 * @author Matthew Hagerty
 * @date   April 5, 2020
 */

#include <stdio.h>          // fputc, fwrite, fflush
#include <stdarg.h>         // va_args


#ifdef _MSC_VER
#pragma warning(push, 3)
#include "uv.h"
#pragma warning(pop)
#else
#include "uv.h"
#endif

// The stb_sprintf implementation is included by IMGUI when STB use is enabled
// by defining IMGUI_USE_STB_SPRINTF, which is done in the CMakeFiles.txt.
#include "stb_sprintf.h"    // stbsp_snprintf

#include "starterkit.h"


// ==========================================================================
//
// Logging support.
//
// Logging starts off with a default of writing to a console until the
// window system is up and running.  For errors during initialization where
// there is no console, having OS-specific functions to display a dialog box
// might be an option (and certainly more friendly for the user rather than
// the app silently failing).
//
// ==========================================================================


/// Max size of a log message buffer.  Adjust as needed, or replace with some
/// other logging scheme.
#define LOG_BUF_MAX 256

/// Number of simultaneous logging calls before a message will be dropped.
#define LOG_BUF_SIZE 10

/// Log buffers.
static uint8_t g_log_bufs[LOG_BUF_SIZE][LOG_BUF_MAX];

/// Current log buffer index.
static uint32_t g_log_idx = 0;

/// Logging buffer mutex.
static uv_mutex_t g_log_mutex;


/**
 * Log a message.
 *
 * @param app
 * @param level
 * @param file
 * @param linenum
 * @param fmt
 * @return The length of the log message, otherwise less-than zero on error.
 */
int
sk_log_ex(app_s *app, uint32_t level, const char *file, uint32_t linenum, const char *fmt, ...)
{
    int rtn = -1; // default to error.

    XYZ_BLOCK

    if ( app == NULL || file == NULL || fmt == NULL ) {
        XYZ_BREAK
    }

    if ( fmt[0] == XYZ_NTERM || level < app->loglevel ) {
        rtn = 0; // non-fatal, but nothing to do.
        XYZ_BREAK
    }

    // Get a buffer for the message.  Blocks on obtaining the mutex, but the
    // delay should never be long enough to cause any problems.  Convert to
    // a try-lock if necessary.

    // Critical section.
    uv_mutex_lock(&g_log_mutex);
    uint8_t *bp = g_log_bufs[g_log_idx];

    g_log_idx = (g_log_idx < (LOG_BUF_SIZE - 1) ? g_log_idx + 1 : 0);

    // If the buffer is free, mark it busy.
    int buffer_busy = XYZ_NO;
    if ( bp[0] == XYZ_NTERM ) {
        bp[0] = ' ';
    } else {
        buffer_busy = XYZ_YES;
    }

    uv_mutex_unlock(&g_log_mutex);
    // End critical section.


    if ( buffer_busy == XYZ_YES )
    {
        // The buffer is not free, too many threads are trying to log messages
        // faster than they can be displayed.

        // TODO decide how to deal with the overflow situation.

        XYZ_BREAK
    }


    // Extract the last segment of the file name.
    const char *f_start = file;
    const char *f_end = file;
    for ( ; *f_end != XYZ_NTERM ; f_end++ )
    {
        if ( *f_end == '/' || *f_end == '\\' ) {
            f_start = f_end;
        }
    }


    // Setup the buffer to skip the busy/free indicator.
    uint8_t *buf = bp + 1;
    uint32_t max = LOG_BUF_MAX - 1;
    uint32_t len = 0;

    // Make sure the file name fits in the buffer.  Truncate if necessary.
    if ( (f_end - f_start) > (LOG_BUF_MAX / 2) )
    {
        f_start = f_end - (LOG_BUF_MAX / 2) - 4;

        // Add truncate indicators '...'
        buf[0] = '.'; buf[1] = '.'; buf[2] = '.';
        len += 3;
    }


    char *logtype;
    switch ( level ) {
    case SK_LOG_LVL_DEBUG   : logtype = "DEBUG"; break;
    case SK_LOG_LVL_INFO    : logtype = "INFO" ; break;
    case SK_LOG_LVL_WARN    : logtype = "WARN" ; break;
    case SK_LOG_LVL_CRIT    : logtype = "CRIT" ; break;
    default                 : logtype = "UNKNOWN"; break;
    }

    // Add the boiler-plate header.
    int part = stbsp_snprintf(&(buf[len]), (max - len), "%s: %s:%d: ", logtype, f_start, linenum);

    if ( part < 0 )
    {
        // Error, should not happen, but in case it does, discard all of the
        // boiler-plate and just try adding the provided message.
        len = 0;
    } else {
        len += (uint32_t )part;
    }


    // Add the provided message.
    va_list args;
    va_start(args, fmt);
    part = stbsp_vsnprintf(&(buf[len]), (max - len), fmt, args);
    va_end(args);

    if ( part < 0 ) {
        // TODO Error, should not happen.  Implement resolution.
        rtn = part;
    } else {
        len += (uint32_t)part;

        // Call the output function.
        app->logwrite_fn(app, buf, len);
        rtn = len;
    }

    // Mark the buffer free.
    bp[0] = XYZ_NTERM;

    XYZ_END

    return rtn;
}
// sk_log_ex()


// TODO Set up OS specific call-backs to display a system dialog box to allow
//      initialization error messages to be seen in cases where there is no
//      console, and before he windowing system has been established.


/**
 * Command shell logging.
 *
 * Once the GUI is set up and working, the logging call-back should be changed
 * from this function to the GUI logging system.
 *
 * @param app
 * @param buf
 * @param len
 */
static void
log_cmd_write(void *app, const char *buf, uint32_t len)
{
    (void)app; // Unused. Cast to app_s if needed.

    const int NUM_BUFS = 1;
    fwrite(buf, len, NUM_BUFS, stdout);
    fputc('\n', stdout);
    fflush(stdout);
}
// log_cmd_write()


// ==========================================================================
//
// Application API.
//
// ==========================================================================


/**
 * Signal the application from the GUI or any other threads.
 *
 * Multiple calls to this function are fine and will not stack; the
 * application will see a single combined signal on its next loop iteration.
 *
 * This function allows the GUI to remain separate and have no knowledge of
 * the details about the application loop, LibUV, etc.
 *
 * The application uses this signal to know it needs to perform actions based
 * on GUI events.
 *
 * @param app       Pointer to the application data structure.
 */
void
sk_app_ext_signal(app_s *app)
{
    if ( app == NULL ) {
        return;
    }

    // Make sure the app structure is passed to the call-back, and signal
    // the LibUV loop.
    app->extsig.data = app;
    uv_async_send(&app->extsig);
}
// sk_app_ext_signal()


// ==========================================================================
//
// Local private functions.
//
// ==========================================================================


/**
 * Call-back when an external signal is received.
 *
 * @param handle    LibUV asynchronous event handle.
 */
static void
app_signal_cb(uv_async_t *handle)
{
    if ( handle == NULL || handle->data == NULL ) {
        return;
    }

    // Get app structure pointer.
    app_s *app = (app_s *)(handle->data);

    SK_LOG_DBG(app, "GUI signaled the application.");

    // If the GUI has exited, terminate the application.
    if ( app->gui.running == XYZ_FALSE )
    {
        SK_LOG_DBG(app, "GUI has exited, exiting application.");
        uv_stop(&(app->loop));
        app->app.running = XYZ_FALSE;
    }
}
// app_signal_cb()


/**
 * Application thread.
 *
 * @param app       Pointer to the application data structure.
 */
static void
app_thread(void *arg)
{
    if ( arg == NULL ) {
        return;
    }

    // Get app structure pointer.
    app_s *app = (app_s *)(arg);

    XYZ_BLOCK

        if ( uv_loop_init(&app->loop) != 0 ) {
            SK_LOG_CRIT(app, "uv_loop_init() failed.");
            XYZ_BREAK;
        }

        // TODO check return
        uv_async_init(&app->loop, &app->extsig, app_signal_cb);
        //    Initialize the handle. A NULL callback is allowed.
        //    Returns:    0 on success, or an error code < 0 on failure.


        // Set the ready flag and wait for the GUI.
        // TODO Add timeout and error.
        app->app.running = XYZ_TRUE;
        while ( app->gui.running == XYZ_FALSE )
        {
            // Yield the thread and wait.
            const uint32_t WAIT_100MS = 100;
            uv_sleep(WAIT_100MS);
        }


        // Blocking call, runs the LibUV event loop until the user quits.
        SK_LOG_DBG(app, "Running application loop.");
        uv_run(&app->loop, UV_RUN_DEFAULT);
        SK_LOG_DBG(app, "Application loop finished.");

        // TODO cleanup.

    XYZ_END

    // Shutdown the UV loop.  If the loop has not stopped on its own, skip
    // calling close because it will block the app shutdown (which will
    // reclaim and close the resources anyway).
    uv_stop(&app->loop);
    if ( uv_loop_alive(&app->loop) == 0 ) {
        uv_loop_close(&app->loop);
    }

    app->app.running = XYZ_FALSE;
}
// app_thread()


// ==========================================================================


/**
 * Main.
 *
 * @param argc      Command line argument count.
 * @param argv      Command line argument array.
 *
 * @return 0 if successful, > 0 on error.
 */
int
main(int argc, char *argv[])
{
    app_s app;
    uv_thread_t app_handle;
    int app_valid = -1;

    XYZ_BLOCK

        app.app.running = XYZ_FALSE;
        app.gui.running = XYZ_FALSE;
        app.loglevel    = SK_LOG_LVL_DEBUG;
        app.logwrite_fn = log_cmd_write;

        if ( uv_mutex_init(&g_log_mutex) != 0 )
        {
            printf("CRIT: %s:%d: cannot initialized log mutex.\n", XYZ_CFL);
            XYZ_BREAK
        }

        // Initialize the log buffers to a free status.
        for ( uint32_t i = 0 ; i < LOG_BUF_SIZE ; i++ ) {
            g_log_bufs[i][0] = XYZ_NTERM;
        }


        // Initialize and start the application thread.  LibUV threads are not
        // part of the LibUV loop.
        app_valid = uv_thread_create(&app_handle, app_thread, &app);

        if ( app_valid != 0 )
        {
            printf("CRIT: %s:%d: failed to start main application thread.\n", XYZ_CFL);
            XYZ_BREAK
        }


        // Blocking call, runs the GUI until the user quits.
        SK_LOG_DBG(&app, "Staring GUI.");
        int rtn = sk_gui_run(&app);
        if ( rtn != 0 ) {
            SK_LOG_WARN(&app, "GUI returned error code [%d].", rtn);
        } else {
            SK_LOG_DBG(&app, "GUI closed.");
        }

    XYZ_END

    // Wait for the application thread to exit.
    if ( app_valid == 0 ) {
        SK_LOG_DBG(&app, "Waiting to join application thread.");
        uv_thread_join(&app_handle);
        SK_LOG_DBG(&app, "Application thread joined.");
    }

    return 0;
}
// main()


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
