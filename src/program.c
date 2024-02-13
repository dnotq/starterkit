/**
 * Main Program.
 *
 * @file   program.c
 * @author Matthew Hagerty
 * @date   April 5, 2020
 */

#include <string.h>         // memset
#include <stdbool.h>        // true, false

#include "GL/gl3w.h"        // OpenGL, must come before other OpenGL headers.
#include "SDL.h"            // Using SDL2
#include "xyz.h"            // short types
#include "log.h"            // logfmt
#include "draw_ui.h"        // IMGUI drawing
#include "draw_3d.h"        // 3D drawing
#include "disco.h"          // Disco interface
#include "program.h"


// This program uses cidx and cbuf.
#define CIDX_IMPLEMENTATION
#include "cidx.h"

#define CBUF_IMPLEMENTATION
#include "cbuf.h"


// Disco callback functions for events and drawing.
static s32 events(void *arg);
static s32 draw_init(void *arg);
static s32 draw_cleanup(void *arg);
static s32 draw_ui(void *arg);
static s32 draw_post_ui(void *arg);


/// One program data set, allocated static.
static pds_s g_progdata;


/**
 * Main.
 *
 * @param argc
 * @param argv
 *
 * @return 0 on success, otherwise 1.
 */
int
main(int argc, char *argv[])
{
    int rtn = 1;

    // Keep the compiler from complaining until these are used.
    (void)argc;
    (void)argv;

    pds_s *pds = &g_progdata; // convenience.


    // TODO Find the database:
    //
    // Current directory if not installed.
    // SDL program data location.
    // Never SDL program install location.

    // TODO Check database version.

    // Set up the window.
    // TODO get location from database.
    // TODO make sure the window is visible on a screen somewhere.

    // Initialize the program data and set up disco.
    memset(pds, 0, sizeof(*pds));

    // Use the IMGUI built-in ini capability for now.
    pds->disco.imgui_ini_filename = "starterkit.ini";

    pds->disco.winpos.x = SDL_WINDOWPOS_CENTERED;
    pds->disco.winpos.y = SDL_WINDOWPOS_CENTERED;
    pds->disco.winpos.w = WINDOW_WIDTH;
    pds->disco.winpos.h = WINDOW_HEIGHT;

    pds->disco.ver_major = VER_MAJOR;
    pds->disco.ver_minor = VER_MINOR;

    pds->disco.prg_name = APP_NAME;


    // Set the event and rendering callbacks.
    pds->disco.callback.events          = events;
    pds->disco.callback.draw_init       = draw_init;
    pds->disco.callback.draw_cleanup    = draw_cleanup;
    pds->disco.callback.draw_ui         = draw_ui;
    pds->disco.callback.draw_post_ui    = draw_post_ui;

    logfmt("%s\n", pds->disco.prg_name);
    SDL_version ver;
    SDL_GetVersion(&ver);
    logfmt("SDL version: %d.%d.%d\n", ver.major, ver.minor, ver.patch);

    // TODO create other program threads as needed.

    // Run disco.  Does not return until exit.
    disco_run(&pds->disco);

    rtn = 0;

DONE:

    // TODO any cleanup.

    return rtn;
}
// main()


/**
 * Event handler.
 *
 * @param[in] arg Pointer to the event SDL_Event structure.
 *
 * @return 0 if events were handled, otherwise -1.
 */
static s32
events(void *arg) {

    SDL_Event *event = (SDL_Event *)arg;
    s32 handled = -1;

    // Example for the event handler.
    // The SDL event structure is pd->disco.events.

    // TODO Event stuff.
    (void)event; // not currently used.

    // Return false to allow disco to handle window exit events.

    return handled;
}
// events()


static s32
draw_init(void *arg) {

    pds_s *pds = &g_progdata; // convenience.
    (void)arg; // unused.
    s32 rtn = -1;


    logfmt("OpenGL %s, GLSL %s\n"
        , glGetString(GL_VERSION), glGetString(GL_SHADING_LANGUAGE_VERSION));

    // TODO local drawing initialization. GL3W loader has been initialized.

    draw_3d_init(pds);

    rtn = 0;

DONE:

    return rtn;
}
// draw_init()


static s32
draw_cleanup(void *arg) {

    pds_s *pds = &g_progdata; // convenience.
    (void)arg; // unused.
    s32 rtn = -1;

    // TODO local drawing cleanup.

    draw_3d_cleanup(pds);

    rtn = 0;

DONE:

    return rtn;
}
// draw_cleanup()


/**
 * Draw callback.
 *
 * The background has been cleared and a new IMGUI frame created.  Draw any
 * elements to appear under the IMGUI UI first, followed by IMGUI functions.
 * Use the draw_post_ui() function to draw on top of IMGUI.
 *
 * Called by the rendering thread, so it is not synchronous to the main
 * program or any other thread.
 *
 * Typically wait for VSYNC is enabled, so this function is called at the
 * system frame rate.
 *
 * @param[in] arg unused.
 *
 * @return 0 if successful, otherwise -1.
 */
static s32
draw_ui(void *arg) {

    pds_s *pds = &g_progdata; // convenience.
    disco_s *disco = &g_progdata.disco; // convenience.
    (void)arg; // unused.

    if ( true == disco->status.minimized ) {
        // TODO update any state or data model while minimized.
        goto DONE;
    }

    // Set background color for *next* frame.
    disco->bgcolor.r = 0;
    disco->bgcolor.g = 0;
    disco->bgcolor.b = 0;
    disco->bgcolor.a = 255;

    // TODO draw anything to appear under IMGUI.


    // IMGUI is C++, so call out to a C++ function for UI drawing.
    draw_imgui_ui(pds);

DONE:
    return 0;
}
// draw_ui()


static s32
draw_post_ui(void *arg) {

    pds_s *pds = &g_progdata; // convenience.
    (void)arg; // unused.

    // TODO drawing after IMGUI.

    draw_3d(pds);

    return 0;
}
// draw_post_ui()


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
