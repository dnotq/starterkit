/**
 * A place for C++ stuff, since for now it cannot be avoided (IMGUI mostly).
 *
 * @file cpp_stuff.cpp
 * @date Jun 5, 2021
 * @author Matthew Hagerty
 */


#include "cpp_stuff.h"

#include "SDL.h"
#include "imgui.h"
#include "program.h"
#include "stb_sprintf.h"    // stbsp_snprintf
#include "math.h"


// Draw the graphic console.  This is local to the program it will most likely
// be highly customized.
static void imgui_console_window(progdata_s *pd);



/// Moving point structure for the line example.
typedef struct unused_tag_linept_s {
   s32 x;
   s32 y;
   s32 dx;
   s32 dy;
} linept_s;

/// A pair of points for a line.
typedef struct unused_tag_pos_s {
   ImVec2 pt1;
   ImVec2 pt2;
} pos_s;


/**
 * Draw the IMGUI stuff, since it needs C++.
 *
 * @param[in] pd Pointer to the program data structure.
 */
void
imgui_draw(progdata_s *pd)
{
   static bool show_gconsole = false;
   static bool show_demo = false;
   static bool show_ball = true;

   static float speed = 0.5;
   static u32 max_pts = 30;

   ImGuiIO &io = ImGui::GetIO();
   ImGuiViewport *view = ImGui::GetMainViewport();

   // Draw the check boxes to show the demo window and console.
   //
   ImGuiWindowFlags window_flags
      = ImGuiWindowFlags_NoDecoration
      | ImGuiWindowFlags_AlwaysAutoResize
      | ImGuiWindowFlags_NoSavedSettings
      | ImGuiWindowFlags_NoFocusOnAppearing
      | ImGuiWindowFlags_NoNav
      | ImGuiWindowFlags_NoMove
      ;

   ImGui::SetNextWindowPos(ImVec2(10.0f, 10.0f), ImGuiCond_Always);
   ImGui::SetNextWindowBgAlpha(0.35f); // Transparent background
   bool always_open = true;
   if ( ImGui::Begin("Show Window Overlay", &always_open, window_flags) == true )
   {
      ImGui::Checkbox("GConsole", &show_gconsole);
      ImGui::SameLine();
      ImGui::Checkbox("Demo", &show_demo);
      ImGui::SameLine();
      ImGui::Checkbox("Ball", &show_ball);

      ImGui::SliderInt("##Lines", (s32 *)&max_pts, 2, 300, "Lines %d");
      ImGui::SameLine();
      ImGui::SliderFloat("##Speed", &speed, 0.01f, 1.0f, "Speed %.2f");

      ImGui::Separator();

      ImGui::Text("Window Size: %.1f, %.1f", view->Size.x, view->Size.y);

      if ( ImGui::IsMousePosValid() == true ) {
         ImGui::Text("Mouse: %.1f, %.1f", io.MousePos.x, io.MousePos.y);
      } else {
         ImGui::Text("Mouse: N/A");
      }
   }
   ImGui::End();

   // The demo-window is nice to have for development and documentation.
   if ( show_demo == true ) { ImGui::ShowDemoWindow(&show_demo); };
   if ( show_gconsole == true ) { imgui_console_window(pd); }


   // Draw a bouncing box on the foreground.
   //
   static float ballx = 100.0f;
   static float bally = 100.0f;
   const float ballsz = 10.0f;

   static float ball_xd = 4.2f;
   static float ball_yd = 7.7f;

   ImVec2 box_tl = view->Pos;
   ImVec2 box_br;
   box_tl.x += (ballx - ballsz);
   box_tl.y += (bally - ballsz);
   box_br.x = box_tl.x + (ballsz * 2);
   box_br.y = box_tl.y + (ballsz * 2);

   if ( show_ball == true ) {
      ImDrawList *fg = ImGui::GetForegroundDrawList();
      fg->AddRectFilled(box_tl, box_br, IM_COL32(255, 0, 0, 255), 2, 0);
   }

   ballx += ball_xd;
   bally += ball_yd;

   ImVec2 winsz = view->Size;
   winsz.x -= ballsz;
   winsz.y -= ballsz;

   if ( ballx < ballsz )  { ballx = ballsz;  ball_xd *= -1.0; }
   if ( ballx > winsz.x ) { ballx = winsz.x; ball_xd *= -1.0; }
   if ( bally < ballsz )  { bally = ballsz;  ball_yd *= -1.0; }
   if ( bally > winsz.y)  { bally = winsz.y; ball_yd *= -1.0; }


   // Draw some graphics on the background.
   //
   ImDrawList *bg = ImGui::GetBackgroundDrawList();

   s32 ox = 20;
   s32 oy = 160;
   s32 step = 20;
   s32 size = 600;
   float thickness = 2.0f;

   static xyz_rbam rbam;
   #define PT_ARRAY 302
   static pos_s points[PT_ARRAY];
   static linept_s pt1;
   static linept_s pt2;

   static bool is_init = false;
   static float rate = 1.0f;

   if ( is_init == false )
   {
      xyz_rbam_init(&rbam, PT_ARRAY);
      pt1.x = ox; pt1.y = oy;      pt1.dx = 0;    pt1.dy = step;
      pt2.x = ox; pt2.y = oy+size; pt2.dx = step; pt2.dy = 0;
      is_init = true;
   }

   if ( rate >= 1.0f )
   {
      rate = 0.0f;

      // Clear a point if necessary.
      while ( xyz_rbam_is_full(&rbam) == XYZ_TRUE || rbam.used > max_pts ) {
         xyz_rbam_read(&rbam);
      }

      // Add a new point.
      points[rbam.wr].pt1 = ImVec2((float)pt1.x, (float)pt1.y);
      points[rbam.wr].pt2 = ImVec2((float)pt2.x, (float)pt2.y);
      xyz_rbam_write(&rbam);

      // Update the points.
      pt1.x += pt1.dx; pt1.y += pt1.dy;
      if ( pt1.x > ox+size ) { pt1.x = ox+size; pt1.dx = 0; pt1.dy = -step; }
      if ( pt1.x < ox )      { pt1.x = ox     ; pt1.dx = 0; pt1.dy =  step; }
      if ( pt1.y > oy+size ) { pt1.y = oy+size; pt1.dy = 0; pt1.dx =  step; }
      if ( pt1.y < oy )      { pt1.y = oy     ; pt1.dy = 0; pt1.dx = -step; }

      pt2.x += pt2.dx; pt2.y += pt2.dy;
      if ( pt2.x > ox+size ) { pt2.x = ox+size; pt2.dx = 0; pt2.dy = -step; }
      if ( pt2.x < ox )      { pt2.x = ox     ; pt2.dx = 0; pt2.dy =  step; }
      if ( pt2.y > oy+size ) { pt2.y = oy+size; pt2.dy = 0; pt2.dx =  step; }
      if ( pt2.y < oy )      { pt2.y = oy     ; pt2.dy = 0; pt2.dx = -step; }
   }

   rate += speed;

   // Draw the points.
   u32 col = 255;

   for ( u32 i = rbam.rd ; i != rbam.wr ; )
   {
      bg->AddLine(points[i].pt1, points[i].pt2, IM_COL32(0, 0, col, 255), thickness);
      i = xyz_rbam_next(&rbam, i);
      col = (col < 10) ? 255 : (col - 10);
   }

}
// imgui_draw()


/**
 * Display the graphic console.
 *
 * Use an adjust as desired to support the program.
 *
 * @param[in] pd Pointer to the program data structure.
 */
static void
imgui_console_window(progdata_s *pd)
{
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
// imgui_console_window()


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
