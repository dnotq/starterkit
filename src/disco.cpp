/**
 * Display and I/O
 *
 * Creates a basic platform layer to separate the graphic display, keyboard, and
 * mouse input from the host program functionality.  The separation could be
 * more complete if the host program implements abstraction functions for
 * drawing, I/O, etc.  That detail is left up to the designer of the host.
 *
 * The setup is based on the IMGUI SDL2 example, so some of the comments and
 * such are from IMGUI.
 *
 * There are two rules for SDL2 related to events and rendering:
 *
 *   1. The event loop must be on the same thread that creates the window.
 *   2. All rendering must be performed by the same thread.
 *
 * The design implemented here manages the event loop on the main process
 * thread, and creates a dedicated thread for rendering.  This allows screen
 * updates to take place while blocking mouse operations are taking place, i.e.
 * window dragging, resizing, etc.
 *
 * The rendering thread will call a host program provided function when screen
 * drawing is required.
 *
 * The decision of where to manage event handling is up to the designer.  It
 * could be done here to help keep the host program free of platform specific
 * code, or if that is not a concern then the event structures can be passed to
 * the host program for direct processing.
 *
 * For the main program, the event handler could call it directly, or it can be
 * set up as a thread so it can run independent from events and rendering.  This
 * decision and implementation is left to the host program designer.  The method
 * used here is a main host program thread, but it does not have to be this way.
 *
 * Compiled as C++ because of IMGUI.
 *
 * @file disco.cpp
 * @author Matthew Hagerty
 * @date April 23, 2021
 */

#include <stdio.h>   // NULL, stdout, fwrite, fflush

// IMGUI has pre-build wrappers for many graphics subsystems.
#define IMGUI_IMPL_OPENGL_LOADER_GL3W
#include "imgui.h"
#include "backends/imgui_impl_sdl.h"
#include "backends/imgui_impl_opengl3.h"

#include "SDL.h"
//#include "SDL_opengl.h" // Do not use with OpenGL3 / GL3W

// OpenGL 3.x complicates things compared to using OpenGL 2.x.
//
// About Desktop OpenGL function loaders:
//  Modern desktop OpenGL doesn't have a standard portable header file to load OpenGL function pointers.
//  Helper libraries are often used for this purpose! Here we are supporting a few common ones (gl3w, glew, glad).
//  You may use another loader/header of your choice (glext, glLoadGen, etc.), or chose to manually implement your own.
#if defined(IMGUI_IMPL_OPENGL_ES2)
#include <GLES2/gl2.h>
#elif defined(IMGUI_IMPL_OPENGL_LOADER_GL3W)
#include <GL/gl3w.h>            // Initialize with gl3wInit()
#elif defined(IMGUI_IMPL_OPENGL_LOADER_GLEW)
#include <GL/glew.h>            // Initialize with glewInit()
#elif defined(IMGUI_IMPL_OPENGL_LOADER_GLAD)
#include <glad/glad.h>          // Initialize with gladLoadGL()
#elif defined(IMGUI_IMPL_OPENGL_LOADER_GLAD2)
#include <glad/gl.h>            // Initialize with gladLoadGL(...) or gladLoaderLoadGL()
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

#include "xyz.h"
#include "program.h"
// Font headers can be built with the utility included with IMGUI.  See below.
#include "cousine_font.h"
#include "stb_sprintf.h"    // stbsp_snprintf


// ==========================================================================
//
// Local private globals and function declarations.
//


// Local private function forward declarations.
//

static s32 disco(progdata_s *pd);
static s32 render_thread(void *arg);
static s32 out_tty(const c8 *text, u32 len);
static s32 out_cons(const c8 *text, u32 len);


/**
 * Main.
 *
 * @param argc
 * @param argv
 *
 * @return 0 on success, otherwise 1.
 */
int
main(int argc, char *argv[])
{
   int rtn = 1;

   // Keep the compiler from complaining until these are used.
   (void)argc;
   (void)argv;

   // Allocate the program data.
   progdata_s *pd = (progdata_s *)xyz_calloc(1, sizeof(progdata_s));

   XYZ_BLOCK

   if ( pd == NULL ) {
      XYZ_BREAK
   }

   // Default initialization.
   pd->tty.buf = (c8 *)xyz_malloc(TTY_LINEBUF_DIM);
   pd->cons.buf = (c8 *)xyz_malloc(CONS_BUF_DIM);

   if ( pd->tty.buf == NULL || pd->cons.buf == NULL ) {
      XYZ_BREAK
   }

   pd->tty.bufdim = TTY_LINEBUF_DIM;
   pd->cons.bufdim = CONS_BUF_DIM;

   pd->tty.out = out_tty;
   pd->cons.out = out_cons;


   // Program initialization.
   if ( program_init(pd) != XYZ_OK ) {
      XYZ_BREAK
   }


   TTYF(pd, "%.*s\n", pd->prg_name.unit_len, (c8 *)pd->prg_name.buf.vp);


   // Run the program.
   disco(pd);

   if ( pd->callback.cleanup != NULL ) {
      pd->callback.cleanup(pd);
   }

   rtn = 0;
   XYZ_END


   // Memory cleanup.
   if ( pd != NULL )
   {
      if ( pd->tty.buf != NULL ) {
         xyz_free(pd->tty.buf);
      }

      if ( pd->cons.buf != NULL ) {
         xyz_free(pd->cons.buf);
      }

      xyz_free(pd);
   }

   return rtn;
}
// main()


/**
 * Main display and event handler.
 *
 * @param[in] pd  Pointer to the program data structure.
 *
 * @return XYZ_OK on success, otherwise XYZ_ERR;
 */
static s32
disco(progdata_s *pd)
{
   s32 rtn = XYZ_OK;

   // Decide GL+GLSL versions
#if defined(IMGUI_IMPL_OPENGL_ES2)
   // GL ES 2.0 + GLSL 100
   pd->disco.glsl_version = "#version 100";
   SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, 0);
   SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_ES);
   SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
   SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);
#elif defined(__APPLE__)
   // GL 3.2 Core + GLSL 150
   pd->disco.glsl_version = "#version 150";
   SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, SDL_GL_CONTEXT_FORWARD_COMPATIBLE_FLAG); // Always required on Mac
   SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
   SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
   SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 2);
#else
   // GL 3.0 + GLSL 130
   pd->disco.glsl_version = "#version 130";
   SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, 0);
   SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
   SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
   SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);
#endif


   SDL_WindowFlags window_flags = (SDL_WindowFlags)
         ( SDL_WINDOW_OPENGL
         | SDL_WINDOW_RESIZABLE
         | SDL_WINDOW_ALLOW_HIGHDPI
         );

   // Create the main window.  When using SDL2, this is the process/thread that
   // will receive events.
   pd->disco.window = SDL_CreateWindow((const char *)pd->prg_name.buf.vp,
         pd->winpos.x, pd->winpos.y, pd->winpos.w, pd->winpos.h,
         window_flags);

   if ( pd->disco.window == NULL ) {
      // TODO use error block.
      rtn = XYZ_ERR;
      return rtn;
   }


   // TODO Set context in pd to allow for hot-loading.  Need to look into
   // IMGUI requirements a little closer first.

   // Set up singleton ImGui context.
   IMGUI_CHECKVERSION();
   ImGui::CreateContext();


   // Set the running flag and start the rendering thread.
   pd->disco.running = XYZ_TRUE;
   pd->disco.render_thread_running = XYZ_FALSE;

   SDL_Thread *render_h;
   render_h = SDL_CreateThread(render_thread, "RenderThread", pd);
   if ( render_h == NULL )
   {
      //SK_LOG_CRIT(app, "%s", SDL_GetError());

      // TODO use error block.
      rtn = XYZ_ERR;
      return rtn;
   }

   // TODO Wait for rendering thread to finish initialization?
   //app->gui.render_sem = SDL_CreateSemaphore(0);


   // This message loop is a hybrid between a game-style loop which polls, and a
   // purely event-based loop which always blocks waiting for events.
   //
   // The loop will only block for a limited time waiting for events.  If no
   // events are received it will go ahead and run through the loop once to
   // allow non-event type activity to be performed.
   //
   // This format makes the loop adaptable and it becomes more responsive when
   // there are a lot of events, i.e. a lot of user interaction, since the
   // event-wait call will return immediately.  Where there are no events, the
   // loop scales back to spending most of its time waiting, which keeps its
   // CPU use low like most wait-on-event-only loops.

   // Notes from IMGUI about events:
   //
   // You can read the io.WantCaptureMouse, io.WantCaptureKeyboard flags to
   // tell if IMGUI wants to use your inputs.
   // - When io.WantCaptureMouse is true, do not dispatch mouse input data
   //   to your main program.
   // - When io.WantCaptureKeyboard is true, do not dispatch keyboard input
   //   data to your main program.
   // Generally you may always pass all inputs to IMGUI and hide them from
   // your program based on those two flags.

   while ( pd->disco.running == XYZ_TRUE )
   {
      SDL_Event event;
      SDL_ClearError();

      while ( pd->disco.running == XYZ_TRUE && SDL_WaitEvent(&event) != 0 )
      {
         pd->disco.event_counter++;
         ImGui_ImplSDL2_ProcessEvent(&event);

         // Check for quit or main window close events.
         if ( event.type == SDL_QUIT ) {
            pd->disco.running = XYZ_FALSE;
         }

         if ( event.type == SDL_WINDOWEVENT &&
              event.window.event == SDL_WINDOWEVENT_CLOSE &&
              event.window.windowID == SDL_GetWindowID(pd->disco.window) ) {
            pd->disco.running = XYZ_FALSE;
         }

         if ( pd->callback.event != NULL ) {
            pd->callback.event(pd);
         }
      }
   }


   // TODO revisit the semaphore use.
   //SDL_SemPost(app->gui.render_sem);
   s32 render_rtn;
//   SK_LOG_DBG(app, "Waiting for render thread to exit.");
   SDL_WaitThread(render_h, &render_rtn);
//   SK_LOG_DBG(app, "Render thread joined.");

   SDL_DestroyWindow(pd->disco.window);
   SDL_Quit();

   return rtn;
}
// disco()


/**
 * Rendering thread.
 *
 * Separating the rendering from the event loop allows the display to continue
 * to update even while the event loop is blocking, for example when the
 * window is being dragged or resized.
 *
 * @param[in] arg Pointer to the program data structure.
 *
 * @return 0 on successful termination, otherwise a non-zero error code.
 */
static s32
render_thread(void *arg)
{
   if ( arg == NULL ) {
      return 1;
   }

   // Set up a program data pointer.
   progdata_s *pd = (progdata_s *)(arg);

   //SK_LOG_DBG(app, "Render thread started.");


   // Create the OpenGL context and bind it to the main window.
   SDL_GLContext gl_context = SDL_GL_CreateContext(pd->disco.window);
   SDL_GL_MakeCurrent(pd->disco.window, gl_context);

   const s32 UPDATE_WITH_VSYNC = 1;
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
      //SK_LOG_CRIT(app, "Failed to initialize OpenGL loader.");
      // TODO errors and returns.
      return 2;
   }


   // Set ImGui I/O options.
   ImGuiIO &io = ImGui::GetIO();
   //io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard; // Enable Keyboard Controls
   //io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;  // Enable Gamepad Controls

   // Set up ImGui style.
   ImGui::StyleColorsDark();
   ImGuiStyle& style = ImGui::GetStyle();
   style.FrameBorderSize = 1.0;
   style.FrameRounding = 3.0;

   // Set up ImGui platform and renderer bindings.
   ImGui_ImplSDL2_InitForOpenGL(pd->disco.window, gl_context);
   ImGui_ImplOpenGL3_Init(pd->disco.glsl_version);


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
   // TODO customize or make available for the caller to change.
   ImVec4 clear_color = ImColor(0, 0, 0);


   // Update status flag that the rendering thread is active.
   pd->disco.render_thread_running = XYZ_TRUE;
   pd->disco.render_time_us = 0;

   //u64 perftime_freq = SDL_GetPerformanceFrequency();

   // Render until signaled to quit.
   while ( pd->disco.running == XYZ_TRUE )
   {
      // Do not render if minimized.
      // TODO Might still need to issue some call-backs?
      if ( (SDL_GetWindowFlags(pd->disco.window) & SDL_WINDOW_MINIMIZED) ==
            SDL_WINDOW_MINIMIZED) {
         const u32 NOTHING_TO_DO = 250;
         SDL_Delay(NOTHING_TO_DO);
         continue;
      }

      u64 start = SDL_GetPerformanceCounter();

      // Start an ImGui frame.
      ImGui_ImplOpenGL3_NewFrame();
      ImGui_ImplSDL2_NewFrame(pd->disco.window);
      ImGui::NewFrame();


      // TODO This call-back assumes drawing data will be to the IMGUI context.
      // Might need a different call-back to support drawing methods that do not
      // write geometry into the IMGUI draw lists.

      // Program drawing call-back.
      if ( pd->callback.draw != NULL ) {
         pd->callback.draw(pd);
      }


      // Rendering.
      ImGui::Render();
      glViewport(0, 0, (s32)io.DisplaySize.x, (s32)io.DisplaySize.y);
      glClearColor(clear_color.x, clear_color.y, clear_color.z, clear_color.w);
      glClear(GL_COLOR_BUFFER_BIT);

      // TODO Any drawing here will be behind anything IMGUI draws.

      // Render the ImGui data.
      ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

      // TODO Any drawing here will be on top of anything IMGUI draws.

      // Track the time to render everything.
      pd->disco.render_time_us = SDL_GetPerformanceCounter() - start;

      // Swapping buffers is timed with VSYNC, so this will keep the whole
      // render-loop at a constant frame rate.
      SDL_GL_SwapWindow(pd->disco.window);
      pd->disco.render_counter++;


      // TODO Wait for a render update event. (does this work?)
      // TODO SDL_SemWaitTimeout() to create a polling render loop.
      //SDL_SemWait(app->gui.render_sem);

      // A delay helps keep CPU use a little lower (~ 10% vs 20%), and should
      // not affect the frame rate unless the frame calculation time is getting
      // close to the actual frame rate.  Can be removed if CPU use is of no
      // no concern or there is an impact to performance.
      // TODO work on a better solution than sleep, like the above signal or
      // at least an interruptible timeout.  Adapt based on render time maybe?
      //SDL_Delay(10);
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
 * Write to the default TTY, usually stdout.
 *
 * @param[in] text   The text to write.
 * @param[in] len    The length of text.
 *
 * @return The value of len on success, otherwise less than len.
 */
static s32
out_tty(const c8 *text, u32 len)
{
   s32 rtn = (s32)len;

   if ( rtn > 0 ) {
      rtn = fwrite(text, len, 1, stdout);
      fflush(stdout);
   }

   return rtn;
}
// out_tty()


static s32
out_cons(const c8 *text, u32 len)
{
   s32 rtn = fwrite(text, len, 1, stdout);
   fflush(stdout);
   return rtn;
}
// out_cons()
