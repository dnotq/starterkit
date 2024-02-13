/**
 * Display and I/O
 *
 * Initialize SDL2
 * Initialize ImGUI
 * Create the main application window
 * Create render thread
 *   Opens and binds an OpenGL context to the application window
 *   Main program pre render callback
 *   Render loop
 *     Main program UI drawing-callback
 *     Render ImGUI draw list
 *     Main program graphics drawing-callback
 *     Swap buffers
 * Enter event loop
 *   Wait on event
 *     Main program event callback
 *
 * The setup is based on the IMGUI SDL2+OpenGL3 example, so some of the
 * comments are from IMGUI.
 *
 * There are two rules when using SDL2 related to events and rendering:
 *
 *   1. The event loop must be on the same thread that creates the window.
 *   2. All rendering must be performed by the same thread.
 *
 * The design implemented here runs the event loop on the main process created
 * by the OS when the program was started, and a thread is created rendering
 * and calling drawing callbacks.
 *
 * This design has several desirable benefits:
 *
 *   1. Window drawing updates will continue even while mouse-blocking
 *      operations are happening, i.e. window dragging and resizing.
 *   2. The event loop can block entirely, or use a timeout, without blocking
 *      rendering.  This keeps program CPU utilization low, and the event
 *      responsiveness high.
 *
 * Compiled as C++ because of ImGUI.
 *
 * @file disco.cpp
 * @author Matthew Hagerty
 * @date April 23, 2021
 */


#include <stdbool.h>       // true, false

// Must come before other OpenGL headers.
#include "GL/gl3w.h"

// IMGUI has pre-built wrappers for many graphics subsystems.
#include "imgui.h"
#include "backends/imgui_impl_sdl2.h"
#include "backends/imgui_impl_opengl3.h"

#include "SDL.h"
#include "SDL_opengl.h"

#include "xyz.h"           // short types
#include "log.h"           // logfmt
#include "disco.h"

// Font headers can be built with the utility included with IMGUI.  See below.
#include "cousine_font.h"


// Define if running on a VirtualBox VM guest and VMSVGA is selected.
//#define SK_GL21_GLSL120


// ==========================================================================
//
// Local private globals and function declarations.
//

/// Generic size of a window title bar.
/// TODO Need a way to obtain this value prior to window creation.
#define TITLEBAR_HEIGHT 48

/// Maximum time in seconds to allow render thread to start.
#define RENDER_THD_STARTUP_TIMEOUT_SEC 5


/// Disco Data Structure.
typedef struct t_disco_data_s {

    SDL_Thread     *h_render;               ///< Render thread handle.
    SDL_Window     *window;                 ///< Pointer to the main application window.
    SDL_GLContext   gl_context;             ///< Pointer to the OpenGL SDL context.
    const c8       *glsl_version;           ///< GLSL version string.
    bool            running;                ///< Flag that disco is running.
    bool            render_thd_running;     ///< Flag that the rendering thread is running.

    struct {
    bool    have_audio;     ///< true if the audio subsystem is available.
    bool    have_timer;     ///< true if the timer subsystem is available.
    } sdl;

} dds_s;

/// Single disco data structure.
static dds_s g_disco_data;

/// Local text buffer to display errors.  A few lines of text should be enough.
static c8 g_errbuf[80*4];


static void disco_cleanup(void);
static bool disco_init_sdl(disco_s *disco);
static bool disco_create_window(disco_s *disco);
static void disco_event_loop(disco_s *disco);
static s32 disco_render_thread(void *arg);



// ====
// Public interface.
//


void
disco_program_exiting(void) {

    // Change disco state.
    g_disco_data.running = false;
}
// disco_program_exiting()


bool
disco_is_running(void) {
    return g_disco_data.running;
}
// disco_is_running()


/**
 * Run disco.
 *
 * @param[in] disco Pointer to an initalized disco_s settings structure.
 */
void
disco_run(disco_s *disco) {

    memset(&g_disco_data, 0, sizeof(g_disco_data));
    dds_s *dds = &g_disco_data; // convenience.

    if ( NULL == disco ) {
        goto DONE;
    }

    // TODO Set up console write call-back function.
    // TODO Find a place to write files and set up a log.

    // Set up singleton ImGui context.
    IMGUI_CHECKVERSION();
    if ( NULL == ImGui::CreateContext() ) {
        goto DONE;
    }

    // Set the IMGUI ini file and other options.
    ImGui::GetIO().IniFilename = disco->imgui_ini_filename;;
    ImGui::GetIO().ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard; // Enable Keyboard Controls
    //ImGui::GetIO().ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;  // Enable Gamepad Controls

    // Set up ImGui style.
    ImGui::StyleColorsDark();
    ImGui::GetStyle().FrameBorderSize = 1.0;
    ImGui::GetStyle().FrameRounding = 3.0;


    if ( false == disco_init_sdl(disco) ) {
        goto DONE;
    }

    if ( false == disco_create_window(disco) ) {
        goto DONE;
    }


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
    ImGui::GetIO().Fonts->AddFontFromMemoryCompressedBase85TTF(cousine_font_compressed_data_base85, 18);


    dds->h_render = SDL_CreateThread(disco_render_thread, "RenderThread", disco);
    if ( NULL == dds->h_render ) {
        const char *sdlerr = SDL_GetError();
        snlogfmt(g_errbuf, sizeof(g_errbuf),
            "Cannot continue: %s:%d SDL_CreateThread(RenderThread) "
            "failed with: [%s]", XYZ_CFL, sdlerr);
        logfmt("%s\n", g_errbuf);
        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, disco->prg_name, g_errbuf, NULL);
        goto DONE;
    }

    // Run the event loop. Does not return until program exits.
    disco_event_loop(disco);

DONE:
    disco_cleanup();
    return;
}
// disco_run()


// ====
// Private functions.
//


/**
 * Cleanup.
 */
static void
disco_cleanup(void) {

    dds_s *dds = &g_disco_data; // convenience.

    if ( true == dds->render_thd_running ) {
        dds->render_thd_running = false;
    }

    if ( NULL != dds->h_render ) {
        s32 render_rtn;
        SDL_WaitThread(dds->h_render, &render_rtn);
    }

    if ( NULL != dds->window ) {
        SDL_DestroyWindow(dds->window);
    }

    SDL_Quit();
}
// disco_cleanup()


/**
 * Initialize SDL.
 *
 * @param[in] disco Pointer to an initalized disco_s settings structure.
 *
 * @return true on success, otherwise false.
 */
static bool
disco_init_sdl(disco_s *disco) {

    dds_s *dds = &g_disco_data; // convenience.
    bool rtn = false;

    // Set up SDL.  Initialize individual SDL subsystems for better error
    // reporting and possible recovery.

    if ( 0 != SDL_InitSubSystem(SDL_INIT_VIDEO) ) {

        // The video (implies events) subsystem is critical.
        const char *sdlerr = SDL_GetError();
        snlogfmt(g_errbuf, sizeof(g_errbuf),
            "Cannot continue: %s:%d SDL_InitSubSystem(SDL_INIT_VIDEO) "
            "failed with: [%s]", XYZ_CFL, sdlerr);
        logfmt("%s\n", g_errbuf);
        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, disco->prg_name, g_errbuf, NULL);
        goto DONE;
    }

    // Be optimistic about subsystem initialization.
    dds->sdl.have_audio = true;
    dds->sdl.have_timer = true;

    // Audio and timer are not critical, for now.
    if ( SDL_InitSubSystem(SDL_INIT_AUDIO) != 0 ) {

        dds->sdl.have_audio = false;

        const char *sdlerr = SDL_GetError();
        snlogfmt(g_errbuf, sizeof(g_errbuf),
            "Warning: %s:%d SDL_InitSubSystem(SDL_INIT_AUDIO) "
            "failed with: [%s]", XYZ_CFL, sdlerr);
        logfmt("%s\n", g_errbuf);
        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, disco->prg_name, g_errbuf, NULL);
    }

    if ( 0 != SDL_InitSubSystem(SDL_INIT_TIMER) ) {

        dds->sdl.have_timer = false;

        const char *sdlerr = SDL_GetError();
        snlogfmt(g_errbuf, sizeof(g_errbuf),
            "Warning: %s:%d SDL_InitSubSystem(SDL_INIT_TIMER) "
            "failed with: [%s]", XYZ_CFL, sdlerr);
        logfmt("%s\n", g_errbuf);
        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, disco->prg_name, g_errbuf, NULL);
    }

    rtn = true;

DONE:

    return rtn;
}
// disco_init_sdl()


/**
 * Create the main window.
 *
 * @param[in] disco Pointer to an initalized disco_s settings structure.
 *
 * @return true if successful, otherwise false.
 */
static bool
disco_create_window(disco_s *disco) {

    dds_s *dds = &g_disco_data; // convenience.
    bool rtn = false; // default to error.

    // Decide GL+GLSL versions, from IMGUI example.
    #if defined(IMGUI_IMPL_OPENGL_ES2)
    // GL ES 2.0 + GLSL 100
    dds->glsl_version = "#version 100";
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, 0);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_ES);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);
    #elif defined(__APPLE__)
    // GL 3.2 Core + GLSL 150
    dds->glsl_version = "#version 150";
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, SDL_GL_CONTEXT_FORWARD_COMPATIBLE_FLAG); // Always required on Mac
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 2);
    #elif defined(SK_GL21_GLSL120)
    // GL 2.1 + GLSL 120 (VirtualBox VMSVGA environment)
    dds->glsl_version = "#version 120";
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, 0);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);
    #else
    // GL 3.0 + GLSL 130
    dds->glsl_version = "#version 130";
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, 0);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);
    #endif

    // Enable native Input Method Editor (IME) if available (since SDL 2.0.18).
    #ifdef SDL_HINT_IME_SHOW_UI
    SDL_SetHint(SDL_HINT_IME_SHOW_UI, "1");
    #endif

    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
    SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 8);


    // TODO Support for multiple monitors.

    // Make sure the window size is inside the usable display area.
    SDL_Rect usable_bounds;
    s32 display_index = 0;
    if ( 0 == SDL_GetDisplayUsableBounds(display_index, &usable_bounds) )
    {
        // TODO Currently the window size passed to SDL is the "client area" and
        //      does not include the height of any title-bar for a window.  There
        //      does not seem to be any way to get the title-bar size prior to
        //      creating the window, so reduce the usable height by a conservative
        //      amount for now.
        //      Alternative is to check the location the first time the window is
        //      is rendered and perform an adjustment.
        usable_bounds.h -= TITLEBAR_HEIGHT;
        usable_bounds.y += TITLEBAR_HEIGHT;

        if ( disco->winpos.h > usable_bounds.h ) {
            // Clamp to the window usable height.
            disco->winpos.h = usable_bounds.h;
            disco->winpos.y = usable_bounds.y;
        }

        else {
            // Convert a centered location to a screen location.
            if ( SDL_WINDOWPOS_CENTERED == disco->winpos.y ) {
                disco->winpos.y = (usable_bounds.h / 2) - (disco->winpos.h / 2);
            }

            // If the bottom-edge of the window is not visible, move up.
            s32 ymax = (disco->winpos.y + disco->winpos.h);
            if ( ymax > usable_bounds.h ) {
                disco->winpos.y = (usable_bounds.y + usable_bounds.h) - disco->winpos.h;
            }

            // If the top-edge of the window is not visible, move down.
            if ( disco->winpos.y < usable_bounds.y ) {
                disco->winpos.y = usable_bounds.y;
            }
        }


        if ( disco->winpos.w > usable_bounds.w ) {
            // Clamp to the window usable width.
            disco->winpos.w = usable_bounds.w;
            disco->winpos.x = usable_bounds.x;
        }

        else {
            // Convert a centered location to a screen location.
            if ( SDL_WINDOWPOS_CENTERED == disco->winpos.x ) {
                disco->winpos.x = (usable_bounds.w / 2) - (disco->winpos.w / 2);
            }

            // If the right-edge of the window is not visible, move left.
            s32 xmax = (disco->winpos.x + disco->winpos.w);
            if ( xmax > usable_bounds.w ) {
                disco->winpos.x = (usable_bounds.x + usable_bounds.w) - disco->winpos.w;
            }

            // If the left-edge of the window is not visible, move right.
            if ( disco->winpos.x < usable_bounds.x ) {
                disco->winpos.x = usable_bounds.x;
            }
        }
    }


    SDL_WindowFlags window_flags = (SDL_WindowFlags)
        ( SDL_WINDOW_OPENGL
        | SDL_WINDOW_RESIZABLE
        | SDL_WINDOW_ALLOW_HIGHDPI
        );

    // Create the main window.  When using SDL2, this is the process/thread that
    // will receive events.
    dds->window = SDL_CreateWindow(
        disco->prg_name,
        disco->winpos.x, disco->winpos.y, disco->winpos.w, disco->winpos.h,
        window_flags);

    if ( NULL == dds->window ) {
        const char *sdlerr = SDL_GetError();
        snlogfmt(g_errbuf, sizeof(g_errbuf),
            "Cannot continue: %s:%d SDL_CreateWindow(OPENGL) failed with: [%s]",
            XYZ_CFL, sdlerr);
        logfmt("%s\n", g_errbuf);
        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, disco->prg_name, g_errbuf, NULL);
        goto DONE;
    }


    // Set the running flag.
    dds->running = true;
    rtn = true;

DONE:
    return rtn;
}
// disco_create_window()


/**
 * Event loop, does not return until program ends.
 *
 * @param[in] disco Pointer to an initalized disco_s settings structure.
 */
static void
disco_event_loop(disco_s *disco) {

    dds_s *dds = &g_disco_data; // convenience.

    // Render thread start-up monitoring.
    u64 render_thd_start_time = SDL_GetPerformanceCounter();
    u64 perftime_freq = SDL_GetPerformanceFrequency();

    // Notes from ImGUI about events:
    //
    // You can read the io.WantCaptureMouse, io.WantCaptureKeyboard flags to
    // tell if IMGUI wants to use your inputs.
    // - When io.WantCaptureMouse is true, do not dispatch mouse input data
    //   to your main program.
    // - When io.WantCaptureKeyboard is true, do not dispatch keyboard input
    //   data to your main program.
    // Generally you may always pass all inputs to ImGUI and hide them from
    // your program based on those two flags.

    SDL_Event event;
    while ( true == dds->running && 0 != SDL_WaitEvent(&event) ) {

        disco->status.event_counter++;
        ImGui_ImplSDL2_ProcessEvent(&event);

        // Give the program event call-back a chance to handle events first.
        s32 handled = -1;
        if ( NULL != disco->callback.events ) {
            handled = disco->callback.events(&event);
        }

        if ( -1 == handled ) {

            // Check for quit or main window close events.
            if ( SDL_QUIT == event.type ) {
                dds->running = false;
            }

            if ( SDL_WINDOWEVENT == event.type
            && SDL_WINDOWEVENT_CLOSE == event.window.event
            && SDL_GetWindowID(dds->window) == event.window.windowID ) {
                dds->running = false;
            }
        }

        // Monitor the render thread to make sure it has started.
        if ( false == dds->render_thd_running ) {
            u64 now = SDL_GetPerformanceCounter();
            if ( RENDER_THD_STARTUP_TIMEOUT_SEC < ((now - render_thd_start_time) / perftime_freq) ) {
                // The rendering thread never signaled that it started.
                logfmt("%s:%d The render thread never signaled it was ready.\n", XYZ_CFL);
                dds->running = false;
            }
        }
    }

    return;
}
// disco_event_loop()


/**
 * Rendering thread.
 *
 * Separating the rendering from the event loop allows the display to continue
 * to update even while the event loop is blocking, for example when the
 * window is being dragged or resized.
 *
 * @param[in] arg Pointer to the program data structure.
 *
 * @return 0 on success, otherwise 1.
 */
static s32
disco_render_thread(void *arg)
{
    dds_s *dds = &g_disco_data; // convenience.

    // Set up a disco settings pointer.
    disco_s *disco = (disco_s *)(arg);
    s32 rtn = -1;
    bool have_vsync = true;

    // Calculate a divisor to get microseconds from the performance counter.
    u64 perftime_freq = SDL_GetPerformanceFrequency();
    u64 usec_divisor = perftime_freq / 1000000u;
    if ( 0 == usec_divisor || 1000u < usec_divisor ) {
        usec_divisor = 1;
    }

    // Create the OpenGL context and bind it to the main window.
    dds->gl_context = SDL_GL_CreateContext(dds->window);
    if ( NULL == dds->gl_context ) {
        const char *sdlerr = SDL_GetError();
        snlogfmt(g_errbuf, sizeof(g_errbuf),
            "Cannot continue: %s:%d SDL_GL_CreateContext() "
            "failed with: [%s]", XYZ_CFL, sdlerr);
        logfmt("%s\n", g_errbuf);
        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, disco->prg_name, g_errbuf, NULL);
        goto DONE;
    }

    SDL_GL_MakeCurrent(dds->window, dds->gl_context);

    // Ask for adaptive VSYNC, or normal VSYNC if not available.
    if ( 0 == SDL_GL_SetSwapInterval(/*adaptive_vsync*/ -1) ) {
        if ( 0 == SDL_GL_SetSwapInterval(/*wait_vsync*/ 1) ) {
            have_vsync = false;
        }
    }

    // Make sure a VSYNC setting was actually accepted.
    if ( 0 == SDL_GL_GetSwapInterval() ) {
        have_vsync = false;
    }

    // Initialize a local (not imgui) OpenGL loader.
    if ( 0 != gl3wInit() ) {
        logfmt("%s:%d gl3wInit() failed to load OpenGL functions.\n", XYZ_CFL);
        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, disco->prg_name,
            "Cannot continue: gl3wInit() failed to load OpenGL functions.", NULL);
        goto DONE;
    }

    // Set up ImGui platform and renderer bindings.
    ImGui_ImplSDL2_InitForOpenGL(dds->window, dds->gl_context);
    ImGui_ImplOpenGL3_Init(dds->glsl_version);

    if ( NULL != disco->callback.draw_init ) {
        if ( 0 != disco->callback.draw_init(/*arg*/ NULL) ) {
            goto DONE;
        }
    }

    // Indicate that the rendering thread is running.
    dds->render_thd_running = true;

    // Render until signaled to quit.
    while ( dds->running == true ) {

        disco->status.minimized =
            (SDL_WINDOW_MINIMIZED == (SDL_GetWindowFlags(dds->window) & SDL_WINDOW_MINIMIZED));

        // Do not render if minimized.
        if ( true == disco->status.minimized ) {

            if ( NULL != disco->callback.draw_ui ) {
                disco->callback.draw_ui(/*arg*/ NULL);
            }
            SDL_Delay(/*16ms*/ 16);
            continue;
        }

        u64 start = SDL_GetPerformanceCounter();
        u64 start_ms = SDL_GetTicks64();

        // Start a new ImGui frame.
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplSDL2_NewFrame();
        ImGui::NewFrame();

        // Get updated window information each frame.
        SDL_GetWindowPosition(dds->window, &(disco->winpos.x), &(disco->winpos.y));
        disco->winpos.w = (s32)ImGui::GetIO().DisplaySize.x;
        disco->winpos.h = (s32)ImGui::GetIO().DisplaySize.y;

        ImVec4 bgcol = ImColor(disco->bgcolor.r, disco->bgcolor.g, disco->bgcolor.b, disco->bgcolor.a);
        glClearColor(bgcol.x, bgcol.y, bgcol.z, bgcol.w);
        glClear(GL_COLOR_BUFFER_BIT);

        // Drawing UI callback.
        if ( NULL != disco->callback.draw_ui ) {
            disco->callback.draw_ui(/*arg*/ NULL);
        }

        // Render() calls EndFrame(). Prepares IMGUI data for drawing.
        ImGui::Render();
        glViewport(0, 0, disco->winpos.w, disco->winpos.h);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        // Drawing post UI callback.
        if ( NULL != disco->callback.draw_post_ui ) {
            disco->callback.draw_post_ui(/*arg*/ NULL);
        }

        if ( false == have_vsync ) {
            // Running in a virtual machine there may be no hardware VSYNC.
            // Just do something simple.  SDL3 has a nanosecond delay that
            // should help make this more accurate.
            while ( /*14ms*/ 14 > (SDL_GetTicks64() - start_ms) ) {
                SDL_Delay(/*1ms*/ 1);
            }
        }

        // Track render time separate from frame time.
        disco->status.render_time_us = (SDL_GetPerformanceCounter() - start) / usec_divisor;

        // The buffer swap may wait for VSYNC, which effects the frame time.
        SDL_GL_SwapWindow(dds->window);
        disco->status.render_counter++;
        disco->status.frame_time_us = (SDL_GetPerformanceCounter() - start) / usec_divisor;
    }

    rtn = 0;

DONE:

    // Cleanup.
    if ( NULL != disco->callback.draw_cleanup ) {
        disco->callback.draw_cleanup(/*arg*/ NULL);
    }

    if ( NULL != dds->gl_context ) {
        ImGui_ImplOpenGL3_Shutdown();
        ImGui_ImplSDL2_Shutdown();
        ImGui::DestroyContext();
        SDL_GL_DeleteContext(dds->gl_context);
    }

    return rtn;
}
// disco_render_thread()


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
