/**
 * Main Program.
 *
 * @file   program.c
 * @author Matthew Hagerty
 * @date   April 5, 2020
 */

#include <stdio.h>	// NULL, stdout, fwrite
#include "SDL.h"
#include "program.h"
#include "stb_sprintf.h"    // stbsp_snprintf
#include "cpp_stuff.h"


#define WINDOW_WIDTH  640        ///< Default window width.
#define WINDOW_HEIGHT 800        ///< Default window height.


// Call-back and thread functions.
static s32 cleanup(void *arg);
static s32 main_program(void *arg);
static s32 events(void *arg);
static s32 draw(void *arg);


/**
 * Program initialization, called by disco.
 *
 * This function is not a call-back per se, disco will call this function by
 * name to do program data initialization.
 *
 * @param[in] pd  Pointer to the program data structure.
 *
 * @return XYZ_OK on success, otherwise XYZ_ERR.
 */
s32
program_init(progdata_s *pd)
{
   s32 rtn = XYZ_ERR;

   XYZ_BLOCK


   // TODO Find the database:
   //
   // Current directory if not installed.
   // SDL program data location.
   // Never SDL program install location.

   // TODO Check database version.

   // Set up the window.
   // TODO get location from database.
   // TODO make sure the window is visible on a screen somewhere.

   // Use the IMGUI built-in ini capability for now.
   pd->imgui_ini_filename = "starterkit.ini";

   pd->winpos.x = SDL_WINDOWPOS_CENTERED;
   pd->winpos.y = SDL_WINDOWPOS_CENTERED;
   pd->winpos.w = WINDOW_WIDTH;
   pd->winpos.h = WINDOW_HEIGHT;

   pd->ver_major = VER_MAJOR;
   pd->ver_minor = VER_MINOR;


   // The program name is a "metadata" type.  Still experimental, needs some
   // support functions.
   pd->prg_metaname.format = XYZ_META_F_POINTER;
   pd->prg_metaname.alloc  = XYZ_META_P_DYNAMIC;
   pd->prg_metaname.type   = XYZ_META_T_ASCII_VARCHAR;
   pd->prg_metaname.byte_dim = 80;
   pd->prg_metaname.buf.vp = xyz_malloc(pd->prg_metaname.byte_dim);
   if ( pd->prg_metaname.buf.vp == NULL ) {
      pd->prg_metaname.byte_dim = 0;
   } else {
      pd->prg_metaname.unit_dim = pd->prg_metaname.byte_dim;
      pd->prg_metaname.unit_len = stbsp_snprintf(
            pd->prg_metaname.buf.vp, pd->prg_metaname.unit_dim, "%s v%d.%d",
            APP_NAME, pd->ver_major, pd->ver_minor);
      pd->prg_metaname.byte_len = pd->prg_metaname.unit_len;
      pd->prg_name = (c8 *)pd->prg_metaname.buf.vp;
   }



   // Set up the main program thread to be started after initialization.
   pd->main_thread = main_program;

   // Set the event, rendering, and cleanup call-backs.
   pd->callback.event = events;
   pd->callback.draw = draw;
   pd->callback.cleanup = cleanup;

   rtn = XYZ_OK;
   XYZ_END

   return rtn;
}
// program_init()


/**
 * Cleanup.
 *
 * @param[in] arg Pointer to the program data structure.
 *
 * @return XYZ_OK on success, otherwise XYZ_ERR.
 */
static s32
cleanup(void *arg)
{
   progdata_s *pd = (progdata_s *)arg;

   s32 rtn = XYZ_ERR;

   XYZ_BLOCK

   if ( pd->prg_metaname.alloc == XYZ_META_P_DYNAMIC &&
         pd->prg_metaname.buf.vp != NULL && pd->prg_metaname.byte_dim > 0 )
   {
      xyz_free(pd->prg_metaname.buf.vp);
      pd->prg_metaname.byte_dim = 0;
   }

   rtn = XYZ_OK;
   XYZ_END

   return rtn;
}
// cleanup()


/**
 * Main Program Thread.
 *
 * The main function to be called for the program.  Follows the SDL2 thread
 * prototype.
 *
 * The program_running flag must be set to XYZ_FALSE prior to exiting in order
 * to properly terminate the program.  The disco.running flag should also be
 * watched, and the program exited if the flag is ever found to be XYZ_FALSE.
 *
 * @note DO NOT TRY TO HANDLE EVENTS IN THE THREAD-FORM OF THIS FUNCTION.  Or,
 * rewrite the program and disco to your liking...
 *
 * @param[in] arg Pointer to the program data structure.
 *
 * @return A return value to propagate to the main exit for the program.
 */
static s32
main_program(void *arg)
{
   s32 rtn = 0; // unix-style return value: 0 == everything was ok.

   progdata_s *pd = (progdata_s *)arg;

   TTYF(pd, "%s\n", pd->prg_name);
   TTYF(pd, "Main program thread is running.\n");


   // Put some text into the graphic console buffer.
   CONSF(pd, "%s\n", pd->prg_name);
   CONSF(pd, "Main program thread is running.\n\n");
   CONSF(pd, "This is the graphic console.  Output can be written here using "
         "the CONSF helper macro (just like printf to the TTY console).\n");


   // Must watch the disco.running flag.
   while ( pd->program_running == XYZ_TRUE && pd->disco.running == XYZ_TRUE )
   {
      // TODO lots of program kinds of stuff here.
      SDL_Delay(100);

      // TODO if the program errors, break and make sure to set
      // program_running to XYZ_FALSE.
   }

   // Must set the program running flag to XYZ_FALSE on exit so the thread is
   // joined properly.  Also push an event to make sure disco responds in a
   // timely manner.

   pd->program_running = XYZ_FALSE;

   SDL_Event sdlevent;
   sdlevent.type = SDL_QUIT;
   SDL_PushEvent(&sdlevent);

   return rtn;
}
// main_program()


/**
 * Event handler.
 *
 * @param[in] arg Pointer to the program data structure.
 *
 * @return XYZ_TRUE if events were handled, otherwise XYZ_FALSE;
 */
static s32
events(void *arg)
{
   s32 handled = XYZ_FALSE;

   progdata_s *pd = (progdata_s *)arg;

   // Example for the event handler.
   // The SDL event structure is pd->disco.events.

   // TODO Event stuff.
   (void)pd; // not currently used.

   // Return XYZ_FALSE to allow disco to handle window exit events.

   return handled;
}
// events()


/**
 * Main program drawing.
 *
 * Called by the rendering thread, so it is not synchronous to the main
 * program thread, or the original "main" called by the OS.
 *
 * Set to align with VSYNC, so this function is called at the system frame rate.
 *
 * @param[in] arg Pointer to the program data structure.
 *
 * @return 0
 */
static s32
draw(void *arg)
{
   progdata_s *pd = (progdata_s *)arg;

   // This is a little convoluted to turn around and call another function,
   // but the goal is mostly C, and IMGUI is C++, so all that kind of code will
   // have to go into the C++ part.

   imgui_draw(pd);

   return 0;
}
// draw()


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
