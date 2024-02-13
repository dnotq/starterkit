@echo off
REM Starter Kit Windows Build.
REM Requires Visual Studio with CMake.

REM Settings if CMake is not doing the right thing.
REM cmake -G "Visual Studio 16 2019" -A Win32
REM cmake -G "Visual Studio 16 2019" -A x64
REM cmake -E chdir build_win cmake -G "Visual Studio 16 2019" -A Win32 ..

cmake -E make_directory build_win

REM Check for a command-line config parameter (Release or Debug)
if [%1]==[] goto no_config_parm
cmake -E chdir build_win cmake -DCMAKE_BUILD_TYPE=%1 ..
cmake --build build_win --config %1
goto done

:no_config_parm
echo.
echo You can pass a "Release" or "Debug" parameter to specify the build type.
echo.
cmake -E chdir build_win cmake ..
cmake --build build_win

:done
echo END OF LINE.
