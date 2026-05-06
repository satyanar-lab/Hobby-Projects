# src/vss/CMakeLists.txt — VssLampOverlay static library
#
# Only reachable when BCL_VSS_ENABLED is ON (set by vss/CMakeLists.txt and
# gated in src/CMakeLists.txt). The library depends on body_control_lighting::core
# for domain types and body_control_lighting::vss_paths for the generated
# bcl_vss_paths.hpp include path and its build-order dependency.

add_library(body_control_lighting_vss_overlay STATIC
    vss_lamp_overlay.cpp
)
add_library(body_control_lighting::vss_overlay
    ALIAS body_control_lighting_vss_overlay)

# Expose src/vss/ as a PUBLIC include root so consumers can include
# "vss_lamp_overlay.hpp" after linking this target.
target_include_directories(body_control_lighting_vss_overlay
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(body_control_lighting_vss_overlay
    PUBLIC
        body_control_lighting::core
        body_control_lighting::vss_paths
        body_control_lighting::project_options
)
