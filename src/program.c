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
#include "wrap_c2cpp.h"

// The stb_sprintf implementation is included by IMGUI when STB use is enabled
// by defining IMGUI_USE_STB_SPRINTF, which is done in the CMakeFiles.txt.
//#define STB_SPRINTF_IMPLEMENTATION
#include "stb_sprintf.h"    // stbsp_snprintf


#define APP_NAME  "Starter Kit"  ///< Application name.
#define VER_MAJOR 1              ///< Major version number.
#define VER_MINOR 0              ///< Minor version number.

#define WINDOW_WIDTH  960        ///< Default window width.
#define WINDOW_HEIGHT 860        ///< Default window height.


// Call-back and thread functions.
static s32 main_program(void *arg);
static s32 draw(void *arg);
static s32 cleanup(void *arg);


/**
 * Program initialization, called by disco.
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

   // Set up SDL.
   if ( SDL_Init(SDL_INIT_EVERYTHING) != 0 )
   {
//       SK_LOG_CRIT(app, "%s", SDL_GetError());
       printf("%s", SDL_GetError());
       XYZ_BREAK
   }


   // TODO Find the database:
   //
   // Current directory if not installed.
   // SDL program data location.
   // Never SDL program install location.

   // TODO Check database version.

   // Set up the window.
   // TODO get location from database.
   // TODO make sure the window is visible on a screen somewhere.
   pd->winpos.x = SDL_WINDOWPOS_CENTERED;
   pd->winpos.y = SDL_WINDOWPOS_CENTERED;
   pd->winpos.w = WINDOW_WIDTH;
   pd->winpos.h = WINDOW_HEIGHT;

   pd->ver_major = VER_MAJOR;
   pd->ver_minor = VER_MINOR;

   pd->prg_name.format = XYZ_META_F_POINTER;
   pd->prg_name.alloc  = XYZ_META_P_DYNAMIC;
   pd->prg_name.type   = XYZ_META_T_ASCII_VARCHAR;
   pd->prg_name.byte_dim = 80;
   pd->prg_name.buf.vp = xyz_malloc(pd->prg_name.byte_dim);
   if ( pd->prg_name.buf.vp == NULL ) {
      pd->prg_name.byte_dim = 0;
      XYZ_BREAK;
   }
   pd->prg_name.unit_dim = pd->prg_name.byte_dim;
   pd->prg_name.unit_len = stbsp_snprintf(pd->prg_name.buf.vp, pd->prg_name.unit_dim, "%s v%d.%d",
         APP_NAME, pd->ver_major, pd->ver_minor);
   pd->prg_name.byte_len = pd->prg_name.unit_len;

   // Set up a main program thread to be started after initialization.
   pd->main_thread = main_program;

   // No event call-back, for now.
   pd->callback.event = NULL;

   // Set the rendering and cleanup call-backs.
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

   if ( pd->prg_name.alloc == XYZ_META_P_DYNAMIC &&
         pd->prg_name.buf.vp != NULL && pd->prg_name.byte_dim > 0 )
   {
      xyz_free(pd->prg_name.buf.vp);
      pd->prg_name.byte_dim = 0;
   }

   rtn = XYZ_OK;
   XYZ_END

   return rtn;
}
// cleanup()


/**
 * Main Program Thread
 *
 * The main function to be called for the program.  Follows the SDL2 thread
 * prototype.  When this function returns, the application will exit.
 *
 * @param[in] arg Pointer to the program data structure.
 *
 * @return 0
 */
static s32
main_program(void *arg)
{
   progdata_s *pd = (progdata_s *)arg;
   (void)pd; // not currently used.

   return 0;
}
// main_program()


/**
 * Main program drawing.
 *
 * Called by the rendering thread, so it is not synchronous to the main
 * program thread, or the original "main" called by the OS.  Any kind of OpenGL,
 * IMGUI, or SDL2 drawing, etc. is ok here.
 *
 * @param[in] arg Pointer to the program data structure.
 *
 * @return 0
 */
static s32
draw(void *arg)
{
   progdata_s *pd = (progdata_s *)arg;
   (void)pd; // not currently used.

   // All program drawing goes here.

   static u32 show_demo_window = 1;
   imgui_ShowDemoWindow(&show_demo_window);

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
