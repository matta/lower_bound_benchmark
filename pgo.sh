#!/usr/bin/env bash

# https://johnysswlab.com/tune-your-programs-speed-with-profile-guided-optimizations/
# https://stackoverflow.com/questions/4365980/how-to-use-profile-guided-optimizations-in-g

set -eu
set -x

./build.sh
cmake --build build_gcc \
      --config Release \
      --target lower_bound_benchmark
cmake --build build_clang \
      --config Release \
      --target lower_bound_benchmark

PROFILE_DIR=$(pwd)/build_profile_dir
mkdir -p $PROFILE_DIR

BUILD_DIR=$(pwd)/build_profile_generate
if [[ ! -d $BUILD_DIR ]]; then
    mkdir -p $BUILD_DIR
    CC=$(which gcc) \
      CXX=$(which g++) \
      cmake \
      -G 'Ninja' \
      -DCMAKE_BUILD_TYPE=RelWithProfileGenerate \
      -DLOWER_BOUND_BENCHMARK_PROFILE_DIR=$PROFILE_DIR \
      -DLOWER_BOUND_BENCHMARK_PROFILE_PREFIX_PATH=$BUILD_DIR \
      -B $BUILD_DIR \
      -S .
fi
cmake --build $BUILD_DIR --target lower_bound_benchmark

rm -rf $PROFILE_DIR
mkdir -p $PROFILE_DIR
$BUILD_DIR/lower_bound_benchmark \
    --benchmark_filter='LowerBound/LayoutRandom/AccessRandom/20$'


BUILD_DIR=$(pwd)/build_profile_use
if [[ ! -d $BUILD_DIR ]]; then
    mkdir -p $BUILD_DIR
    CC=$(which gcc) \
      CXX=$(which g++) \
      cmake \
      -G 'Ninja' \
      -DCMAKE_BUILD_TYPE=RelWithProfileUse \
      -DLOWER_BOUND_BENCHMARK_PROFILE_DIR=$PROFILE_DIR \
      -DLOWER_BOUND_BENCHMARK_PROFILE_PREFIX_PATH=$BUILD_DIR \
      -B $BUILD_DIR \
      -S .
fi
cmake --build $BUILD_DIR --target clean
cmake --build $BUILD_DIR --target lower_bound_benchmark
taskset -c 3 $BUILD_DIR/lower_bound_benchmark \
    --benchmark_filter=LowerBound/LayoutAscending/AccessRandom/20
taskset -c 3 build_gcc/Release/lower_bound_benchmark \
    --benchmark_filter=LowerBound/LayoutAscending/AccessRandom/20
taskset -c 3 build_clang/Release/lower_bound_benchmark \
    --benchmark_filter=LowerBound/LayoutAscending/AccessRandom/20
