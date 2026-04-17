# ============================================================================
# Application executables.
#
# Each executable is a thin wire-up around body_control_lighting::core:
# it constructs a transport adapter and hands it to a service facade which
# in turn drives the application-layer controller or node.
# ============================================================================

function(body_control_lighting_add_application app_target_name app_source_file)
    add_executable(${app_target_name} ${app_source_file})

    target_link_libraries(${app_target_name}
        PRIVATE
            body_control_lighting::core
            body_control_lighting::project_options
    )
endfunction()

body_control_lighting_add_application(
    central_zone_controller_app
    central_zone_controller/main.cpp
)

body_control_lighting_add_application(
    hmi_control_panel
    hmi_control_panel/main.cpp
)

body_control_lighting_add_application(
    diagnostic_console
    diagnostic_console/main.cpp
)

body_control_lighting_add_application(
    rear_lighting_node_simulator
    rear_lighting_node_simulator/main.cpp
)
