cmake_minimum_required(VERSION 2.8)
project(chisel)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/bin/)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_LIST_DIR}/cmake")

find_package(LLVM REQUIRED)
find_package(Clang REQUIRED)

set(CMAKE_CXX_FLAGS "-w -std=c++11 ${LLVM_COMPILE_FLAGS}")

include_directories(${LLVM_INCLUDE_DIRS})
include_directories(${CLANG_INCLUDE_DIRS})
include_directories(include)

add_executable(chisel
  src/chisel.cc
  src/core/TransformationManager.cc
  src/core/Transformation.cc
  src/core/GlobalReduction.cc
  src/core/LocalReduction.cc
  src/utils/RewriteUtils.cc
  src/utils/Options.cc
  src/utils/Report.cc
  src/utils/Counting.cc
  src/utils/Profiling.cc
  src/utils/Stats.cc
  src/utils/StringUtils.cc
)

target_link_libraries(chisel ${CLANG_LIBS} ${LLVM_LIBS_CORE} ${LLVM_LDFLAGS})
