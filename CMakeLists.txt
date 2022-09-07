cmake_minimum_required(VERSION 3.14)
project(LowerBound CXX)

# specify the C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)

include(FetchContent)

# Disable all testing of the Google Benchmark library.  This speeds up the
# build significantly, and lets us not download Google Test.
set(BENCHMARK_ENABLE_TESTING OFF)

# FetchContent_Declare(googletest
#   GIT_REPOSITORY https://github.com/google/googletest.git
#   GIT_TAG main
# )

FetchContent_Declare(googlebenchmark
  GIT_REPOSITORY https://github.com/google/benchmark.git
  GIT_TAG main
)

FetchContent_MakeAvailable(
  # googletest
  googlebenchmark
)

# add the executable
add_executable(lower_bound lower_bound.cxx)
target_link_libraries(lower_bound benchmark::benchmark_main)
