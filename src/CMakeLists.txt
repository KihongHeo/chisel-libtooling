cmake_minimum_required(VERSION 2.8)

find_package(LLVM REQUIRED)
find_package(Clang REQUIRED)

set(CMAKE_CXX_FLAGS "-w -std=c++11 ${LLVM_COMPILE_FLAGS}")

include_directories(${LLVM_INCLUDE_DIRS})
include_directories(${CLANG_INCLUDE_DIRS})


set(CLANG_TOOL_SRC

        main.cc
        
        actions/frontendaction.cc
        consumer/consumer.cc
        utils/utils.cc
        
        transformer/transformer.cc
        transformer/global_element_remover.cc
        
        finder/finder.cc
        finder/global_element_finder.cc
)

# remove trailing whitespaces
string(STRIP ${LLVM_LDFLAGS} LLVM_LDFLAGS)

add_executable(clang-tool ${CLANG_TOOL_SRC})
target_link_libraries(clang-tool ${CLANG_LIBS} ${LLVM_LIBS_CORE} ${LLVM_LDFLAGS})

