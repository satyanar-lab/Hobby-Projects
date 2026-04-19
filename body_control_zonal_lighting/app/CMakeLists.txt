# ============================================================================
# Application executables.
#
# Linux builds: four simulation/HMI apps wired to vsomeip transport.
# STM32 build : one bare-metal rear-lighting-node ELF.
# ============================================================================

if(BODY_CONTROL_LIGHTING_TARGET_PLATFORM STREQUAL "linux")

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

elseif(BODY_CONTROL_LIGHTING_TARGET_PLATFORM STREQUAL "stm32")

    add_executable(body_control_stm32
        stm32_nucleo_h753zi/main.cpp
    )

    target_link_libraries(body_control_stm32
        PRIVATE
            body_control_lighting::core
            body_control_lighting::project_options
            stm32h7xx_hal
            stm32h753zi_startup
    )

    # Post-build: produce a .bin and a .hex suitable for flashing.
    add_custom_command(TARGET body_control_stm32 POST_BUILD
        COMMAND ${CMAKE_OBJCOPY} -O binary
                $<TARGET_FILE:body_control_stm32>
                ${CMAKE_BINARY_DIR}/body_control_stm32.bin
        COMMAND ${CMAKE_OBJCOPY} -O ihex
                $<TARGET_FILE:body_control_stm32>
                ${CMAKE_BINARY_DIR}/body_control_stm32.hex
        COMMAND ${CMAKE_SIZE} $<TARGET_FILE:body_control_stm32>
        COMMENT "Generating .bin / .hex and reporting size"
    )

endif()
