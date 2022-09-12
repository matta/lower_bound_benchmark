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

function build_test() {
    cmake --build . --config "$1" && ctest -C "$1" --output-on-failure
}

function build() {
    (cd build_$1 && build_test Asan && build_test Release)
}

configure gcc g++
configure clang clang++

if [[ ! -f compile_commands.json ]]; then
    rm -f compile_commands.json
    ln -s build_clang/compile_commands.json
fi

build gcc
build clang
