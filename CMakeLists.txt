cmake_minimum_required(VERSION 3.23)

project(
    FSM
    VERSION 0.0.1
    DESCRIPTION "A header-only library that provides template classes to implement Finite State Machine"
    HOMEPAGE_URL "https://github.com/adchawla/FSM")

if(NOT DEFINED CMAKE_CXX_STANDARD)
    set(CMAKE_CXX_STANDARD 17)
    set(CMAKE_CXX_STANDARD_REQUIRED ON)
endif()

add_library(${PROJECT_NAME} INTERFACE)
target_include_directories(
    ${PROJECT_NAME}
    INTERFACE $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>)

if(PROJECT_IS_TOP_LEVEL)
    add_subdirectory(common)
    add_subdirectory(tests)
    add_subdirectory(benchmarks)
endif()
