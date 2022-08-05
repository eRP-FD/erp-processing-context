#!/bin/bash -ex
cd /root/llvm-build
git init llvm-project
cd llvm-project
git remote add origin https://github.com/llvm/llvm-project.git
# llvmorg-15-init-15826-g9b37d48dd9c2 from Thu Jul 7 09:54:09 2022 +0200
git fetch --depth=1 origin 9b37d48dd9c26a277c84dd0691173c83616306e2:main
git checkout main
mkdir build
cd build
CC=gcc-11 CXX=g++-11 cmake -C../../clang-tidy.cmake -GNinja ../llvm
cmake --build . --target install
cd /root
rm -rf llvm-build
