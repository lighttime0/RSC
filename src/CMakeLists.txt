cmake_minimum_required(VERSION 3.4.3)

project(RSC)

find_package(LLVM REQUIRED CONFIG)

message(STATUS "Found LLVM ${LLVM_PACKAGE_VERSION}")
message(STATUS "Using LLVMConfig.cmake in: ${LLVM_DIR}")

# Set project compile flags. Enable C++11 support and openmp, etc
# -Wall -fno-rtti : https://stackoverflow.com/questions/17225956/developing-an-llvm-pass-with-cmake-out-of-llvm-source-directory
set(CMAKE_CXX_FLAGS "-std=c++11 -fPIC -fopenmp -Wall -fno-rtti")

add_definitions(${LLVM_DEFINITIONS})
include_directories(${LLVM_INCLUDE_DIRS})

add_subdirectory(tools/demo-tool)