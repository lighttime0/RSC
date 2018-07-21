cmake_minimum_required(VERSION 2.8)

project(RSC)

set(CMAKE_INSTALL_PREFIX "${PROJECT_SOURCE_DIR}/..")

find_package(LLVM REQUIRED CONFIG)

message(STATUS "Found LLVM ${LLVM_PACKAGE_VERSION}")
message(STATUS "Using LLVMConfig.cmake in: ${LLVM_DIR}")

add_definitions(${LLVM_DEFINITIONS})
include_directories(${LLVM_INCLUDE_DIRS})
link_directories(${LLVM_LIBRARY_DIRS})

include_directories("include/")

set(CMAKE_CXX_FLAGS "-std=c++11 -fPIC -fopenmp")

add_subdirectory(lib)
add_subdirectory(tools/rsc)
