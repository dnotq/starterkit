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
 * updates to take place while mouse-blocking operations are taking place, i.e.
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


#include <stdio.h>         // NULL, stdout, fwrite, fflush
#include <stdint.h>        // uintXX_t, intXX_t, UINTXX_MAX, INTXX_MAX, etc.
#include <stdbool.h>       // true, false

// IMGUI has pre-built wrappers for many graphics subsystems.
#include "imgui.h"
#include "backends/imgui_impl_sdl.h"
#include "backends/imgui_impl_opengl3.h"

#include "SDL.h"
#include "SDL_opengl.h"

#include "xyz.h"
#include "program.h"
// Font headers can be built with the utility included with IMGUI.  See below.
#include "cousine_font.h"
#include "stb_sprintf.h"    // stbsp_snprintf


// ==========================================================================
//
// Local private globals and function declarations.
//

/// Generic size of a window title bar.
/// TODO Need a way to obtain this value prior to window creation.
#define TITLEBAR_HEIGHT 48

/// Buffer size for critical error messages that need to be displayed prior
/// to window creation or during startup.
#define ERROR_BUF_DIM 1024


// Local private function forward declarations.
//

static s32 disco(progdata_s *pd);
static s32 render_thread(void *arg);
static u32 out_tty(void *arg, const c8 *text, u32 len);
static u32 out_cons(void *arg, const c8 *text, u32 len);


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

   // TODO Set up memory allocator so malloc/calloc is only called once for
   //      the entire run of the program.

   // Allocate the program data and zero all fields.
   progdata_s *pd = (progdata_s *)xyz_calloc(1, sizeof(progdata_s));

   XYZ_BLOCK

   // The SDL simple message box is available even before the library has been
   // initialized, so it can be used for error messages.  If the SDL2 DLL
   // could not be loaded, the code would never have gotten this far.

   if ( pd == NULL ) {
      SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, APP_NAME,
            "Cannot continue: the main program data structure could not "
            "be allocated.  Sorry.", NULL);
      XYZ_BREAK
   }

   pd->err.buf = (c8 *)xyz_malloc(ERROR_BUF_DIM);
   if ( pd->err.buf == NULL ) {
      SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, APP_NAME,
            "Cannot continue: the error message buffer could not "
            "be allocated.  Ironic.", NULL);
      XYZ_BREAK
   }

   // Temporary assignment until the progam_init() function can be called.
   pd->prg_name = APP_NAME;

   // Default initialization.
   pd->tty.buf  = (c8 *)xyz_malloc(TTY_LINEBUF_DIM);
   pd->cons.buf = (c8 *)xyz_malloc(CONS_BUF_DIM);
   pd->cons.linelist = (consline_s *)xyz_calloc(CONS_LINELIST_DIM, sizeof(consline_s));
   pd->cons.mutex = SDL_CreateMutex();
   pd->cons.lockfailures = 0;

   if (
      pd->tty.buf == NULL ||
      pd->cons.buf == NULL ||
      pd->cons.linelist == NULL ||
      pd->cons.mutex == NULL ) {
      SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, pd->prg_name,
            "Cannot continue: the TTY and Console message buffer could not "
            "be allocated.", NULL);
      XYZ_BREAK
   }

   pd->tty.bufdim = TTY_LINEBUF_DIM;
   pd->cons.bufdim = CONS_BUF_DIM;
   xyz_rbam_init(&(pd->cons.rbam), CONS_LINELIST_DIM);

   pd->tty.out = out_tty;
   pd->cons.out = out_cons;

   // TODO Find a place to write files and set up a log.  The TTY is most
   //      likely not available in release builds.

   // Set up SDL.  Initialize individual SDL subsystems for better error
   // reporting and possible recovery.

   if ( SDL_InitSubSystem(SDL_INIT_VIDEO) != 0 )
   {
      // The video (implies events) subsystem is critical.
      const char *sdlerr = SDL_GetError();
      s32 errlen = stbsp_snprintf(pd->err.buf, pd->err.bufdim,
            "Cannot continue: %s:%d SDL_InitSubSystem(SDL_INIT_VIDEO) "
            "failed with: [%s]\n", XYZ_CFL, sdlerr);
      SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, pd->prg_name,
            pd->err.buf, NULL);
      pd->tty.out(pd, pd->err.buf, errlen);
      XYZ_BREAK
   }

   // Be optimistic about subsystem initialization.
   pd->sdl.have_audio = true;
   pd->sdl.have_timer = true;

   // Audio and timer are not critical, for now.
   if ( SDL_InitSubSystem(SDL_INIT_AUDIO) != 0 )
   {
      pd->sdl.have_audio = false;

      const char *sdlerr = SDL_GetError();
      s32 errlen = stbsp_snprintf(pd->err.buf, pd->err.bufdim,
            "Warning: %s:%d SDL_InitSubSystem(SDL_INIT_AUDIO) "
            "failed with: [%s]\n", XYZ_CFL, sdlerr);
      SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, pd->prg_name,
            pd->err.buf, NULL);
      pd->tty.out(pd, pd->err.buf, errlen);
   }

   if ( SDL_InitSubSystem(SDL_INIT_TIMER) != 0 )
   {
      pd->sdl.have_timer = false;

      const char *sdlerr = SDL_GetError();
      s32 errlen = stbsp_snprintf(pd->err.buf, pd->err.bufdim,
            "Warning: %s:%d SDL_InitSubSystem(SDL_INIT_TIMER) "
            "failed with: [%s]\n", XYZ_CFL, sdlerr);
      SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, pd->prg_name,
            pd->err.buf, NULL);
      pd->tty.out(pd, pd->err.buf, errlen);
   }


   // Program initialization.
   if ( program_init(pd) != XYZ_OK ) {
      XYZ_BREAK
   }


   // Run the program.
   rtn = disco(pd);

   if ( pd->callback.cleanup != NULL ) {
      pd->callback.cleanup(pd);
   }

   XYZ_END


   // Memory cleanup.
   if ( pd != NULL )
   {
      if ( pd->cons.lockfailures != 0 ) {
         TTYF(pd, "Warning: Console mutex lock failure count: %u\n",
               pd->cons.lockfailures);
      }

      if ( pd->tty.buf != NULL ) {
         xyz_free(pd->tty.buf);
      }

      if ( pd->cons.buf != NULL ) {
         xyz_free(pd->cons.buf);
      }

      if ( pd->cons.linelist != NULL ) {
         xyz_free(pd->cons.linelist);
      }

      if ( pd->cons.mutex != NULL ) {
         SDL_DestroyMutex(pd->cons.mutex);
      }

      if ( pd->err.buf != NULL ) {
         xyz_free(pd->err.buf);
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
   s32 rtn = XYZ_ERR;

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

   SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
   SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
   SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 8);


   SDL_Thread *render_h = NULL;
   SDL_Thread *program_h = NULL;

   XYZ_BLOCK

   // TODO Support for multiple monitors.

   // Make sure the window size is inside the usable display area.
   SDL_Rect usable_bounds;
   s32 display_index = 0;
   if ( SDL_GetDisplayUsableBounds(display_index, &usable_bounds) == 0 )
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

      if ( pd->winpos.h > usable_bounds.h )
      {
         // Clamp to the window usable height.
         pd->winpos.h = usable_bounds.h;
         pd->winpos.y = usable_bounds.y;
      }

      else
      {
         // Convert a centered location to a screen location.
         if ( pd->winpos.y == SDL_WINDOWPOS_CENTERED ) {
            pd->winpos.y = (usable_bounds.h / 2) - (pd->winpos.h / 2);
         }

         // If the bottom-edge of the window is not visible, move up.
         s32 ymax = (pd->winpos.y + pd->winpos.h);
         if ( ymax > usable_bounds.h ) {
            pd->winpos.y = (usable_bounds.y + usable_bounds.h) - pd->winpos.h;
         }

         // If the top-edge of the window is not visible, move down.
         if ( pd->winpos.y < usable_bounds.y ) {
            pd->winpos.y = usable_bounds.y;
         }
      }


      if ( pd->winpos.w > usable_bounds.w ) {
         // Clamp to the window usable width.
         pd->winpos.w = usable_bounds.w;
         pd->winpos.x = usable_bounds.x;
      }

      else
      {
         // Convert a centered location to a screen location.
         if ( pd->winpos.x == SDL_WINDOWPOS_CENTERED ) {
            pd->winpos.x = (usable_bounds.w / 2) - (pd->winpos.w / 2);
         }

         // If the right-edge of the window is not visible, move left.
         s32 xmax = (pd->winpos.x + pd->winpos.w);
         if ( xmax > usable_bounds.w ) {
            pd->winpos.x = (usable_bounds.x + usable_bounds.w) - pd->winpos.w;
         }

         // If the left-edge of the window is not visible, move right.
         if ( pd->winpos.x < usable_bounds.x ) {
            pd->winpos.x = usable_bounds.x;
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
   pd->disco.window = SDL_CreateWindow((const c8 *)pd->prg_name,
         pd->winpos.x, pd->winpos.y, pd->winpos.w, pd->winpos.h,
         window_flags);

   if ( pd->disco.window == NULL )
   {
      const char *sdlerr = SDL_GetError();
      s32 errlen = stbsp_snprintf(pd->err.buf, pd->err.bufdim,
            "Cannot continue: %s:%d SDL_CreateWindow(OPENGL) failed with: [%s]\n",
            XYZ_CFL, sdlerr);
      SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, pd->prg_name,
            pd->err.buf, NULL);
      pd->tty.out(pd, pd->err.buf, errlen);
      XYZ_BREAK
   }


   // TODO Set context in pd to allow for hot-loading.  Need to look into
   // IMGUI requirements a little closer first.

   // Set up singleton ImGui context.
   IMGUI_CHECKVERSION();
   ImGui::CreateContext();

   // Set the IMGUI ini file.
   ImGuiIO &io = ImGui::GetIO();
   io.IniFilename = pd->imgui_ini_filename;


   // Set the running flag and start the rendering thread.
   pd->disco.running = true;
   pd->disco.render_thread_running = false;

   render_h = SDL_CreateThread(render_thread, "RenderThread", pd);
   if ( render_h == NULL )
   {
      const char *sdlerr = SDL_GetError();
      s32 errlen = stbsp_snprintf(pd->err.buf, pd->err.bufdim,
            "Cannot continue: %s:%d SDL_CreateThread(RenderThread) "
            "failed with: [%s]\n", XYZ_CFL, sdlerr);
      SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, pd->prg_name,
            pd->err.buf, NULL);
      pd->tty.out(pd, pd->err.buf, errlen);
      XYZ_BREAK
   }

   // Wait for rendering thread to finish initialization.
   u32 sanity = 300;
   while ( pd->disco.render_thread_running == false && sanity > 0 ) {
      SDL_Delay(10);
      sanity--;
   }

   if ( sanity == 0 ) {
      // The rendering thread never signaled that it started.
      SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, pd->prg_name,
            "Cannot continue: the render thread never responded.", NULL);
      TTYF(pd, "%s:%d The render thread never signaled it was ready.", XYZ_CFL);
      XYZ_BREAK
   }


   // Set the program running flag and start the program thread if specified.
   // When this flag is set false by the program, the event loop will stop
   // the render thread and shutdown.
   pd->program_running = true;

   if ( pd->main_thread != NULL )
   {
      program_h = SDL_CreateThread(pd->main_thread, "ProgramThread", pd);
      if ( program_h == NULL )
      {
         const char *sdlerr = SDL_GetError();
         s32 errlen = stbsp_snprintf(pd->err.buf, pd->err.bufdim,
               "Cannot continue: %s:%d SDL_CreateThread(ProgramThread) "
               "failed with: [%s]\n", XYZ_CFL, sdlerr);
         SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, pd->prg_name,
               pd->err.buf, NULL);
         pd->tty.out(pd, pd->err.buf, errlen);

         pd->program_running = false;
         pd->disco.running = false;
         XYZ_BREAK
      }
   }


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

   SDL_Event *event = &(pd->disco.event); // convenience.

   while ( pd->disco.running == true && SDL_WaitEvent(event) != 0 )
   {
      pd->disco.event_counter++;
      ImGui_ImplSDL2_ProcessEvent(event);

      // Give the program event call-back a chance to handle events first.
      s32 handled = false;
      if ( pd->callback.event != NULL ) {
         handled = pd->callback.event(pd);
      }

      if ( handled == false )
      {
         // Check for quit or main window close events.
         if ( event->type == SDL_QUIT ) {
            pd->disco.running = false;
         }

         if ( event->type == SDL_WINDOWEVENT &&
              event->window.event == SDL_WINDOWEVENT_CLOSE &&
              event->window.windowID == SDL_GetWindowID(pd->disco.window) ) {
            pd->disco.running = false;
         }
      }

      // Check for program exit.
      if ( pd->program_running == false ) {
         pd->disco.running = false;
      }
   }

   rtn = XYZ_OK;
   XYZ_END


   if ( render_h != NULL ) {
      s32 render_rtn;
      SDL_WaitThread(render_h, &render_rtn);
   }

   if ( program_h != NULL )
   {
      s32 program_rtn;
      SDL_WaitThread(program_h, &program_rtn);
      if ( rtn == XYZ_OK ) {
         // If there was no internal error, pass the program return value back.
         rtn = program_rtn;
      }
   }

   if ( pd->disco.window != NULL ) {
      SDL_DestroyWindow(pd->disco.window);
   }

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
 * @return XYZ_OK on success, otherwise XYZ_ERR;
 */
static s32
render_thread(void *arg)
{
   s32 rtn = XYZ_ERR;

   if ( arg == NULL ) {
      return rtn;
   }

   // Set up a program data pointer.
   progdata_s *pd = (progdata_s *)(arg);


   XYZ_BLOCK

   // Create the OpenGL context and bind it to the main window.
   pd->disco.gl_context = SDL_GL_CreateContext(pd->disco.window);
   if ( pd->disco.gl_context == NULL )
   {
      const char *sdlerr = SDL_GetError();
      s32 errlen = stbsp_snprintf(pd->err.buf, pd->err.bufdim,
            "Cannot continue: %s:%d SDL_GL_CreateContext() "
            "failed with: [%s]\n", XYZ_CFL, sdlerr);
      SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, pd->prg_name,
            pd->err.buf, NULL);
      pd->tty.out(pd, pd->err.buf, errlen);
      XYZ_BREAK
   }

   SDL_GL_MakeCurrent(pd->disco.window, pd->disco.gl_context);

   const s32 UPDATE_WITH_VSYNC = 1;
   SDL_GL_SetSwapInterval(UPDATE_WITH_VSYNC);


   // TODO Implement and initialize a local (not imgui) OpenGL loader.
   bool gl_init_err = false; //gl3wInit() != 0;

   if ( gl_init_err == true )
   {
      SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, pd->prg_name,
            "Cannot continue: gl3wInit() failed to load OpenGL functions.", NULL);
      TTYF(pd, "%s:%d gl3wInit() failed to load OpenGL functions.\n", XYZ_CFL);
      XYZ_BREAK
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
   ImGui_ImplSDL2_InitForOpenGL(pd->disco.window, pd->disco.gl_context);
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
   pd->disco.render_thread_running = true;
   pd->disco.render_time_us = 0;

   //u64 perftime_freq = SDL_GetPerformanceFrequency();

   // Render until signaled to quit.
   while ( pd->disco.running == true )
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
   }

   rtn = XYZ_OK;

   XYZ_END

   // Cleanup.
   ImGui_ImplOpenGL3_Shutdown();
   ImGui_ImplSDL2_Shutdown();
   ImGui::DestroyContext();

   if ( pd->disco.gl_context != NULL ) {
      SDL_GL_DeleteContext(pd->disco.gl_context);
   }

   return rtn;
}
// render_thread()


/**
 * Write to the default TTY, usually stdout.
 *
 * @param[in] arg    Pointer to the program data structure.
 * @param[in] text   The text to write.
 * @param[in] len    The length of text.
 *
 * @return The value of len on success, otherwise less than len.
 */
static u32
out_tty(void *arg, const c8 *text, u32 len)
{
   progdata_s *pd = (progdata_s *)arg;
   (void)pd; // not currently used.

   size_t rtn = len;

   if ( rtn > 0 ) {
      rtn = fwrite(text, len, 1, stdout);
      fflush(stdout);
   }

   // The return value cannot be larger than len, so casting is fine.
   return (u32)rtn;
}
// out_tty()


/**
 * Write to the console buffer.
 *
 * TODO Handle UTF8.
 *
 * This is a line-oriented buffer, so input text will be split on newline
 * as well as lines longer than a maximum number of characters.
 *
 * @param[in] arg    Pointer to the program data structure.
 * @param[in] text   The text to write.
 * @param[in] len    The length of text.
 *
 * @return The value of len on success, otherwise less than len.
 */
static u32
out_cons(void *arg, const c8 *text, u32 len)
{
   progdata_s *pd = (progdata_s *)arg;

   s32 locked = -1;

   XYZ_BLOCK

   if ( len == 0 || text == NULL || *text == XYZ_NTERM ) {
      len = 0;
      XYZ_BREAK
   }

   // This is a console log and should be quick, so refuse lines longer than 4x
   // the maximum single line length.  If support for huge blobs of text is
   // desired, this is not the implementation to use.
   if ( len > (CONS_MAX_LINE * 4) ) {
      len = CONS_MAX_LINE * 4;
   }

   // The design is a ring-buffer list of line positions and lengths, and a
   // byte buffer holding the line data.
   //
   // Both the line list and buffer are fixed arrays, so which ever buffer
   // fills up first determines how many lines there are for display.


   locked = SDL_TryLockMutex(pd->cons.mutex);
   if ( locked != 0 )
   {
      pd->cons.lockfailures++;

      // Yield and wait the minimum time, and try the lock once more.
      SDL_Delay(1);
      locked = SDL_TryLockMutex(pd->cons.mutex);

      if ( locked != 0 ) {
         pd->cons.lockfailures++;
         XYZ_BREAK
      }
   }


   u32 idx = 0;
   u32 linelen = 0;
   u32 start_idx = 0;
   while ( idx < len )
   {
      // Set up for the next line.
      linelen = 0;
      start_idx = idx;

      while ( linelen < CONS_MAX_LINE )
      {
         if ( text[idx] == XYZ_NTERM ) {
            // A terminator will end the input even if the length was specified
            // as being longer.  This is not a binary-safe buffer.
            len = idx;
            break;
         }

         if ( text[idx] == '\n' || text[idx] == '\r' )
         {
            // Include the EOL byte.
            idx++;
            linelen++;

            if ( idx < len && text[idx - 1] == '\r' && text[idx] == '\n' ) {
               // Check for and include a CRLF pair.
               idx++;
               linelen++;
            }

            break;
         }

         idx++;
         linelen++;
      }

      // A terminator will be added to the line to prevent buffer overflows
      // and make it compatible with other functions that expect / need
      // terminated buffers.
      linelen += 1;

      // Sanity.
      if ( linelen > pd->cons.bufdim ) {
         continue;
      }

      // Convenience.
      consline_s *list = pd->cons.linelist;
      u32 *rd = &(pd->cons.rbam.rd);
      u32 *wr = &(pd->cons.rbam.wr);
      xyz_rbam *rbam = &(pd->cons.rbam);

      // If the line list is full, make room for the new line.
      if ( XYZ_RBAM_FULL(rbam) ) { xyz_rbam_read(rbam); }

      // Calculate the position of the end of the line in the buffer, which is
      // also the "new position" for the line after this one.
      u32 newpos = list[*wr].pos + linelen;

      // Clear any top lines that would be overwritten by the new line.
      while ( list[*rd].pos >= list[*wr].pos &&
              list[*rd].pos < newpos &&
              XYZ_RBAM_MORE(rbam) )
      { xyz_rbam_read(rbam); }

      // Adjust the new starting location if out of bounds.
      if ( newpos > pd->cons.bufdim )
      {
         // The new line will not fit in the remainder of the buffer,
         // so restart at position 0.
         list[*wr].pos = 0;
         newpos = linelen;

         // Clear any lines that will be overwritten by the new line.
         while ( list[*rd].pos < newpos && XYZ_RBAM_MORE(rbam) )
         { xyz_rbam_read(rbam); }
      }

      // The line has been checked to be shorter than the size of the buffer.
      list[*wr].len = linelen;


      // The line data is (linelen - 1), since it was increased to account for
      // the terminator that will be written in the buffer.
      memcpy(pd->cons.buf + list[*wr].pos, text + start_idx, linelen - 1);

      // Set the last reserved byte to the terminator.
      pd->cons.buf[newpos - 1] = XYZ_NTERM;

      // Indicate that a new line has been written.
      xyz_rbam_write(rbam);

      // If the line list is now full, read at least one line so the new
      // position can be stored.
      if ( XYZ_RBAM_FULL(rbam) ) { xyz_rbam_read(rbam); }

      // Prepare for the next line.
      list[*wr].pos = newpos;
      list[*wr].len = 0;
   }

   XYZ_END

   if ( locked == 0 ) {
      SDL_UnlockMutex(pd->cons.mutex);
   }

   return len;
}
// out_cons()


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
