cmake_minimum_required(VERSION 3.20)

# Zephyr's find_package must come before project() — the opposite of a normal
# CMake project.  ZEPHYR_BASE is set by west before invoking cmake.
find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})

project(rear_lighting_zephyr_node
    VERSION 1.0.0
    LANGUAGES C CXX
)

# Absolute path to the shared body-control project sources.
# Using absolute path intentionally: this Zephyr app directory is a peer of
# the main CMake project, not a subdirectory, so a relative path would require
# counting ../.. from wherever west runs, which breaks if the workspace moves.
set(PROJ_ROOT
    "/home/pavankumar/workspace/Hobby-Projects/body_control_zonal_lighting")
set(DOMAIN_SRC_DIR  "${PROJ_ROOT}/src")
set(DOMAIN_INC_DIR  "${PROJ_ROOT}/include")

target_sources(app PRIVATE
    main.cpp

    # Zephyr-specific platform implementations
    ${DOMAIN_SRC_DIR}/platform/zephyr/zephyr_gpio_driver.cpp
    ${DOMAIN_SRC_DIR}/platform/zephyr/zephyr_udp_transport.cpp

    # Shared domain sources — reused unchanged from the bare-metal build.
    # Only the rear-node subset is included; CentralZoneController (std::thread)
    # and HMI sources are excluded as they were for the bare-metal target.
    ${DOMAIN_SRC_DIR}/application/rear_lighting_function_manager.cpp
    ${DOMAIN_SRC_DIR}/domain/domain_type_validators.cpp
    ${DOMAIN_SRC_DIR}/domain/lighting_payload_codec.cpp
    ${DOMAIN_SRC_DIR}/transport/someip_message_builder.cpp
    ${DOMAIN_SRC_DIR}/transport/someip_message_parser.cpp
)

target_include_directories(app PRIVATE
    ${DOMAIN_INC_DIR}
    ${DOMAIN_SRC_DIR}/platform/zephyr
)

set_property(TARGET app PROPERTY CXX_STANDARD          17)
set_property(TARGET app PROPERTY CXX_STANDARD_REQUIRED ON)
set_property(TARGET app PROPERTY CXX_EXTENSIONS        OFF)

# ── C++ STL headers ──────────────────────────────────────────────────────────
# Zephyr's cross-compile toolchain variant passes -nostdinc++ which suppresses
# the compiler's implicit search for its own C++ standard library headers.
# The arm-none-eabi Debian package installs libstdc++ headers at a predictable
# path; add them explicitly as SYSTEM includes (suppresses their warnings).
#
# We also need libstdc++.a for STL container destructors and runtime support.
# print-file-name returns the ABI-correct .a for the given CPU/FP flags;
# get_filename_component REALPATH resolves the ../ components in the result.

execute_process(
    COMMAND ${CMAKE_CXX_COMPILER} -dumpversion
    OUTPUT_VARIABLE ARMNE_CXX_VER
    OUTPUT_STRIP_TRAILING_WHITESPACE
)

execute_process(
    COMMAND ${CMAKE_CXX_COMPILER} -mcpu=cortex-m7 -mthumb
            -print-file-name=libstdc++.a
    OUTPUT_VARIABLE LIBSTDCXX_PATH_RAW
    OUTPUT_STRIP_TRAILING_WHITESPACE
)
execute_process(
    COMMAND ${CMAKE_CXX_COMPILER} -mcpu=cortex-m7 -mthumb
            -print-file-name=libsupc++.a
    OUTPUT_VARIABLE LIBSUPCXX_PATH_RAW
    OUTPUT_STRIP_TRAILING_WHITESPACE
)

# Resolve "../../../" components to get the canonical path.
get_filename_component(LIBSTDCXX_PATH "${LIBSTDCXX_PATH_RAW}" REALPATH)
get_filename_component(LIBSUPCXX_PATH "${LIBSUPCXX_PATH_RAW}" REALPATH)

# Strip from "/newlib/" onward to reach the arm-none-eabi sysroot root.
# Real path: /usr/lib/arm-none-eabi/newlib/thumb/v7e-m/nofp/libstdc++.a
#    -> base: /usr/lib/arm-none-eabi
string(REGEX REPLACE "/newlib/.*" "" ARMNE_BASE "${LIBSTDCXX_PATH}")

target_include_directories(app SYSTEM PRIVATE
    ${ARMNE_BASE}/include/c++/${ARMNE_CXX_VER}
    ${ARMNE_BASE}/include/c++/${ARMNE_CXX_VER}/arm-none-eabi
)

target_link_libraries(app PRIVATE
    ${LIBSTDCXX_PATH}
    ${LIBSUPCXX_PATH}
)
