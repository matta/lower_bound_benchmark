#!/usr/bin/env bash

set -x
set -e
set -u

./build.sh

function run_bench() {
    compiler=$1
    benchmark=build_${compiler}/Release/lower_bound
    out=results/${compiler}.json
    mkdir -p results
    taskset -c 0 \
            $benchmark \
            --benchmark_out=$out \
            --benchmark_out_format=json \
            --benchmark_repetitions=20 \
            --benchmark_context=compiler=$compiler
}

run_bench gcc
run_bench clang
