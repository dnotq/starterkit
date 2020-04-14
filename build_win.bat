@echo off
REM Starter Kit Windows Build.
REM Requires Visual Studio with CMake.
REM cmake -G "Visual Studio 16 2019" -A Win32
REM cmake -G "Visual Studio 16 2019" -A x64

REM cmake -E chdir build cmake -G "Visual Studio 16 2019" -A Win32 ..
cmake -E make_directory build
cmake -E chdir build cmake ..
cmake --build build
