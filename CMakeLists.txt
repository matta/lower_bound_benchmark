cmake_minimum_required(VERSION 3.14)
project(LowerBound CXX)

# specify the C++ standard
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED True)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

include(FetchContent)

# Disable all testing of the Google Benchmark library.  This speeds up the
# build significantly, and lets us not download Google Test.
set(BENCHMARK_ENABLE_TESTING OFF)

# Disable -Werror in benchmark code because I was getting a weird error
# from gcc compiling benchmark.cc related to std::string and
# -Werror=restrict that occurs only with C++20 enabled.
set(BENCHMARK_ENABLE_WERROR OFF)

# FetchContent_Declare(googletest
#   GIT_REPOSITORY https://github.com/google/googletest.git
#   GIT_TAG main
# )

FetchContent_Declare(googlebenchmark
  GIT_REPOSITORY https://github.com/google/benchmark.git
  GIT_TAG main
)

FetchContent_Declare(doctest
  GIT_REPOSITORY https://github.com/doctest/doctest.git
  GIT_TAG master
)

FetchContent_MakeAvailable(
  # googletest
  googlebenchmark
  doctest
)

# add the executable
add_executable(lower_bound lower_bound.cxx)
target_link_libraries(lower_bound benchmark::benchmark_main)
