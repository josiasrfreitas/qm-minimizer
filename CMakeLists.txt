cmake_minimum_required(VERSION 3.16)
project(qm-minimizer LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang|AppleClang")
    add_compile_options(-Wall -Wextra -Wpedantic -Wno-unused-parameter)
    add_compile_options($<$<CONFIG:Release>:-O3>)
    add_compile_options($<$<CONFIG:Release>:-march=native>)
endif()

find_package(Threads REQUIRED)

add_library(qm-core STATIC
    src/bitvec.cc
    src/term.cc
    src/pla_reader.cc
    src/pla_writer.cc
    src/quine_mccluskey.cc
)
target_include_directories(qm-core PUBLIC include)
target_link_libraries(qm-core PUBLIC Threads::Threads)

add_executable(qm src/main.cc)
target_link_libraries(qm PRIVATE qm-core)

set_target_properties(qm PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}")

enable_testing()
add_test(
    NAME smoke_and
    COMMAND $<TARGET_FILE:qm> ${CMAKE_SOURCE_DIR}/data/tests/ex_and.pla
)
add_test(
    NAME smoke_3var
    COMMAND $<TARGET_FILE:qm> ${CMAKE_SOURCE_DIR}/data/tests/ex01_funcao3var.pla
)
add_test(
    NAME smoke_4var
    COMMAND $<TARGET_FILE:qm> ${CMAKE_SOURCE_DIR}/data/tests/ex02_funcao4var.pla
)
