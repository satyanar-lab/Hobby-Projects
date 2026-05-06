# vss/CMakeLists.txt — VSS path-constant generation
#
# Controlled by BODY_CONTROL_LIGHTING_BUILD_VSS (AUTO/ON/OFF):
#   AUTO — probes for the vspec CLI; degrades gracefully if absent
#   ON   — FATAL_ERROR if vspec or Python3 are missing
#   OFF  — skipped entirely

# Internal flag resolved from the tri-state.
set(_BCL_VSS_ENABLED OFF)

if(BODY_CONTROL_LIGHTING_BUILD_VSS STREQUAL "OFF")
    message(STATUS "[VSS] Skipped (BODY_CONTROL_LIGHTING_BUILD_VSS=OFF)")
else()
    # Probe 1: vspec CLI (installed by pip install vss-tools into PATH or venv).
    # find_program respects PATH at cmake-configure time, so activating a venv
    # before running cmake is the supported way to point at a specific install.
    find_program(_BCL_VSPEC_PROG NAMES vspec)

    if(NOT _BCL_VSPEC_PROG)
        if(BODY_CONTROL_LIGHTING_BUILD_VSS STREQUAL "ON")
            message(FATAL_ERROR
                "[VSS] 'vspec' executable not found. "
                "Install vss-tools and ensure it is on PATH: "
                "pip install -r vss/requirements.txt. "
                "BODY_CONTROL_LIGHTING_BUILD_VSS=ON requires it.")
        else()
            message(STATUS
                "[VSS] 'vspec' not found — VSS generation disabled "
                "(install with: pip install -r vss/requirements.txt)")
        endif()
    else()
        # Probe 2: Python3 interpreter for the generator script
        find_package(Python3 QUIET COMPONENTS Interpreter)
        if(NOT Python3_FOUND)
            if(BODY_CONTROL_LIGHTING_BUILD_VSS STREQUAL "ON")
                message(FATAL_ERROR
                    "[VSS] Python3 interpreter not found. "
                    "BODY_CONTROL_LIGHTING_BUILD_VSS=ON requires it.")
            else()
                message(STATUS "[VSS] Python3 not found — VSS generation disabled")
            endif()
        else()
            set(_BCL_VSS_ENABLED ON)
        endif()
    endif()
endif()

if(_BCL_VSS_ENABLED)
    set(_BCL_VSS_SPEC_DIR    "${CMAKE_CURRENT_SOURCE_DIR}/spec")
    set(_BCL_VSS_BASE_VSPEC  "${_BCL_VSS_SPEC_DIR}/bcl_lighting.vspec")
    set(_BCL_VSS_EXT_VSPEC   "${_BCL_VSS_SPEC_DIR}/bcl_extension.vspec")
    set(_BCL_VSS_GENERATOR   "${CMAKE_CURRENT_SOURCE_DIR}/generate_header.py")
    set(_BCL_VSS_MERGED_JSON "${CMAKE_BINARY_DIR}/vss/bcl_merged.json")
    set(_BCL_VSS_HPP         "${CMAKE_BINARY_DIR}/include/vss/bcl_vss_paths.hpp")

    # Step A: merge vspec files → JSON
    add_custom_command(
        OUTPUT  "${_BCL_VSS_MERGED_JSON}"
        COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_BINARY_DIR}/vss"
        COMMAND "${_BCL_VSPEC_PROG}"
                    export json
                    --vspec "${_BCL_VSS_BASE_VSPEC}"
                    --overlays "${_BCL_VSS_EXT_VSPEC}"
                    --output  "${_BCL_VSS_MERGED_JSON}"
                    --pretty
        DEPENDS "${_BCL_VSS_BASE_VSPEC}" "${_BCL_VSS_EXT_VSPEC}"
        COMMENT "[VSS] Merging vspec overlays -> bcl_merged.json"
        VERBATIM
    )

    # Step B: JSON → C++ header
    add_custom_command(
        OUTPUT  "${_BCL_VSS_HPP}"
        COMMAND "${Python3_EXECUTABLE}" "${_BCL_VSS_GENERATOR}"
                    "${_BCL_VSS_MERGED_JSON}" "${_BCL_VSS_HPP}"
        DEPENDS "${_BCL_VSS_MERGED_JSON}" "${_BCL_VSS_GENERATOR}"
        COMMENT "[VSS] Generating bcl_vss_paths.hpp"
        VERBATIM
    )

    # Custom target drives generation during every build
    add_custom_target(bcl_vss_generate ALL
        DEPENDS "${_BCL_VSS_HPP}"
    )

    # Interface library: consumers link this to get the generated include path
    # and the build dependency on generation.
    add_library(bcl_vss_paths INTERFACE)
    add_library(body_control_lighting::vss_paths ALIAS bcl_vss_paths)
    target_include_directories(bcl_vss_paths
        INTERFACE "${CMAKE_BINARY_DIR}/include"
    )
    add_dependencies(bcl_vss_paths bcl_vss_generate)

    message(STATUS "[VSS] Generation enabled")
    message(STATUS "[VSS]   vspec:  ${_BCL_VSPEC_PROG}")
    message(STATUS "[VSS]   output: ${_BCL_VSS_HPP}")
endif()

# Export flag so parent scope (e.g. diagnostic_console) can conditionalize.
set(BCL_VSS_ENABLED ${_BCL_VSS_ENABLED} PARENT_SCOPE)
