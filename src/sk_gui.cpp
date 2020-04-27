/**
 * Starter Kit GUI.
 *
 * This file is C++ because IMGUI is written with a mix of C and C++ styles.
 *
 * @file sk_gui.cpp
 * @author Matthew Hagerty
 * @date Jan 16, 2020
 */


#include <stdio.h>          // fprintf, stdout, stderr
#include <stdarg.h>         // va_args, vfprint

// IMGUI has pre-build wrappers for many graphics subsystems.
#define IMGUI_IMPL_OPENGL_LOADER_GL3W
#include "imgui.h"
#include "examples/imgui_impl_sdl.h"
#include "examples/imgui_impl_opengl3.h"

// SDL2
#include "SDL.h"
//#include "SDL_opengl.h" // Do not use with OpenGL3 / GL3W


// OpenGL 3.x complicates things compared to using OpenGL 2.x.
//
// About Desktop OpenGL function loaders:
//  Modern desktop OpenGL doesn't have a standard portable header file to load OpenGL function pointers.
//  Helper libraries are often used for this purpose! Here we are supporting a few common ones (gl3w, glew, glad).
//  You may use another loader/header of your choice (glext, glLoadGen, etc.), or chose to manually implement your own.
#if defined(IMGUI_IMPL_OPENGL_LOADER_GL3W)
#include <GL/gl3w.h>            // Initialize with gl3wInit()
#elif defined(IMGUI_IMPL_OPENGL_LOADER_GLEW)
#include <GL/glew.h>            // Initialize with glewInit()
#elif defined(IMGUI_IMPL_OPENGL_LOADER_GLAD)
#include <glad/glad.h>          // Initialize with gladLoadGL()
#elif defined(IMGUI_IMPL_OPENGL_LOADER_GLBINDING2)
#define GLFW_INCLUDE_NONE       // GLFW including OpenGL headers causes ambiguity or multiple definition errors.
#include <glbinding/Binding.h>  // Initialize with glbinding::Binding::initialize()
#include <glbinding/gl/gl.h>
using namespace gl;
#elif defined(IMGUI_IMPL_OPENGL_LOADER_GLBINDING3)
#define GLFW_INCLUDE_NONE       // GLFW including OpenGL headers causes ambiguity or multiple definition errors.
#include <glbinding/glbinding.h>// Initialize with glbinding::initialize()
#include <glbinding/gl/gl.h>
using namespace gl;
#else
#include IMGUI_IMPL_OPENGL_LOADER_CUSTOM
#endif


// The stb_sprintf implementation is included by IMGUI when STB use is enabled
// by defining IMGUI_USE_STB_SPRINTF, which is done in the CMakeFiles.txt.
#include "stb_sprintf.h"    // stbsp_snprintf

// This header includes the declarations for the public GUI API functions.
#include "starterkit.h"

// The header can be built with the utility included with IMGUI.  See below.
#include "cousine_font.h"



// ==========================================================================
//
// Local private globals and function declarations.
//


/// Pointer to the main application window.
static SDL_Window * g_window;

/// GLSL version string.
static const char* g_glsl_version;

/// Flag for the rendering thread.
static int g_renderer_running;


// Local private function forward declarations.
//

static void draw_gui(app_s *app);
static void example_panel(app_s *app);
static int render_thread(void *arg);


/**
 * Initialize, create a window, and run the GUI.
 *
 * Blocks until the GUI is exited.
 *
 * @param app       Pointer to the application data structure.
 *
 * @return 0 on success, otherwise a non-zero error code.
 */
int
sk_gui_run(app_s *app)
{
    // Setup SDL
    if ( SDL_Init(SDL_INIT_EVERYTHING) != 0 )
    {
        SK_LOG_CRIT(app, "%s", SDL_GetError());
        return 1;
    }

    // Decide GL+GLSL versions
#if __APPLE__
    // GL 3.2 Core + GLSL 150
    g_glsl_version = "#version 150";
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, SDL_GL_CONTEXT_FORWARD_COMPATIBLE_FLAG); // Always required on Mac
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 2);
#else
    // GL 3.0 + GLSL 130
    g_glsl_version = "#version 130";
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, 0);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);
#endif


    // Set SDL and window options for OpenGL.
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
    SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 8);

    SDL_WindowFlags window_flags = (SDL_WindowFlags)
        (SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI);

    // Create the main window.
    const int APP_TITLE_MAX = 80;
    char title[APP_TITLE_MAX];
    stbsp_snprintf(title, sizeof(title), "%s v%d.%d", SK_APP_NAME, SK_VER_MAJOR, SK_VER_MINOR);

    g_window = SDL_CreateWindow(
        (const char *)title,
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        SK_WINDOW_WIDTH, SK_WINDOW_HEIGHT, window_flags);


    // Set up singleton ImGui context.
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();


    // Create the rendering thread.
    g_renderer_running = XYZ_FALSE;
    SDL_Thread *render_h;
    render_h = SDL_CreateThread(render_thread, "RenderThread", (void *)app);
    if ( render_h == NULL )
    {
        SK_LOG_CRIT(app, "%s", SDL_GetError());
        return 2;
    }


    // Run the event loop until the user or the application exits.
    app->gui.running = XYZ_TRUE;
    while ( app->gui.running == XYZ_TRUE )
    {
        int sanity = 100;
        const int EVENT_WAIT_MS_MAX = 100;
        SDL_Event event;
        SDL_ClearError();

        while ( SDL_WaitEventTimeout(&event, EVENT_WAIT_MS_MAX) != 0 )
        {
            // Let ImGui update its state based on events.
            ImGui_ImplSDL2_ProcessEvent(&event);

            // Application event processing.
            if ( event.type == SDL_QUIT || app->app.running == XYZ_FALSE )
            {
                app->gui.running = XYZ_FALSE;
                g_renderer_running = XYZ_FALSE;
                sk_app_ext_signal(app);
            }


            // Make sure the main outer-loop (and/or other code) has a chance
            // to evaluate in the case where a constant stream of messages
            // would prevent the event-wait timeout.
            sanity--;
            if ( sanity <= 0 ) {
                break;
            }
        }

        // SDL_WaitEventTimeout() returns 0 for both a timeout or an error.
        // The only way to know if there was an error is to check for one.
        const char *sdlerr = SDL_GetError();
        if ( sdlerr != NULL && sdlerr[0] != XYZ_NTERM ) {
            // TODO bad stuff.
            SK_LOG_WARN(app, "SDL SDL_WaitEventTimeout error [%s]", sdlerr);
        }
    }


    // Wait for the render thread to stop.
    int render_rtn;
    SK_LOG_DBG(app, "Waiting for render thread to exit.");
    SDL_WaitThread(render_h, &render_rtn);
    if ( render_rtn == 0 ) {
        SK_LOG_DBG(app, "Render thread joined.");
    }

    else
    {
        SK_LOG_DBG(app, "Render thread joined, but returned an error [%d].", render_rtn);

        // Differentiate between render thread errors and this function's
        // return value.
        render_rtn = -render_rtn;
    }

    // Cleanup.
    SDL_DestroyWindow(g_window);
    SDL_Quit();

    return render_rtn;
}
// sk_gui_run()


// ==========================================================================
//
// Local private functions.
//
// ==========================================================================


/**
 * Rendering thread.
 *
 * Separating the rendering from the event loop allows the display to continue
 * to update even while the event loop is blocking, for example when the
 * window is being dragged or resized.
 *
 * @param arg       Pointer to the application data structure.
 *
 * @return 0 on successful termination, otherwise a non-zero error code.
 */
static int
render_thread(void *arg)
{
    if ( arg == NULL ) {
        return 1;
    }

    // Get app structure pointer.
    app_s *app = (app_s *)(arg);

    SK_LOG_DBG(app, "Render thread started.");


    // Create the OpenGL context and bind it to the main window.
    SDL_GLContext gl_context = SDL_GL_CreateContext(g_window);
    SDL_GL_MakeCurrent(g_window, gl_context);

    const int UPDATE_WITH_VSYNC = 1;
    SDL_GL_SetSwapInterval(UPDATE_WITH_VSYNC);


    // Initialize the OpenGL loader.
#if defined(IMGUI_IMPL_OPENGL_LOADER_GL3W)
    bool gl_init_err = gl3wInit() != 0;
#elif defined(IMGUI_IMPL_OPENGL_LOADER_GLEW)
    bool gl_init_err = glewInit() != GLEW_OK;
#elif defined(IMGUI_IMPL_OPENGL_LOADER_GLAD)
    bool gl_init_err = gladLoadGL() == 0;
#elif defined(IMGUI_IMPL_OPENGL_LOADER_GLBINDING2)
    bool gl_init_err = false;
    glbinding::Binding::initialize();
#elif defined(IMGUI_IMPL_OPENGL_LOADER_GLBINDING3)
    bool gl_init_err = false;
    glbinding::initialize([](const char* name) { return (glbinding::ProcAddress)SDL_GL_GetProcAddress(name); });
#else
    bool gl_init_err = false; // If you use IMGUI_IMPL_OPENGL_LOADER_CUSTOM, your loader is likely to require some form of initialization.
#endif
    if ( gl_init_err == true )
    {
        SK_LOG_CRIT(app, "Failed to initialize OpenGL loader.");
        return 2;
    }


    // Set ImGui I/O options.
    ImGuiIO &io = ImGui::GetIO();
    //io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;     // Enable Keyboard Controls
    //io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;      // Enable Gamepad Controls

    // Set up ImGui style.
    ImGui::StyleColorsDark();
    ImGuiStyle& style = ImGui::GetStyle();
    style.FrameBorderSize = 1.0f;
    style.FrameRounding = 3.0;

    // Set up ImGui platform and renderer bindings.
    ImGui_ImplSDL2_InitForOpenGL(g_window, gl_context);
    ImGui_ImplOpenGL3_Init(g_glsl_version);


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


    // Render until signaled to quit.
    g_renderer_running = XYZ_TRUE;
    while ( g_renderer_running == XYZ_TRUE )
    {
        // This delay helps keep CPU use a little lower (~ 10% vs 20%), and
        // should not affect the frame rate unless the frame calculation time
        // is getting close to the actual frame rate.  Can be removed if CPU
        // use of no concern, of if necessary for application performance.
        SDL_Delay(10);

        // Start an ImGui frame.
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplSDL2_NewFrame(g_window);
        ImGui::NewFrame();


        // Application specific UI drawing.
        draw_gui(app);


        // Rendering.
        ImGui::Render();
        glViewport(0, 0, (int)io.DisplaySize.x, (int)io.DisplaySize.y);
        glClearColor(clear_color.x, clear_color.y, clear_color.z, clear_color.w);
        glClear(GL_COLOR_BUFFER_BIT);

        // Render the ImGui UI.
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        // Swapping buffers is timed with VSYNC, so this will keep the whole
        // render-loop at a constant frame rate.
        SDL_GL_SwapWindow(g_window);
    }


    // Cleanup.
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplSDL2_Shutdown();
    ImGui::DestroyContext();

    SDL_GL_DeleteContext(gl_context);

    return 0;
}
// render_thread()


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
