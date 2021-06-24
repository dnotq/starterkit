#!bash

export CC=clang
export CXX=clang++
#export CC=gcc
#export CXX=g++
cmake -E make_directory build_unix
cmake -E chdir build_unix cmake -G "MinGW Makefiles" ..
cmake --build build_unix
