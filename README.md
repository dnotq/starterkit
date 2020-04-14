# Starter Kit for Desktop Application Development.

Provides a C/C++ starter-kit setup for cross-platform application or game
development.  Executables created with this kit will be very small and only have
a few DLL or SO dependencies (SDL2 and LibUV).

## Requirements

- A system with OpenGL installed and available to the compiler and linker.
- A C and C++ compiler.  For Windows, Visual Studio 2019 has been tested.
- CMake with support for the system compiler.
- Git.

## Dependencies

TL;DR: Clone or download the dependency libraries into the `src/deps` directory
which should end up like this:

```
src/deps/imgui
src/deps/libuv
src/deps/SDL2
src/deps/stb
```

### The longer description:

The following libraries are used to provide abstraction and cross platform
support for network, file system, graphics, sound, and user IO.  The libraries
are all open-source and written in C (at least most of them are).

Unless otherwise noted, all libraries support Unix/Win/MAC.

- Download SDL2 prebuilt dev and extract to src/deps/SDL2.
- Alternate, install SDL2 with your system package manager.(1)
- Clone LibUV into src/deps/libuv.
- Alternate, install LibUV2 with your system package manager.(1)
- Clone IMGUI into src/deps/imgui.
- Clone STB into src/deps/stb.
- Run build_xxx for whatever platform.(2)

Note(1): Even if SDL2 and LibUV are installed with package managers, the build
and application code still need the development sources installed so the include
header files will be available.

Note(2): A Debug build is the default when not specified.  LibUV will generate a
lot of warnings when built with VS2019 and CMake (see LibUV section below).


## LibUV 1.x

Cross platform asynchronous file access, networking, and threads.

- Website:  [https://libuv.org/](https://libuv.org/)
- Clone:    [https://github.com/libuv/libuv.git](https://github.com/libuv/libuv.git)
- Download: [https://dist.libuv.org/dist/](https://dist.libuv.org/dist/)
- Docs:     [http://docs.libuv.org/en/v1.x/](http://docs.libuv.org/en/v1.x/)

Notes: Build from source for Windows, build from source or install via package
manager on Unix / Mac.

### Windows Build:

The main Starter-Kit CMakeLists.txt file adds the LibUV directory, so the LibUV
CMakeLists.txt script will be found and executed by CMake when the Starter-Kit
is built for the first time.  When LibUV is built this way, it currently
generates a lot of warnings with VS2019 and VS2017 (the only two VS environments
tested).  These builds also fail a few of the LibUV tests.  Hopefully the LibUV
project will be fixing these warnings and bugs soon.


## SDL2 2.x

Provides cross platform windowing, graphics, sound, events, and user I/O.

- Website:  [https://libsdl.org/](https://libsdl.org/)
- Clone:    [http://hg.libsdl.org/SDL](http://hg.libsdl.org/SDL)
- Download: [https://libsdl.org/download-2.0.php](https://libsdl.org/download-2.0.php)
- Docs:     [https://wiki.libsdl.org/FrontPage](https://wiki.libsdl.org/FrontPage)

Notes: Use prebuilt DLLs for Windows, install dev libs via package manager on
unix systems.

Extract the library to `src/deps/SDL2-2.x.xx` and remove the version part so the
path is only `src/deps/SDL2`.


## IMGUI

Cross platform Immediate-Mode GUI with widgets.

- Website:  [https://github.com/ocornut/imgui](https://github.com/ocornut/imgui)
- Clone:    [https://github.com/ocornut/imgui.git](https://github.com/ocornut/imgui.git)
- Docs:     [https://github.com/ocornut/imgui/tree/master/docs](https://github.com/ocornut/imgui/tree/master/docs)

Notes: Included as source.


## STB

Single-file public domain (or MIT licensed) libraries for C/C++.

- Website:  [https://github.com/nothings/stb](https://github.com/nothings/stb)
- Clone:    [https://github.com/nothings/stb.git](https://github.com/nothings/stb.git)
- Docs:     See each source file.

Notes: Included as source.

----

## Compiling

Once the dependencies are set up, building the Starter-Kit can be done with one
of the provided scripts:

- `build_win.bat` for Windows.
- `build_unix.sh` for Unix or Mac.

The scripts are very basic and are just system agnostic CMake commands that do
the following:

```
$ mkdir build
$ cd build
$ cmake ..
$ cmake --build .
```

The final executable can be found in:

- `build/bin/starterkit` on Unix / Mac
- `build/bin/Debug/starterkit.exe` on Windows.

VS creates the Debug and Release directories inside the specified destination,
so for a Windows build the final executable path will be `build/bin/Debug` for
the Debug build.

Windows also needs the SDL2.dll in the same directory as the executable, and the
CMake script attempts to copy the correct (32-bit or 64-bit) SDL2.dll to the
executable path.  If CMakes fails to copy the file, you will need to do so
manually (see the SDL2 dependency `libs` directory).


----


## Code Organization

The code is roughly split into to main parts consisting of the GUI and the
application.

The SDL2-based GUI is required to run on the main process, so a LibUV thread
is created to run the application.  Both SDL2 and LibUV work in an event
type configuration, and application code is generally executed via call-backs
(in the case of LibUV), or in response to user events (in the case of SDL2).

The GUI and application-thread communicate via an asynchronous LibUV signal.

```
main -+---> sk_gui_run()  SDL2+IMGUI
      |        |
      |        | GUI calls to let app know of a user action.
      |        V
      |     sk_app_ext_signal()
      |        |
      |        | triggers local LibUV event loop call-back.
      |        V
      +---> app_thread()  LibUV
```

Updates from the application to the GUI are simply via changes to application
data that the GUI is watching and presenting.

If a design requires both the GUI and application to update (write) the same
data, standard synchronization techniques should be used.  LibUV provides an
abstraction for synchronization.

### Main Loop

The SDL2-based GUI loop provided here is a hybrid between a game-style loop
which polls, and a purely event-based loop which always blocks waiting for
events.

This hybrid design is to avoid the straight game-style polling-loop which causes
higher CPU utilization (than an event-triggered loop), and is undesirable when
writing more traditional (non-game) applications.

The loop will block for a maximum timeout waiting for events, and if no events
are received it will go ahead and run through the loop once to allow non-event
type activity to be performed.

This format makes the loop adaptable and it becomes more responsive when there
are a lot of events, i.e. a lot of user interaction, since the event-wait call
will return immediately.  When there are no events, the loop scales back to
spending most of its time waiting, which keeps its CPU use low like most
event-only applications.  However the loop is still executed at a minimum rate
even when there are no events.

The loop can easily be changed to a polling-only or event-only type, however
in a purely event-based loop the UI will be sluggish, which is due to the
Immediate-Mode design.

In practice, the default hybrid loop shows around 1% to 2% CPU when idle.  The
polling-only loop idles around 20% CPU.  Of course these statistics are from one
particular system (typical 2016, ~2.5GHz, x64 laptop).

----

# IMGUI Fonts

IMGUI provides some true-type fonts to use instead of the single built-in
"proggy-clean" font.

- "Cousine" font has slashed zeros, so it wins.
- "Roboto" font is also nice looking, as well as "Karla".

The true-type fonts need to be converted to a header file and included in the
source code.  IMGUI comes with a simple utility to do the conversion.

Building and converting a font using a VS2019 command prompt (unix shell would
just use the system compiler):

```
$ cd deps/imgui/misc/fonts

// pick one of these depending on your system compiler:
$ cl binary_to_compressed_c.cpp
$ clang -o binary_to_compressed_c binary_to_compressed_c.cpp
$ g++ -o binary_to_compressed_c binary_to_compressed_c.cpp

// Convert a font file to a compressed header:
$ ./binary_to_compressed_c.exe -base85 Cousine-Regular.ttf cousine_font > cousine_font.h

// Copy the font header to the app source directory and include like this:
#include "cousine_font.h"

// Add to IMGUI with:
ImGui::GetIO().Fonts->AddFontFromMemoryCompressedBase85TTF(cousine_font_compressed_data_base85, 18);
```

----

# License

In the spirit of the STB library, this code is in the public domain. You can do
anything you want with it.  You have no legal obligation to do anything else,
although I appreciate attribution.

The code is also licensed under the MIT open source license, if you are unhappy
about using public domain.  Every source file includes an explicit dual-license
for you to choose from.
