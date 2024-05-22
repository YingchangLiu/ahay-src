#!/bin/bash
./configure CC=gcc-11 CXX=g++-11 CUDAFLAGS='-x cu --compiler-bindir=/usr/x86_64-pc-linux-gnu/gcc-bin/11/ -ccbin=/usr/bin/g++-11' API=c++,f90

