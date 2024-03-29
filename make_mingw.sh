#!bash

#export CC=clang
#export CXX=clang++
export CC=gcc
export CXX=g++
cmake -E make_directory build_mingw

# Check for a command-line config parameter (Release or Debug)
if [ -z "$1" ] ; then
   echo
   echo "You can pass a \"Release\" or \"Debug\" parameter to specify the build type."
   echo
   cmake -E chdir build_mingw cmake -G "MinGW Makefiles" ..
   cmake --build build_mingw
else
   cmake -E chdir build_mingw cmake -G "MinGW Makefiles" -DCMAKE_BUILD_TYPE=$1 ..
   cmake --build build_mingw --config $1
fi

echo "END OF LINE."
