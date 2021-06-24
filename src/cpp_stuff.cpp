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


/**
 * Draw the IMGUI stuff, since it needs C++.
 *
 * @param[in] pd Pointer to the program data structure.
 */
void
imgui_draw(progdata_s *pd)
{
   (void)pd; // not currently used.


   // The demo is nice to have for development and documentation.
   bool show_demo = true;
   ImGui::ShowDemoWindow(&show_demo);
   imgui_console_window(pd);

   // Draw a bouncing box, just for fun.
   static float x = 100.0f;
   static float y = 100.0f;
   const float sz = 10.0f;

   static float xd = 4.2f;
   static float yd = 7.7f;

   ImGuiViewport *view = ImGui::GetMainViewport();
   ImVec2 box_tl = view->Pos;
   ImVec2 box_br;
   box_tl.x += (x - sz);
   box_tl.y += (y - sz);
   box_br.x = box_tl.x + (sz * 2);
   box_br.y = box_tl.y + (sz * 2);

   ImDrawList *bg = ImGui::GetBackgroundDrawList();
   bg->AddRectFilled(box_tl, box_br, IM_COL32(255, 0, 0, 255), 2, 0);

   x += xd;
   y += yd;

   ImVec2 winsz = view->Size;
   winsz.x -= sz;
   winsz.y -= sz;

   if ( x < sz )      { x = sz;      xd *= -1.0; }
   if ( x > winsz.x ) { x = winsz.x; xd *= -1.0; }
   if ( y < sz )      { y = sz;      yd *= -1.0; }
   if ( y > winsz.y)  { y = winsz.y; yd *= -1.0; }


   c8 buf[40];
   s32 len;
   ImVec2 p(10, 10);
   len = stbsp_snprintf(buf, sizeof(buf), "winsz: %f, %f", winsz.x, winsz.y);
   bg->AddText(p, IM_COL32(255, 255, 0, 255), buf, (buf + len));


   {
      s32 tilesx = 20;
      s32 tilesy = 20;
      s32 cellsz = 30;
      s32 offx = 40;
      s32 offy = 100;

      ImGuiIO &io = ImGui::GetIO();
      s32 mouse_x = -1;
      s32 mouse_y = -1;
      if ( io.WantCaptureMouse == false && ImGui::IsMousePosValid() == true &&
           io.MousePos.x > offx && io.MousePos.y > offy )
      {
          mouse_x = (s32)((io.MousePos.x - offx) / cellsz);
          mouse_y = (s32)((io.MousePos.y - offy) / cellsz);
      }

      p = {10, 24};
      len = stbsp_snprintf(buf, sizeof(buf), "x,y: %f, %f", io.MousePos.x, io.MousePos.y);
      len = (len > 0 && len < (s32)sizeof(buf)) ? len : (s32)sizeof(buf) - 1;
      bg->AddText(p, IM_COL32(255, 255, 0, 255), buf, (buf + len));
      p = {10, 38};
      len = stbsp_snprintf(buf, sizeof(buf), "mx,my: %d, %d", mouse_x, mouse_y);
      bg->AddText(p, IM_COL32(255, 255, 0, 255), buf, (buf + len));


      ImVec2 tpos = {view->Pos.x + offx, view->Pos.y + offy};
      ImDrawListFlags orig_flags = bg->Flags;
      bg->Flags = ImDrawListFlags_None;

      for ( s32 yi = 0 ; yi < tilesy ; yi++ )
      {
         for ( s32 xi = 0 ; xi < tilesx ; xi++ )
         {
            ImVec2 cell  = {tpos.x + xi * cellsz, tpos.y + yi * cellsz};
            ImVec2 cell2 = {cell.x + cellsz, cell.y + cellsz};

            if ( mouse_x == xi && mouse_y == yi ) {
               cell.x += 4; cell.y += 4;
               cell2.x -= 3; cell2.y -= 3;
               bg->AddRectFilled(cell, cell2, IM_COL32(255, 255, 0, 255), 4, 0);
            } else {
               bg->AddRect(cell, cell2, IM_COL32(0, 0, 255, 255), 0, 0, 1.0f);
            }
         }
      }

      bg->Flags = orig_flags;

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
   // TODO 840x680 console size.  Add vars in cons struct to load at startup.

   // Make a window for the graphic console.
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
