# Starter Kit for Cross-Platform Program Development

The purpose of the Starter Kit is to provide a minimal amount of code necessary to create a program, with graphic and UI capability, on *nix (BSD or Linux), MacOS, and Windows systems.

Goals:
- Simple
- Minimal dependencies, included as source code where possible or practical
- Quick and easy to create a graphics-oriented program
- Small executable with minimal additional runtime libraries

The Starter Kit is NOT a framework, a library, or engine of any kind.

The default configuration uses SDL2 and OpenGL3.x for the graphics abstraction layer, and ImGUI for widgets and controls.  C is the primary language, however since ImGUI is written using some C++ features, a C++ compiler is also required.

The Starter Kit is deliberately small and does only a minimum amount of setup necessary to get a graphic-capable program up and running.  A standard event-loop and rendering thread are provided which makes starting a new project very similar to a simple "Hello, World" C program.  A minimal graphical console for text output (currently broken) is also included, but the rest is up to you.

The base code has some minimal example code for drawing using ImGUI primitives (lines, boxes, etc.), as well as setting up a pair of shaders for direct OpenGL drawing.


## Motivation

Writing programs for a personal-computer (PC) has become overly complicated, much more so than it needs to be.  While there are many frameworks, engines, and "environments" (i.e. Electron, etc.) available, using one (or more) is yet-another-layer to be learned, become frustrated with, and locked into.  The Starter Kit attempts to provide the minimal amount of abstraction and dependencies necessary to start writing platform independent graphical programs.

The Starter Kit is just that, a starting point, you are expected to change it and make it your own.

![Starterkit Screenshot](starterkit_shot.png "Starterkit Screenshot")


## Requirements

- A C and C++ compiler
- Python (for gl3w only)
- An SDL2 supported OS with OpenGL
- CMake (or your own build system)


## Dependency Setup and Build

Dependencies need to be setup once.  SDL2 build from source, or download developer prebuilt library for your system.  If OpenGL direct drawing is not needed then the `gl3w` and `cglm` dependencies can be skipped, however the example code requires both.

Example for a *nix system.

```
$ git clone git@github.com:dnotq/starterkit.git
$ cd starterkit/src/deps

$ export SDL2_INSTALL_DIR=`pwd`/SDL2_linux_2.30.1
$ mkdir $SDL2_INSTALL_DIR
$ git clone https://github.com/libsdl-org/SDL.git --branch release-2.30.1 --single-branch SDL2-src
$ cd SDL2-src
$ ./configure --prefix=$SDL2_INSTALL_DIR
$ make -j 12
$ make install

$ git clone https://github.com/ocornut/imgui.git
$ git clone https://github.com/nothings/stb.git

Dependencies for direct OpenGL drawing:

$ git clone https://github.com/recp/cglm.git
$ git clone https://github.com/skaslev/gl3w.git
$ cd gl3w
$ python gl3w_gen.py
$ cd ..

Build and run:
$ cd ../..
$ chmod 755 make_unix.sh
$ ./make_unix.sh
$ cd build_unix
$ bin/starterkit
```

Edit the top CMakeLists.txt file to specify the exact directory for the libraries (SDL2 mostly) if necessary.

Run the make script (make_win.bat, make_mingw.sh, make_unix.sh) for your OS.  You may have to set execute permissions on the scripts.


### Dependency Details

The following libraries are used to provide abstraction and cross platform support.  The libraries are all open-source or public domain, and written in C or C++.

All libraries support *nix/MAC/Win.


### CMAKE

Currently CMake is used because it helps with the cross-platform tools detection and Makefile setup.  The setup is very minimal and as little "magic" used as possible.

It should be reasonably simple to move to something else entirely, write your own build scripts or Makefiles, etc..  There is nothing special required to compile any of the source code used by the Starter Kit, other than normal compiler parameters, library and header locations, etc..


### SDL2 2.x (soon SDL3)

Provides a nice cross platform windowing, graphics, sound, events, threads, atomics, and file I/O.  The thread, atomics, and file I/O abstractions are central to writing cross platform code without additional libraries or having to use language-specific types.  SDL2 strikes a good balance between "just enough" and "going too far", which helps make cross-platform development much easier.

Website: [https://libsdl.org/](https://libsdl.org/)

Use prebuilt DLLs for Windows or MacOS.  For *nix systems install with a package manager (be sure to also install the developer support package) or from source.

Building SDL2 from source is straight forward and allows the project to use a specific version of the library, separate from the system installed version, if necessary (or desired).  See the Dependency Setup and Build section above for building SDL2 from source on *nix.


### ImGUI

Cross platform Immediate-Mode widgets and controls.  Allows for very responsive programs that are easy to write.  Limited drawing can also be done using ImGUI's internal functions, which provides a quick and simple way to access accelerated drawing without any additional OpenGL setup.

- Website: [https://github.com/ocornut/imgui](https://github.com/ocornut/imgui)

Included as source and built as part of the program.  The project has example code and support for almost every OS and graphics abstraction layer, so feel free to use whatever you want.


### STB

Single-file public domain (or MIT licensed) libraries for C/C++.  Currently `stb_sprintf` is the main requirement from this collection and provides a faster and cleaner replacement for `printf` formatting capability.  Cross-platform tokens are provided for consistent 64-bit values, as well as better floating point conversion, and other tokens for things like direct binary formatting, etc..

STB also provides some very popular image and font loading functions, among others.

- Website: [https://github.com/nothings/stb](https://github.com/nothings/stb)

Included as source and built as part of the program.


### GL3W

OpenGL header generator for full access to OS OpenGL libraries.  Optional, not needed if only ImGUI or SDL2 are used for drawing.

- Website: [https://github.com/skaslev/gl3w](https://github.com/skaslev/gl3w)

Run the python script `gl3w_gen.py` to generate the OpenGL headers and C file.  The generated headers and C file are included as source and built as part of the program.


### CGLM

An optimized pure C version of the GLM (OpenGL Mathematics library).  Optional, not needed if the provided math types and functions are not needed.

- Website: [https://github.com/recp/cglm](https://github.com/recp/cglm)

Included as source and built as part of the program.

----

## Compiling

Once the dependencies are set up, edit the CMakeLists.txt file to specify the exact directory for the libraries (SDL2 mostly).  The SDL2 developer library path must be specified so it can be found by the compiler.  Most of the "FindSDL2" CMake modules out there are just overly complicated, IMO, and failed to work correctly.  It is much easier to just tell the compiler where the lib is located.

Building the Starter Kit can be done with one of the provided scripts (you may need to set execute permissions on the scripts):

- `make_unix.sh` for *nix or MAC (MAC compiling / testing is a TODO).
- `make_mingw.sh` for MinGW.
- `make_win.bat` for Windows.

The scripts are very basic and simply use the system agnostic CMake commands that do the following (build_unix or build_win, depending on which is run):

```
$ mkdir build_unix
$ cd build_unix
$ cmake ..
$ cmake --build .
```

The final executable can be found in:

- `build_unix/bin/starterkit` on Unix / Mac
- `build_win/bin/Debug/starterkit.exe` on Windows.

Visual Studio creates the `Debug` and `Release` directories inside the specified build directory.

Generally the `SDL2.dll` or `libSDL2.so` library needs to be in the same directory as the executable, and CMake attempts to copy the correct (32-bit or 64-bit) `SDL2.dll` for Windows, `libSDL2.so` for *nix, to the executable path.  If CMakes fails to copy the file, you will need to do so manually (see the SDL2 dependency `libs` directory).

Program distribution should only require the executable itself and `SDL2.dll` for Windows and `libSDL2.so` for *nix.  If it is expected that the target system will have SDL installed globally, then only the executable should be required.

The default build will statically link core libs, so when compiling with MinGW, for example, the MinGW DLLs are *not* required to be distributed with the executable.


## Running

Native OS, just run the binary:

- *nix build: `build_unix/bin/starterkit`
- MinGW build: `build_mgw/bin/starterkit`
- Windows build: `build_win/bin/Debug/starterkit`


### VirtualBox Guest

Linux guest VM under VirtualBox with VMSVGA only supports OpenGL up to 2.1.  The VboxSVGA setting supports OpenGL 3.x, but apparently has security issues (need reference) and does not work for Linux any more?  There are a few options:

- Compile with SK_GL21_GLSL120 defined (see disco.cpp) which limits GL to 2.1.
- If using MESA, use the Gallium LLVMpipe software renderer to run:

`LIBGL_ALWAYS_SOFTWARE=true GALLIUM_DRIVER=llvmpipe build_unix/bin/starterkit`

The software renderer does not seem to support VSYNC, so there is a simple frame-rate limiter in the render thread that may need to be modified.

----


## Code Organization

The code is roughly split into to main parts, the OS abstraction (event-loop and rendering) and the program.  The abstraction part is called `disco`, which is an attempt to be a catchy name for "Display and I/O".

The SDL2 event-handler (the message pump) is required to run on the process started by the OS when the program is run.  To create a program that is event-friendly and CPU-use friendly, a typical message pump likes to block in a system call waiting on events.

A blocking message pump is good for CPU use and system load, but this can be tricky for games and emulators which usually poll the message pump and are driven more by frame update time (and consume more CPU time).

The Starter Kit design tries to strike a balance between game-like performance and being nice on CPU load, by incorporating a few ideas:

1. A rendering thread is created to allow the program to have a responsive interface and update the window while it is being dragged, resized, etc..
2. The event-loop uses a blocking call to wait-for and respond to events.
3. The main program can create its own thread (or threads) to run in the background if necessary.

The rendering thread has several callback hooks that can be called every frame.  This allows a program to run code for every frame to generate the UI and graphics, run program logic etc..

The event-handler has a callback hook that can be called for every event.  If the program does not handle the event, a default handler is used.

Between the once-per-frame rendering and event callbacks, a program make never require any more sophistication.

The program organization looks something like this:

```
OS process
  main()
    initialize
    call disco_run()
  end

  disco_run()
    initialize
    create render_thread()
    while running
      wait for event
      events() callback
    loop
  end

  render_thread()
    initialize
    while running
      draw_ui() callback
      render ImGUI
      draw_post_ui() callback
      wait vsync
    loop
  end
```

The rendering thread is set to synchronize with the display's vertical sync, so the `draw_ui()` and `draw_post_ui()` callbacks will be called once per frame.  The `event()` callback will be called any time there is a event not handled by ImGUI.

The graphic-context for the window is created in the render thread, so all drawing needs to happen in functions called by the render thread, i.e. the `draw_ui()` and `draw_post_ui()` callbacks.

----

## ImGUI Fonts

ImGUI provides some true-type fonts to use instead of the single built-in "proggy-clean" font.

- "Cousine" font has slashed zeros, so it wins.
- "Roboto" font is also nice looking, as well as "Karla".

The true-type fonts need to be converted to a header file and included in the source code.  ImGUI comes with a simple utility to do the conversion.

Building and converting a font using a VS2019 command prompt (unix shell would just use the system compiler):

```
$ cd deps/imgui/misc/fonts

// pick one of these depending on your system compiler:
$ cl binary_to_compressed_c.cpp
$ clang -o binary_to_compressed_c binary_to_compressed_c.cpp
$ g++ -o binary_to_compressed_c binary_to_compressed_c.cpp

// Convert a font file to a compressed header:
$ ./binary_to_compressed_c.exe -base85 Cousine-Regular.ttf cousine_font > cousine_font.h

// Copy the font header to the primary source directory and include like this:
#include "cousine_font.h"

// Add to ImGUI with:
ImGui::GetIO().Fonts->AddFontFromMemoryCompressedBase85TTF(cousine_font_compressed_data_base85, 18);
```

----

## License

In the spirit of the STB library, this code is in the public domain. You can do anything you want with it.  You have no legal obligation to do anything else, although attribution is greatly appreciated.

The code is also licensed under the MIT open source license, if you are unhappy about using public domain.  Every source file includes an explicit dual-license for you to choose from.
