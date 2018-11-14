cmake_minimum_required(VERSION 2.8)

find_package(LLVM REQUIRED)
find_package(Clang REQUIRED)

set(CMAKE_CXX_FLAGS "-w -std=c++11 ${LLVM_COMPILE_FLAGS}")

include_directories(${LLVM_INCLUDE_DIRS})
include_directories(${CLANG_INCLUDE_DIRS})
include_directories(../include)

set(CHISEL_SRC

        chisel.cc
        
        core/TransformationManager.cc
        core/Transformation.cc
        core/GlobalReduction.cc
        utils/RewriteUtils.cc
        utils/Options.cc
        utils/Report.cc
        utils/Counting.cc
        utils/Profiling.cc
        utils/Stats.cc
        utils/StringUtils.cc
)

# remove trailing whitespaces
string(STRIP ${LLVM_LDFLAGS} LLVM_LDFLAGS)

add_executable(chisel ${CHISEL_SRC})
target_link_libraries(chisel ${CLANG_LIBS} ${LLVM_LIBS_CORE} ${LLVM_LDFLAGS})
