##
# Starter Kit.
#

project(STARTERKIT)

# cmake is unhappy without a required version specification.
cmake_minimum_required(VERSION 3.5)
set(CMAKE_COLOR_MAKEFILE ON)

# Designate a binary directory.
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${PROJECT_BINARY_DIR}/bin)

# Compiler options
if (MSVC)
    # -MT is suppose to include the C/C++ runtime which should prevent "dll not found"
    # errors on different versions of Windows, and prevent needing to distribute a
    # "C++ runtime".  Mileage may vary.
    # fp:fast  Fast floating point (vs. precise or strict)
    # W4       Output warning level
    # MT       Compiles to create a multithreaded executable file, by using LIBCMT.lib
    # Gy       Enable function-level linking
    # O2       Creates fast code
    # Od       Disabled optimization
    # Oi       Create intrinsic functions
    # Zi       Generates complete debugging information
    if (CMAKE_BUILD_TYPE STREQUAL "Release")
        set(CMAKE_CXX_FLAGS "-nologo -fp:fast -W4 -MT  -Gy -O2 -Oi")
    else ()
        set(CMAKE_CXX_FLAGS "-nologo -fp:fast -W4 -MTd -Gy -Od -Zi")
    endif ()

    # -subsystem:windows,5.01 should allow running on WinXP and above.
    # This option controls if a console window is opened when running.  Even if run  from
    # the command line, without this option the program will not have a stdout.
    # Minimums: 5.01 (x86), 5.02 (x64), 6.02 (ARM)
    #set(CMAKE_EXE_LINKER_FLAGS "-subsystem:windows,5.02")
    set(CMAKE_EXE_LINKER_FLAGS "-subsystem:windows")

elseif (MINGW)
    if (CMAKE_BUILD_TYPE STREQUAL "Release")
        # The -static option prevents needing to include the MinGW dlls with the exe.
        set(CMAKE_CXX_FLAGS "-Wall -Wextra -march=native -mtune=native -ffast-math -static -O2")

        # Set linker flags as necessary.
        # Remove debug symbols for release builds.
        set(CMAKE_EXE_LINKER_FLAGS "-Wl,--strip-debug")

    else ()
        set(CMAKE_CXX_FLAGS "-Wall -Wextra -march=native -mtune=native -ffast-math -static -Og -g")
    endif ()

else ()
    # TODO other switches for MacOS.
    set(CMAKE_CXX_FLAGS "-Wall -Wextra -O2")
    # TODO linker flags as necessary.
    #set(CMAKE_EXE_LINKER_FLAGS "")
endif ()

##
# Dependency locations.
set(IMGUI_DIR "${CMAKE_SOURCE_DIR}/src/deps/imgui")
set(STB_DIR "${CMAKE_SOURCE_DIR}/src/deps/stb")

##
# SDL2
#
# Must set:
#
# SDL2_BASE_DIR   base location of SDL2.
# SDL2_INC_DIR    proper SDL convention is #include "SDL.h", not <SDL2/SDL.h>.
# SDL2_LIB_DIR    link-time library directory.
# SDL2_RUNTIME    run-time DLL if needed (MinGW/Windows), otherwise leave empty.

if (MINGW)
    # MinGW
    set(SDL2_BASE_DIR "${CMAKE_SOURCE_DIR}/src/deps/SDL2_mingw_2.26.1/x86_64-w64-mingw32")
    set(SDL2_INC_DIR "${SDL2_BASE_DIR}/include/SDL2")
    set(SDL2_LIB_DIR "${SDL2_BASE_DIR}/lib")
    set(SDL2_RUNTIME "${SDL2_BASE_DIR}/bin/SDL2.dll")

elseif (MSVC)
    # Windows
    set(SDL2_BASE_DIR "${CMAKE_SOURCE_DIR}/src/deps/SDL2_vc_2.26.1")
    set(SDL2_INC_DIR "${SDL2_BASE_DIR}/include")
    set(SDL2_LIB_DIR "${SDL2_BASE_DIR}/lib/x64")
    set(SDL2_RUNTIME "${SDL2_BASE_DIR}/lib/x64/SDL2.dll")

elseif (APPLE)
    # TODO MACOS

else ()
    # Unix
    set(SDL2_BASE_DIR "/usr/include")
    set(SDL2_INC_DIR "${SDL2_BASE_DIR}/SDL2")
    set(SDL2_LIB_DIR "/lib/x86_64-linux-gnu/")
    set(SDL2_RUNTIME "")

endif ()

# Find the full paths to the SDL2 libs, which is the recommended practice.
find_library(SDL2_LIB_MAIN
    NAMES SDL2main
    HINTS ${SDL2_LIB_DIR})
find_library(SDL2_LIB_SDL2
    NAMES SDL2
    HINTS ${SDL2_LIB_DIR})

# Decision tree, again, to assemble the libs in the correct order.
if (MINGW)
	# Final link for MinGW should be: -lmingw32 -lSDL2main -lSDL2 -mwindows
	set(SDL2_LINK_LIBS "-lmingw32" ${SDL2_LIB_MAIN} ${SDL2_LIB_SDL2} "-mwindows")

elseif (MSVC)
	set(SDL2_LINK_LIBS ${SDL2_LIB_MAIN} ${SDL2_LIB_SDL2})

elseif (APPLE)
    # Untested, not sure if this is correct to MACOS.
	# Might need -framework CoreFoundation
	set(SDL2_LINK_LIBS "-framework Cocoa" ${SDL2_LIB_MAIN} ${SDL2_LIB_SDL2})

else ()
	set(SDL2_LINK_LIBS ${SDL2_LIB_MAIN} ${SDL2_LIB_SDL2})
endif ()

# SDL2
##

find_package(OpenGL REQUIRED)

if (MINGW)
    # Fix undefined references to:
	# 'ImmGetContext' 'ImmSetCompositionWindow' 'ImmReleaseContext'
	set(IMGUI_LINK_LIBS "Imm32")
endif ()



# Include the source files.
add_subdirectory(src)
