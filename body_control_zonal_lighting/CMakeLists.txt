cmake_minimum_required(VERSION 3.16)

project(body_control_zonal_lighting
    VERSION 1.0.0
    DESCRIPTION "Body Control Zonal Lighting over Ethernet using modern C++"
    LANGUAGES CXX
)

# ----------------------------------------------------------------------------
# Language level — locked to C++17 for all targets in this project.
# The toolchain used for the STM32 target is expected to enforce the same.
# ----------------------------------------------------------------------------
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# ----------------------------------------------------------------------------
# Build options
# ----------------------------------------------------------------------------
option(BODY_CONTROL_LIGHTING_BUILD_TESTS
    "Build body control lighting unit and integration tests" ON)

option(BODY_CONTROL_LIGHTING_BUILD_APPS
    "Build body control lighting executables (controller, HMI, node, diag)" ON)

option(BODY_CONTROL_LIGHTING_WARNINGS_AS_ERRORS
    "Promote all compiler warnings to errors" ON)

# ----------------------------------------------------------------------------
# Target platform detection
#
# The project supports two target platforms:
#   - Linux (default for development / laptop simulation)
#   - STM32 NUCLEO-H753ZI (cross-compiled via an external toolchain file)
#
# BODY_CONTROL_LIGHTING_TARGET_PLATFORM is inferred from CMAKE_SYSTEM_NAME
# so passing -DCMAKE_TOOLCHAIN_FILE=... for the STM32 build is enough to
# redirect platform-specific sources.
# ----------------------------------------------------------------------------
if(NOT DEFINED BODY_CONTROL_LIGHTING_TARGET_PLATFORM)
    if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
        set(BODY_CONTROL_LIGHTING_TARGET_PLATFORM "linux"
            CACHE STRING "Target platform: linux or stm32" FORCE)
    else()
        set(BODY_CONTROL_LIGHTING_TARGET_PLATFORM "stm32"
            CACHE STRING "Target platform: linux or stm32" FORCE)
    endif()
endif()

message(STATUS
    "Body Control Lighting target platform: ${BODY_CONTROL_LIGHTING_TARGET_PLATFORM}")

# The STM32 target builds HAL (C) and startup (ASM) sources.
# Enable these languages here, before any add_subdirectory, so CMake
# generates compile rules for them. The Linux build only needs CXX.
if(BODY_CONTROL_LIGHTING_TARGET_PLATFORM STREQUAL "stm32")
    enable_language(C ASM)
endif()

# ----------------------------------------------------------------------------
# Interface target: project-wide compile options
#
# Every compiled target in this project depends on this interface library.
# It is the single place where warning flags, language-level features, and
# project-wide compile definitions are expressed.
# ----------------------------------------------------------------------------
add_library(body_control_lighting_project_options INTERFACE)
add_library(body_control_lighting::project_options
    ALIAS body_control_lighting_project_options)

target_compile_features(body_control_lighting_project_options
    INTERFACE
        cxx_std_17
)

target_compile_options(body_control_lighting_project_options
    INTERFACE
        $<$<CXX_COMPILER_ID:GNU,Clang,AppleClang>:-Wall>
        $<$<CXX_COMPILER_ID:GNU,Clang,AppleClang>:-Wextra>
        $<$<CXX_COMPILER_ID:GNU,Clang,AppleClang>:-Wpedantic>
        $<$<CXX_COMPILER_ID:GNU,Clang,AppleClang>:-Wshadow>
        $<$<CXX_COMPILER_ID:GNU,Clang,AppleClang>:-Wconversion>
        $<$<CXX_COMPILER_ID:GNU,Clang,AppleClang>:-Wnon-virtual-dtor>
        $<$<CXX_COMPILER_ID:MSVC>:/W4>
)
# -Wold-style-cast is enabled on Linux where we control all headers.
# On STM32 the CMSIS/HAL peripheral macros (ETH, GPIOB, …) expand C-style
# casts that we cannot change; suppress the warning for that target only.
if(BODY_CONTROL_LIGHTING_TARGET_PLATFORM STREQUAL "linux")
    target_compile_options(body_control_lighting_project_options
        INTERFACE
            $<$<CXX_COMPILER_ID:GNU,Clang,AppleClang>:-Wold-style-cast>
    )
endif()

if(BODY_CONTROL_LIGHTING_WARNINGS_AS_ERRORS)
    target_compile_options(body_control_lighting_project_options
        INTERFACE
            $<$<CXX_COMPILER_ID:GNU,Clang,AppleClang>:-Werror>
            $<$<CXX_COMPILER_ID:MSVC>:/WX>
    )
endif()

# ----------------------------------------------------------------------------
# Sub-directories
# ----------------------------------------------------------------------------

# Board-support library must be added before src/ so that the stm32h7xx_hal
# and stm32h753zi_startup targets exist when src/CMakeLists.txt links them.
if(BODY_CONTROL_LIGHTING_TARGET_PLATFORM STREQUAL "stm32")
    add_subdirectory(target/stm32_nucleo_h753zi)
endif()

add_subdirectory(src)

if(BODY_CONTROL_LIGHTING_BUILD_APPS)
    add_subdirectory(app)
endif()

# Tests require GoogleTest which uses POSIX threading — only build on Linux.
if(BODY_CONTROL_LIGHTING_BUILD_TESTS
   AND BODY_CONTROL_LIGHTING_TARGET_PLATFORM STREQUAL "linux")
    enable_testing()
    add_subdirectory(test)
endif()
