#!/usr/bin/env bash

set -x
set -e
set -u

function configure() {
    if [[ ! -d build_$1 ]]; then
        (
            mkdir build_$1
            cd build_$1
            CC=$1 CXX=$2 cmake -G 'Ninja Multi-Config' ..
        )
    fi
}

function build() {
    (cd build_$1 && cmake --build . --config Release)
}

configure gcc g++
configure clang clang++

if [[ ! -f compile_commands.json ]]; then
    ln -s build_clang/compile_commands.json
fi

build gcc
build clang
