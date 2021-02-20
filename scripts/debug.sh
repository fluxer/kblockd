#!/bin/sh

set -e

cwd="$(dirname $(realpath $0))"

rm -rf "$cwd/../build"
mkdir -p "$cwd/../build"
cd "$cwd/../build"

if which clang && which clang++; then
    export CC=clang
    export CXX=clang++
fi

cmake ../ -DCMAKE_BUILD_TYPE=Debug \
    -DCMAKE_INSTALL_PREFIX=/usr $@
make -j$(nproc || echo 1)
