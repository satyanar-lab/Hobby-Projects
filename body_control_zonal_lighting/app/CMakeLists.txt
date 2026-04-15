add_executable(diagnostic_console
    diagnostic_console/main.cpp
)

target_link_libraries(diagnostic_console
    PRIVATE
        body_control_lighting_core
)

target_include_directories(diagnostic_console
    PRIVATE
        ${PROJECT_SOURCE_DIR}/include
)

add_executable(hmi_control_panel
    hmi_control_panel/main.cpp
)

target_link_libraries(hmi_control_panel
    PRIVATE
        body_control_lighting_core
)

target_include_directories(hmi_control_panel
    PRIVATE
        ${PROJECT_SOURCE_DIR}/include
)

add_executable(central_zone_controller_app
    central_zone_controller/main.cpp
)

target_link_libraries(central_zone_controller_app
    PRIVATE
        body_control_lighting_core
)

target_include_directories(central_zone_controller_app
    PRIVATE
        ${PROJECT_SOURCE_DIR}/include
)

add_executable(rear_lighting_node_simulator
    rear_lighting_node_simulator/main.cpp
)

target_link_libraries(rear_lighting_node_simulator
    PRIVATE
        body_control_lighting_core
)

target_include_directories(rear_lighting_node_simulator
    PRIVATE
        ${PROJECT_SOURCE_DIR}/include
)