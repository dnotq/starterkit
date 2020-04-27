#!/bin/sh

cmake -E make_directory build
cmake -E chdir build cmake ..
cmake --build build
