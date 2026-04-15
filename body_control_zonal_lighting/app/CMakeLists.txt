if (NOT BCZL_BUILD_APPS)
    return()
endif()

add_executable(bczl_central_zone_controller
    central_zone_controller/main.cpp)

target_link_libraries(
    bczl_central_zone_controller
    PRIVATE
        bczl::core)

add_executable(bczl_diagnostic_console
    diagnostic_console/main.cpp)

target_link_libraries(
    bczl_diagnostic_console
    PRIVATE
        bczl::core)

add_executable(bczl_hmi_control_panel
    hmi_control_panel/main.cpp)

target_link_libraries(
    bczl_hmi_control_panel
    PRIVATE
        bczl::core)

add_executable(bczl_rear_lighting_node_simulator
    rear_lighting_node_simulator/main.cpp)

target_link_libraries(
    bczl_rear_lighting_node_simulator
    PRIVATE
        bczl::core)