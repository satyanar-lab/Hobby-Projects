cmake_minimum_required(VERSION 3.20)

project(
    body_control_zonal_lighting
    VERSION 1.0.0
    DESCRIPTION "Body Control Zonal Lighting over Automotive Ethernet"
    LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(BCZL_BUILD_APPS "Build executable applications" ON)
option(BCZL_BUILD_TESTS "Build unit and integration tests" OFF)
option(BCZL_ENABLE_VSOMEIP "Enable SOME/IP transport targets" ON)
option(BCZL_ENABLE_LINUX_SIMULATION "Enable Linux simulation targets" ON)

list(APPEND CMAKE_MODULE_PATH
    "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

include(GNUInstallDirs)

add_library(bczl_project_options INTERFACE)
target_compile_features(bczl_project_options INTERFACE cxx_std_17)

target_include_directories(
    bczl_project_options
    INTERFACE
        "${CMAKE_CURRENT_SOURCE_DIR}/include")

if (CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(
        bczl_project_options
        INTERFACE
            -Wall
            -Wextra
            -Wpedantic
            -Wconversion
            -Wsign-conversion)
elseif (CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
    target_compile_options(
        bczl_project_options
        INTERFACE
            /W4
            /permissive-)
endif()

add_subdirectory(src)

if (BCZL_BUILD_APPS)
    add_subdirectory(app)
endif()

if (BCZL_BUILD_TESTS AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/test/CMakeLists.txt")
    enable_testing()
    add_subdirectory(test)
endif()