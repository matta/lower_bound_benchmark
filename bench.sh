#!/usr/bin/env bash

set -euo pipefail
set -x

./build.sh

function run_bench() {
    compiler=$1
    benchmark=build_${compiler}/Release/lower_bound
    mkdir -p results
    out=results/${compiler}.json
    log=results/${compiler}.log
    (taskset -c 0 \
             $benchmark \
             --benchmark_out=$out \
             --benchmark_out_format=json \
             --benchmark_repetitions=20 \
             --benchmark_context=compiler=$compiler) |& \
        tee $log
}

run_bench gcc
run_bench clang
