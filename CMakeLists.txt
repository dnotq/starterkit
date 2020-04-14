#
# Starter Kit base CMake.
#

project(STARTERKIT)

# The FindSDL script uses SDL2_DIR as a hint.
set(SDL2_DIR "${CMAKE_SOURCE_DIR}/src/deps/SDL2")
set(IMGUI_DIR "${CMAKE_SOURCE_DIR}/src/deps/imgui")
set(LIBUV_DIR "${CMAKE_SOURCE_DIR}/src/deps/libuv")
set(STB_DIR "${CMAKE_SOURCE_DIR}/src/deps/stb")

# The minimum CMake version is actually unknown.  This build has only been
# tested with CMake 3.16.x.
cmake_minimum_required(VERSION 3.5.1)
set(CMAKE_COLOR_MAKEFILE ON)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${PROJECT_BINARY_DIR}/bin)

# Add the project directory to the module path so FindSDL will be picked up.
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_SOURCE_DIR}")

find_package(SDL2 REQUIRED)
find_package(OpenGL REQUIRED)

set(CMAKE_CXX_FLAGS "-O2")

# Detect the architecture type.
if ( CMAKE_SIZEOF_VOID_P EQUAL 8 )
    set(ARCHTYPE "x64")
else ()
    set(ARCHTYPE "x86")
endif ()

add_subdirectory(${LIBUV_DIR})
add_subdirectory(src)
