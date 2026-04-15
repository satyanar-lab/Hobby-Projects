cmake_minimum_required(VERSION 3.16)

project(body_control_zonal_lighting
    VERSION 1.0.0
    DESCRIPTION "Body Control Zonal Lighting over Ethernet using modern C++"
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(BODY_CONTROL_LIGHTING_BUILD_TESTS "Build body control lighting tests" ON)

include_directories(${PROJECT_SOURCE_DIR}/include)

add_subdirectory(src)
add_subdirectory(app)

if(BODY_CONTROL_LIGHTING_BUILD_TESTS)
    add_subdirectory(test)
endif()