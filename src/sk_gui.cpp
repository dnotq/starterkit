/**
 * Starter Kit GUI.
 *
 * This file is C++ because IMGUI is written with a mix of C and C++ styles.
 *
 * @file sk_gui.cpp
 * @author Matthew Hagerty
 * @date Jan 16, 2020
 */

#include <stdio.h>          // stdout, stderr
#include <stdarg.h>         // va_args, vfprint

// IMGUI has pre-build wrappers for many graphics subsystems.
#include "imgui.h"
#include "examples/imgui_impl_sdl.h"
#include "examples/imgui_impl_opengl2.h"

// SDL2
#include "SDL.h"
#include "SDL_opengl.h"

// The stb_sprintf implementation is included by IMGUI when STB use is enabled
// by defining IMGUI_USE_STB_SPRINTF, which is done in the CMakeFiles.txt.
#include "stb_sprintf.h"    // stbsp_snprintf

// This header includes the declarations for the public GUI API functions.
#include "starterkit.h"

// The header can be built with the utility included with IMGUI.  See below.
#include "cousine_font.h"


//
// Local private function forward declarations.
//

static void draw_gui(app_s *app);
static void example_panel(app_s *app);


/**
 * Initialize, create a window, and run the GUI.
 *
 * Blocks until the GUI is exited.
 *
 * @param app       Pointer to the application data structure.
 *
 * @return 0 on success, otherwise a negative error code.
 */
int
sk_gui_run(app_s *app)
{
    // Setup SDL
    if ( SDL_Init(SDL_INIT_EVERYTHING) != 0 )
    {
        SK_LOG_CRIT(app, "%s", SDL_GetError());
        return -1;
    }

    // Setup window.
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
    SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 8);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 2);

    SDL_WindowFlags window_flags = (SDL_WindowFlags)
        (SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI);

    const int APP_TITLE_MAX = 80;
    char title[APP_TITLE_MAX];
    stbsp_snprintf(title, sizeof(title), "%s v%d.%d", SK_APP_NAME, SK_VER_MAJOR, SK_VER_MINOR);
    SDL_Window *window = SDL_CreateWindow(
        (const char *)title,
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        SK_WINDOW_WIDTH, SK_WINDOW_HEIGHT, window_flags);

    SDL_GLContext gl_context = SDL_GL_CreateContext(window);
    SDL_GL_MakeCurrent(window, gl_context);

    const int UPDATE_WITH_VSYNC = 1;
    SDL_GL_SetSwapInterval(UPDATE_WITH_VSYNC);

    // Set up ImGui context.
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO &io = ImGui::GetIO();

    // Set up ImGui style.
    ImGui::StyleColorsDark();
    ImGuiStyle& style = ImGui::GetStyle();
    style.FrameBorderSize = 1.0f;
    style.FrameRounding = 3.0;

    // Set up ImGui platform and renderer bindings.
    ImGui_ImplSDL2_InitForOpenGL(window, gl_context);
    ImGui_ImplOpenGL2_Init();

    // Add extra font and set as default.  The built-in "proggy-clean" font is
    // fixed to 13-point and is too small on larger monitors.
    //
    // "Cousine" font has slashed zeros, so it wins.
    // "Roboto" font is also nice looking, as well as "Karla".
    //
    // The fixed-point font header is generated via binary_to_compressed_c.cpp
    // from the true-type font file and included above.
    //
    //ImFont *roboto_font = io.Fonts->AddFontFromMemoryCompressedBase85TTF(roboto_font_base85, 16);
    // TODO font size 18 might be too big on smaller displays.
    io.Fonts->AddFontFromMemoryCompressedBase85TTF(cousine_font_compressed_data_base85, 18);

    // Background color.
    // TODO customize or make available for user to change.
    ImVec4 clear_color = ImColor(128, 128, 128);


    // Set the running flag and wait for the Application.
    // TODO Add timeout and error.
    app->gui.running = XYZ_TRUE;
    while ( app->app.running == XYZ_FALSE )
    {
        // Yield and wait for the application thread to start.
        SDL_Event event;
        const int WAIT_100MS = 100;
        if ( SDL_WaitEventTimeout(&event, WAIT_100MS) != 0 ) {
            ImGui_ImplSDL2_ProcessEvent(&event);
        }
    }


    // This GUI loop is a hybrid between a game-style loop which polls, and a
    // purely event-based loop which always blocks waiting for events.
    //
    // The loop will block for a maximum time waiting for events, and if no
    // events are received it will go ahead and run through the loop once to
    // allow non-event type activity to be performed.
    //
    // This format makes the loop adaptable and it becomes more responsive when
    // there are a lot of events, i.e. a lot of user interaction, since the
    // event-wait call will return immediately.  Where there are no events, the
    // loop scales back to spending most of its time waiting, which keeps its
    // CPU use low like most event-only applications.

    while ( app->gui.running == XYZ_TRUE )
    {
        const int WAIT_20MS = 20;
        SDL_Event event;
        SDL_ClearError();

        while ( SDL_WaitEventTimeout(&event, WAIT_20MS) != 0 )
        {
            // Process all events.
            do
            {
                ImGui_ImplSDL2_ProcessEvent(&event);
                if ( event.type == SDL_QUIT || app->app.running == XYZ_FALSE )
                {
                    app->gui.running = XYZ_FALSE;
                    sk_app_ext_signal(app);
                }
            } while ( SDL_PollEvent(&event) == 1 );

            // Make sure to allow other code to run.
            break;
        }

        // SDL_WaitEventTimeout() returns 0 for both a timeout or an error.
        // The only way to know if there was an error is to check for one.
        const char *sdlerr = SDL_GetError();
        if ( sdlerr != NULL ) {
            // TODO bad stuff.
        }

        // Start an ImGui frame.
        ImGui_ImplOpenGL2_NewFrame();
        ImGui_ImplSDL2_NewFrame(window);
        ImGui::NewFrame();


        // Application specific UI drawing.
        draw_gui(app);


        // Rendering.
        ImGui::Render();
        glViewport(0, 0, (int)io.DisplaySize.x, (int)io.DisplaySize.y);
        glClearColor(clear_color.x, clear_color.y, clear_color.z, clear_color.w);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL2_RenderDrawData(ImGui::GetDrawData());
        SDL_GL_SwapWindow(window);
    }

    // Cleanup.
    ImGui_ImplOpenGL2_Shutdown();
    ImGui_ImplSDL2_Shutdown();
    ImGui::DestroyContext();

    SDL_GL_DeleteContext(gl_context);
    SDL_DestroyWindow(window);
    SDL_Quit();

    return 0;
}
// sk_gui_run()


// ==========================================================================
//
// Local private functions.
//
// ==========================================================================


/**
 * Draw the GUI.
 *
 * @param app       Pointer to the application data structure.
 */
static void
draw_gui(app_s *app)
{
    // Make a window.
    ImGui::Begin("Example");
    example_panel(app);
    ImGui::End();
}
// draw_gui()


/**
 * Create an example panel.
 *
 * @param app       Pointer to the application data structure.
 */
static void
example_panel(app_s *app)
{
    static bool show_demo_window = false;

    // Note, for ImGui, the ## before the control ID hides the label.

    if ( show_demo_window == true ) {
        ImGui::ShowDemoWindow(&show_demo_window);
    }

    ImGui::Text("Fill this space with cool stuff!");

    if ( ImGui::Button("Click") == true ) {
        sk_app_ext_signal(app);
    }
    if ( ImGui::IsItemHovered() )
    {
        // Hover applies to the last control, i.e. the button.
        ImGui::BeginTooltip();
        ImGui::Text("Really, click this button and watch the terminal window for a message.");
        ImGui::EndTooltip();
    }

    ImGui::Checkbox("Demo Window", &show_demo_window);
    if ( ImGui::IsItemHovered() )
    {
        ImGui::BeginTooltip();
        ImGui::Text("Check this box to explore the ImGUI Demo Window.");
        ImGui::EndTooltip();
    }


    ImGui::Text("Application average %.3f ms/frame (%.1f FPS)",
        1000.0f / ImGui::GetIO().Framerate, ImGui::GetIO().Framerate);

}
// example_panel()


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

