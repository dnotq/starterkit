/**
 * Draw the user interface.
 *
 * This is very much just a quick hack / messing around.
 *
 * @file draw_ui.cpp
 * @date Jun 5, 2021
 * @author Matthew Hagerty
 */


#include <stdbool.h>        // true, false

#include "SDL.h"            // SDL2
#include "imgui.h"          // IMGUI
#include "xyz.h"            // short types
#include "draw_ui.h"
#include "program.h"
#include "math.h"
#include "cidx.h"


/// Moving point structure for the line example.
typedef struct t_linept_s {
    s32 x;
    s32 y;
    s32 dx;
    s32 dy;
} linept_s;

/// A pair of points for a line.
typedef struct t_pos_s {
    ImVec2 pt1;
    ImVec2 pt2;
} pos_s;


/**
 * Draw the UI.
 *
 * @param[in] pd Pointer to the program data structure.
 */
void
draw_imgui_ui(pds_s *pds) {

    static bool show_gconsole = false;
    static bool show_demo = false;
    static bool show_ball = true;

    static float speed = 0.5;
    static u32 max_pts = 30;
    #define PT_ARRAY_SIZE 120

    static u32 framecnt = 0;
    static f64 sum_hz = 0;
    static f64 avg_hz = 0; // average Hz


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

        ImGui::SliderInt("##Lines", (s32 *)&max_pts, 2, PT_ARRAY_SIZE, "Lines %d");
        ImGui::SameLine();
        ImGui::SliderFloat("##Speed", &speed, 0.01f, 1.0f, "Speed %.2f");

        ImGui::Separator();

        ImGui::Text("Window Size: %.1f, %.1f", view->Size.x, view->Size.y);
        ImGui::SameLine();
        f64 usec = (f64)pds->disco.status.frame_time_us;
        sum_hz += 1 / (usec / (1000 * 1000));
        framecnt++;
        if ( 16 == framecnt ) {
            avg_hz = sum_hz / (f64)framecnt;
            sum_hz = 0;
            framecnt = 0;
        }
        ImGui::Text("Frame rate: %.2fHz, %.3fms", avg_hz, (usec / 1000));

        if ( ImGui::IsMousePosValid() == true ) {
            ImGui::Text("Mouse: %.1f, %.1f", io.MousePos.x, io.MousePos.y);
        } else {
            ImGui::Text("Mouse: N/A");
        }

        //float w = ImGui::GetContentRegionAvail().x - ImGui::GetStyle().ItemSpacing.y;
        //ImGui::SetNextItemWidth(w);
        ImGui::SliderAngle("rot x", &pds->model.xrot, -180, 180, "%.1f");
        ImGui::SameLine();
        ImGui::SliderFloat("cam x", &pds->camera.x, -180, 180, "%.0f");

        ImGui::SliderAngle("rot y", &pds->model.yrot);
        ImGui::SameLine();
        ImGui::SliderFloat("cam y", &pds->camera.y, -180, 180, "%.0f");

        ImGui::SliderAngle("rot z", &pds->model.zrot);
        ImGui::SameLine();
        ImGui::SliderFloat("cam z", &pds->camera.z, -180, 180, "%.0f");

    }
    ImGui::End();

    #if 1
    // The demo-window is nice to have for development and documentation.
    if ( show_demo == true ) {
        ImGui::ShowDemoWindow(&show_demo);
    };
    #endif
    #if HAVE_CONSOLE_UI_WINDOW
    if ( show_gconsole == true ) {
        console_ui_window(pd);
    }
    #endif

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

    static cidx_s circular_index;
    static cidx_s *ci = &circular_index; // convenience
    static pos_s points[PT_ARRAY_SIZE];
    static linept_s pt1;
    static linept_s pt2;

    static bool is_init = false;
    static float rate = 1.0f;

    if ( is_init == false ) {
        cidx_init(ci, sizeof(pos_s), points, PT_ARRAY_SIZE);
        pt1.x = ox; pt1.y = oy;      pt1.dx = 0;    pt1.dy = step;
        pt2.x = ox; pt2.y = oy+size; pt2.dx = step; pt2.dy = 0;
        is_init = true;
    }

    if ( 1.0f <= rate )
    {
        rate = 0.0f;

        // Clear a point if necessary.
        while ( true == cidx_isfull(ci) || max_pts < cidx_used(ci) ) {
            cidx_consume(ci);
        }

        // Add a new point.
        pos_s *newpt = (pos_s *)cidx_we(ci);
        newpt->pt1 = ImVec2((float)pt1.x, (float)pt1.y);
        newpt->pt2 = ImVec2((float)pt2.x, (float)pt2.y);
        cidx_commit(ci);

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

    if ( 0 < cidx_used(ci) ) {

        // Draw the points.
        u32 cinc = 255 / (u32)cidx_used(ci);
        cinc = (0 == cinc ? 1 : cinc);
        u32 col = cinc;

        size_t i = ci->rd;
        do {
            bg->AddLine(points[i].pt1, points[i].pt2, IM_COL32(0, 0, col, 255), thickness);
            i = cidx_next(ci, i);
            col += cinc;
        } while ( i != ci->wr );
    }

}
// draw_ui()


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
