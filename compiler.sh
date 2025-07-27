#!/usr/bin/env bash

# This script provides an example for configuring and compiling Madagascar.
# It is intended for use only if issues arise during the standard compilation process.
# Please modify this script to suit your specific distribution and environment.

## C++ and Fortran-90 API bindings
API=c++,f90

## Installation prefix
prefix=$HOME/opt/ahay

## The default compilers for C, C++, and Fortran
CC=gcc-11
CXX=g++-11
F77=gfortran-11
FORTRAN=gfortran-11
F90=gfortran-11
#CC=gcc
#CXX=g++
#F77=gfortran
#FORTRAN=gfortran
#F90=gfortran


## Configure madagascar
./configure API=$API --prefix=$prefix CC=$CC CXX=$CXX F77=$F77 FORTRAN=$FORTRAN F90=$F90 #CUDAFLAGS=$CUDAFLAGS
## Full configuration command for reference
## ./configure CC=gcc-11 CXX=g++-11 F77=gfortran-11 FORTRAN=gfortran-11 F90=gfortran-11 CUDAFLAGS='-x cu --compiler-bindir=/usr/x86_64-pc-linux-gnu/gcc-bin/11/ -ccbin=/usr/bin/g++-11' API=c++,f90


## Compile madagascar
# make
# make install
