/**
 * Very thin wrapping of any needed C++ functions so they can be called from C.
 *
 * @file interface.cpp
 * @date Jun 5, 2021
 * @author Matthew Hagerty
 */


#include "SDL.h"
#include "imgui.h"
#include "wrap_c2cpp.h"
#include "program.h"



void
imgui_ShowDemoWindow(u32 *show)
{
   bool _show = *show == 0 ? false : true;

   ImGui::ShowDemoWindow(&_show);
   *show = _show == false ? 0 : 1;
}
// imgui_ShowDemoWindow
