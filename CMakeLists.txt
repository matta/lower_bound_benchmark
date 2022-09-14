cmake_minimum_required(VERSION 3.14)
project(LowerBound CXX)

# specify the C++ standard
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED True)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

include(CTest)

#
# BEGIN Address Sanitizer Incantations
#
get_property(isMultiConfig GLOBAL PROPERTY GENERATOR_IS_MULTI_CONFIG)

if(isMultiConfig)
    if(NOT "Asan" IN_LIST CMAKE_CONFIGURATION_TYPES)
        list(APPEND CMAKE_CONFIGURATION_TYPES Asan)
    endif()
else()
    set(allowedBuildTypes Asan Debug Release RelWithDebInfo MinSizeRel)
    set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS "${allowedBuildTypes}")

    if(CMAKE_BUILD_TYPE AND NOT CMAKE_BUILD_TYPE IN_LIST allowedBuildTypes)
        message(FATAL_ERROR "Invalid build type: ${CMAKE_BUILD_TYPE}")
    endif()
endif()

set(CMAKE_C_FLAGS_ASAN
    "${CMAKE_C_FLAGS_DEBUG} -fsanitize=address -fno-omit-frame-pointer" CACHE STRING
    "Flags used by the C compiler for Asan build type or configuration." FORCE)

set(CMAKE_CXX_FLAGS_ASAN
    "${CMAKE_CXX_FLAGS_DEBUG} -fsanitize=address -fno-omit-frame-pointer" CACHE STRING
    "Flags used by the C++ compiler for Asan build type or configuration." FORCE)

set(CMAKE_EXE_LINKER_FLAGS_ASAN
    "${CMAKE_SHARED_LINKER_FLAGS_DEBUG} -fsanitize=address" CACHE STRING
    "Linker flags to be used to create executables for Asan build type." FORCE)

set(CMAKE_SHARED_LINKER_FLAGS_ASAN
    "${CMAKE_SHARED_LINKER_FLAGS_DEBUG} -fsanitize=address" CACHE STRING
    "Linker lags to be used to create shared libraries for Asan build type." FORCE)
#
# END Address Sanitizer Incantations
#

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
target_link_libraries(lower_bound benchmark::benchmark doctest)
add_test(
  NAME lower_bound
  COMMAND lower_bound --benchmark_filter=/5$ --benchmark_min_time=0.01)
