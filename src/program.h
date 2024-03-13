/**
 * Program header.
 *
 * @file   program.h
 * @author Matthew Hagerty
 * @date   April 5, 2020
 */

#pragma once

#include <stdint.h>         // uintXX_t, intXX_t, UINTXX_MAX, INTXX_MAX, etc.
#include <stdbool.h>        // true, false

#include "SDL.h"            // SDL_Window, SDL_atomics, SDL_event
#include "xyz.h"            // short types.
#include "disco.h"          // disco data struct.
//#include "console.h"        // Graphic console.


// Included in the header for early error message use.
#define APP_NAME  "Starter Kit"     ///< Application name.
#define VER_MAJOR 2                 ///< Major version number.
#define VER_MINOR 0                 ///< Minor version number.

#define WINDOW_WIDTH  640           ///< Default window width.
#define WINDOW_HEIGHT 800           ///< Default window height.


// Avoid C++ name-mangling since the main source is C.
#ifdef __cplusplus
extern "C" {
#endif


/// Program Data Structure.  Program-wide data.
typedef struct t_program_data_s
{
    disco_s disco;      ///< Disco interface.

    struct {
    f32 xrot;
    f32 yrot;
    f32 zrot;
    } model;

    struct {
    f32 x;
    f32 y;
    f32 z;
    } camera;

} pds_s;



#ifdef __cplusplus
}
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
