/**
 * @file console_ui.cpp
 *
 * @date April 23, 2023
 * @author Matthew Hagerty
 */

#include <stdint.h>        // uintXX_t, intXX_t, UINTXX_MAX, INTXX_MAX, etc.
#include <stdbool.h>       // true, false

#include "SDL.h"
#include "imgui.h"
#include "xyz.h"
#include "program.h"
#include "console.h"


/**
 * Display the graphic console.
 *
 * Use and adjust as desired to support the program.
 *
 * @param[in] pd Pointer to the program data structure.
 */
void
console_ui_window(progdata_s *pd) {

   // Make a window for the graphic console.
   ImGui::SetNextWindowSize(ImVec2(840, 680), ImGuiCond_FirstUseEver);
   ImGui::Begin("GConsole");

   ImVec2 sz = ImGui::GetWindowSize();
   ImGui::Text("Window Size: %.0f,%.0f", sz.x, sz.y);

   u32 rdpos = pd->cons.linelist[pd->cons.rbam.rd].pos;
   u32 wrpos = pd->cons.linelist[pd->cons.rbam.wr].pos;
   u32 bufuse = rdpos <= wrpos ?
         wrpos - rdpos : (pd->cons.bufdim - rdpos) + wrpos;
   ImGui::Text("Console lines: %'u/%'u  Buffer: %_$u/%_$u"
         , pd->cons.rbam.used, (pd->cons.rbam.dim - 1), bufuse, pd->cons.bufdim);

   // Console lines area.
   ImGui::Separator();

   // Leave room at the bottom for the input box.
   const float footer_height_to_reserve =
         ImGui::GetStyle().ItemSpacing.y +   // separator height
         ImGui::GetFrameHeightWithSpacing(); // input box height

   // Always showing the scroll bar makes long console lines resize with
   // changes in the window size.
   ImGuiWindowFlags_ sflags =
         // ImGuiWindowFlags_HorizontalScrollbar
         ImGuiWindowFlags_AlwaysHorizontalScrollbar;

   ImGui::BeginChild("ConsScrollingRegion", ImVec2(0, -footer_height_to_reserve),
         false, sflags);

   // Use the Clipper to control the scrolling.
   float text_height = ImGui::GetTextLineHeightWithSpacing();
   ImGuiListClipper clipper;
   clipper.Begin(pd->cons.rbam.used, text_height);
   while ( clipper.Step() == true )
   {
      for ( s32 line_no = clipper.DisplayStart ; line_no < clipper.DisplayEnd ; line_no++ )
      {
         if ( line_no < 0 ) { continue; }

         // Making an assumption here that line_no can never be higher than the
         // dimension of the lines array.
         u32 idx = pd->cons.rbam.rd + (u32)line_no;

         if ( pd->cons.rbam.rd <= pd->cons.rbam.wr ) {
            if ( idx >= pd->cons.rbam.wr ) { break; }
         }

         else if ( idx >= pd->cons.rbam.dim ) {
            idx -= pd->cons.rbam.dim; // wrap the index.
            if ( idx >= pd->cons.rbam.wr  ) { break; }
         }

         // TODO add ability to select text, copy to clip board, etc.

         // TextWrapped will, uh, wrap lines (sometimes, it is odd).
         // TextUnformatted will not wrap lines at all, ever.
         // Also, the horizontal scroll bar only appears when the long lines
         // are actually displayed.
         ImGui::TextWrapped("%s", pd->cons.buf + pd->cons.linelist[idx].pos);

         //const c8 *line_start = pd->cons.buf + pd->cons.linelist[idx].pos;
         //const c8 *line_end   = line_start + pd->cons.linelist[idx].len - 1;
         //ImGui::TextUnformatted(line_start, line_end);
      }
   }
   clipper.End();

   // Keep scrolling at the bottom.  This gets a little funky when the
   // window width is really narrow and lines of text are wrapping.  Sometimes
   // there can be a lot of jitter in drawing the text.
   if ( ImGui::GetScrollY() >= (ImGui::GetScrollMaxY() - text_height) ) {
      ImGui::SetScrollHereY();
   }

   ImGui::EndChild();


   // Input line.
   ImGui::Separator();

   #define BUFDIM 81
   static c8 buf[2 + BUFDIM] = {'>', ' ', XYZ_NTERM};
   c8 *inbuf = buf + 2;
   ImGui::SetNextItemWidth(ImGui::GetFontSize() * BUFDIM * 0.54f);
   if ( ImGui::InputText("##console_input", inbuf, BUFDIM,
         ImGuiInputTextFlags_EnterReturnsTrue) == true )
   {
      ImGui::SetKeyboardFocusHere(-1); // Auto focus previous widget
      pd->cons.out(pd, buf, (u32)strlen(buf));
      *inbuf = XYZ_NTERM; // Clear the input box.
   }

   ImGui::End();
}
// console_ui_window()


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
