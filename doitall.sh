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

build gcc
build clang

COMPARE=./build_gcc/_deps/googlebenchmark-src/tools/compare.py
taskset -c 0 $COMPARE \
        benchmarks \
        ./build_clang/Release/lower_bound \
        ./build_gcc/Release/lower_bound \
        --benchmark_filter=1048576 \
        --benchmark_repetitions=10
