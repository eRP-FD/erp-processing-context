#!/bin/bash -ex
cd /root/llvm-build
git init llvm-project
cd llvm-project
git remote add origin https://github.com/llvm/llvm-project.git
# llvmorg-15-init-15826-g9b37d48dd9c2 from Thu Jul 7 09:54:09 2022 +0200
git fetch --depth=1 origin llvmorg-15.0.2
git checkout FETCH_HEAD
mkdir build
cd build
CC=gcc-11 CXX=g++-11 cmake -C../../clang-tidy.cmake -GNinja ../llvm
cmake --build . --target install
cd /root
rm -rf llvm-build
