#!/usr/bin/env bash

set -euo pipefail
set -x

./build.sh

function run_bench() {
    host=$(hostname)
    compiler=$1
    benchmark=build_${compiler}/Release/lower_bound_benchmark
    mkdir -p results
    results=results/$host/$compiler
    mkdir -p "$results"
    ./runbench.py \
        "$results" \
        taskset -c 0 \
        $benchmark
}

run_bench gcc
run_bench clang
