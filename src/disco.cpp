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
#include "SDL.h"
//#include "SDL_opengl.h" // Do not use with OpenGL3 / GL3W


// IMGUI has pre-build wrappers for many graphics subsystems.
#define IMGUI_IMPL_OPENGL_LOADER_GL3W
#include "imgui.h"
#include "backends/imgui_impl_opengl3.h"
#include "backends/imgui_impl_sdl.h"


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

   // Allocate the program data and zero all fields.
   progdata_s *pd = (progdata_s *)xyz_calloc(1, sizeof(progdata_s));

   XYZ_BLOCK

   if ( pd == NULL ) {
      XYZ_BREAK
   }

   // Default initialization.
   pd->tty.buf = (c8 *)xyz_malloc(TTY_LINEBUF_DIM);
   pd->cons.buf = (c8 *)xyz_malloc(CONS_BUF_DIM);
   pd->cons.linelist = (consline_s *)xyz_calloc(CONS_LINELIST_DIM, sizeof(consline_s));
   pd->cons.mutex = SDL_CreateMutex();
   pd->cons.lockfailures = 0;

   if (
      pd->tty.buf == NULL ||
      pd->cons.buf == NULL ||
      pd->cons.linelist == NULL ||
      pd->cons.mutex == NULL ) {
      XYZ_BREAK
   }

   pd->tty.bufdim = TTY_LINEBUF_DIM;
   pd->cons.bufdim = CONS_BUF_DIM;
   xyz_rbam_init(&(pd->cons.rbam), CONS_LINELIST_DIM);

   pd->tty.out = out_tty;
   pd->cons.out = out_cons;

   // Set up SDL.
   if ( SDL_Init(SDL_INIT_EVERYTHING) != 0 )
   {
      SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Program using SDL2",
            "Something really basic failed.  Cannot continue, sorry.", NULL);
      const char *sdlerr = SDL_GetError();
      TTYF(pd, "%s:%d SDL_Init(SDL_INIT_EVERYTHING) failed with: [%s]\n",
            XYZ_CFL, sdlerr);
      XYZ_BREAK
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

   SDL_Thread *render_h = NULL;
   SDL_Thread *program_h = NULL;

   XYZ_BLOCK

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
      SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, (c8 *)pd->prg_name.buf.vp,
            "Failed to create the main program window.  Cannot continue, sorry.", NULL);
      TTYF(pd, "%s:%d SDL_CreateWindow() failed.", XYZ_CFL);
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
   pd->disco.running = XYZ_TRUE;
   pd->disco.render_thread_running = XYZ_FALSE;

   render_h = SDL_CreateThread(render_thread, "RenderThread", pd);
   if ( render_h == NULL ) {
      SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, (c8 *)pd->prg_name.buf.vp,
            "The pixels spilled out of the program. :(  Cannot continue.", NULL);
      TTYF(pd, "%s:%d SDL_CreateThread(RenderThread) failed.", XYZ_CFL);
      XYZ_BREAK
   }

   // Wait for rendering thread to finish initialization.
   u32 sanity = 300;
   while ( pd->disco.render_thread_running == XYZ_FALSE && sanity > 0 ) {
      SDL_Delay(10);
      sanity--;
   }

   if ( sanity == 0 ) {
      // The rendering thread never signaled that it started.
      SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, (c8 *)pd->prg_name.buf.vp,
            "The box of pixels never said hello.  Sorry, cannot continue.", NULL);
      TTYF(pd, "%s:%d The render thread never signaled it was ready.", XYZ_CFL);
      XYZ_BREAK
   }


   // Set the program running flag and start the program thread if specified.
   // When this flag is set false by the program, the event loop will stop
   // the render thread and shutdown.
   pd->program_running = XYZ_TRUE;

   if ( pd->main_thread != NULL )
   {
      program_h = SDL_CreateThread(pd->main_thread, "ProgramThread", pd);
      if ( program_h == NULL )
      {
         SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, (c8 *)pd->prg_name.buf.vp,
               "Could not start the program. :(  Cannot continue.", NULL);
         TTYF(pd, "%s:%d SDL_CreateThread(ProgramThread) failed.", XYZ_CFL);
         pd->program_running = XYZ_FALSE;
         pd->disco.running = XYZ_FALSE;
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

   while ( pd->disco.running == XYZ_TRUE && SDL_WaitEvent(event) != 0 )
   {
      pd->disco.event_counter++;
      ImGui_ImplSDL2_ProcessEvent(event);

      // Give the program event call-back a chance to handle events first.
      s32 handled = XYZ_FALSE;
      if ( pd->callback.event != NULL ) {
         handled = pd->callback.event(pd);
      }

      if ( handled == XYZ_FALSE )
      {
         // Check for quit or main window close events.
         if ( event->type == SDL_QUIT ) {
            pd->disco.running = XYZ_FALSE;
         }

         if ( event->type == SDL_WINDOWEVENT &&
              event->window.event == SDL_WINDOWEVENT_CLOSE &&
              event->window.windowID == SDL_GetWindowID(pd->disco.window) ) {
            pd->disco.running = XYZ_FALSE;
         }
      }

      // Check for program exit.
      if ( pd->program_running == XYZ_FALSE ) {
         pd->disco.running = XYZ_FALSE;
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
   if ( pd->disco.gl_context == NULL ) {
      SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, (c8 *)pd->prg_name.buf.vp,
            "Could not talk the to graphics card.  Cannot continue, sorry.", NULL);
      const char *sdlerr = SDL_GetError();
      TTYF(pd, "%s:%d SDL_GL_CreateContext() failed with: [%s]\n",
            XYZ_CFL, sdlerr);
      XYZ_BREAK
   }

   SDL_GL_MakeCurrent(pd->disco.window, pd->disco.gl_context);

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
      SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, (c8 *)pd->prg_name.buf.vp,
            "Could not load important graphics.  Cannot continue, sorry.", NULL);
      TTYF(pd, "%s:%d Failed to load OpenGL functions.\n", XYZ_CFL);
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
      if ( xyz_rbam_is_full(rbam) == XYZ_TRUE ) {
         xyz_rbam_read(rbam);
      }

      // Calculate the position of the end of the line in the buffer, which is
      // also the "new position" for the line after this one.
      u32 newpos = list[*wr].pos + linelen;

      // Clear any top lines that would be overwritten by the new line.
      while ( list[*rd].pos >= list[*wr].pos &&
              list[*rd].pos < newpos &&
              xyz_rbam_is_empty(rbam) == XYZ_FALSE )
      { xyz_rbam_read(rbam); }

      // Adjust the new starting location if out of bounds.
      if ( newpos > pd->cons.bufdim )
      {
         // The new line will not fit in the remainder of the buffer,
         // so restart at position 0.
         list[*wr].pos = 0;
         newpos = linelen;

         // Clear any lines that will be overwritten by the new line.
         while ( list[*rd].pos < newpos &&
                 xyz_rbam_is_empty(rbam) == XYZ_FALSE )
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
      if ( xyz_rbam_is_full(rbam) == XYZ_TRUE ) {
         xyz_rbam_read(rbam);
      }

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
