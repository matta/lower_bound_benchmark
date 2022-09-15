#!/usr/bin/env bash

set -euo pipefail
set -x

./build.sh

. ./benchlib.sh

function run_bench() {
    compiler=$1
    benchmark=build_${compiler}/Release/lower_bound_benchmark
    mkdir -p results
    set -x
    out=${outfiles[${compiler}]}
    log=${logfiles[${compiler}]}
    (taskset -c 0 \
             $benchmark \
             --benchmark_out=$out \
             --benchmark_out_format=json \
             --benchmark_repetitions=10 \
             --benchmark_context=compiler=$compiler) |& \
        tee $log
}

run_bench gcc
run_bench clang
