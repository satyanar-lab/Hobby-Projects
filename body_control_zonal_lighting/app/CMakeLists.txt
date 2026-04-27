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
        hmi_control_panel_terminal
        hmi_control_panel/main.cpp
    )

    # -------------------------------------------------------------------------
    # Qt6 GUI HMI — optional, guarded by BODY_CONTROL_LIGHTING_BUILD_QT_HMI.
    # Not linked to body_control_lighting::project_options because Qt MOC-
    # generated code does not satisfy the project's strict warning set
    # (-Wshadow, -Wold-style-cast, -Werror).
    # -------------------------------------------------------------------------
    option(BODY_CONTROL_LIGHTING_BUILD_QT_HMI "Build Qt6 GUI HMI" ON)

    if(BODY_CONTROL_LIGHTING_BUILD_QT_HMI)

        find_package(Qt6 COMPONENTS Core Quick Qml REQUIRED)

        qt6_add_resources(QT_HMI_RESOURCES
            "${CMAKE_CURRENT_SOURCE_DIR}/hmi_control_panel/resources.qrc"
        )

        add_executable(hmi_control_panel_qt
            hmi_control_panel/qt_main.cpp
            hmi_control_panel/qml_hmi_bridge.cpp
            ${QT_HMI_RESOURCES}
        )

        set_target_properties(hmi_control_panel_qt PROPERTIES AUTOMOC ON)

        target_include_directories(hmi_control_panel_qt PRIVATE
            "${CMAKE_CURRENT_SOURCE_DIR}/hmi_control_panel"
        )

        target_link_libraries(hmi_control_panel_qt
            PRIVATE
                body_control_lighting::core
                Qt6::Core
                Qt6::Quick
                Qt6::Qml
        )

        # Reasonable warnings without -Werror so MOC output compiles cleanly.
        target_compile_options(hmi_control_panel_qt PRIVATE
            $<$<CXX_COMPILER_ID:GNU,Clang,AppleClang>:
                -Wall -Wextra -Wno-shadow -Wno-old-style-cast>
        )

    endif() # BODY_CONTROL_LIGHTING_BUILD_QT_HMI

    body_control_lighting_add_application(
        diagnostic_console
        diagnostic_console/main.cpp
    )

    body_control_lighting_add_application(
        rear_lighting_node_simulator
        rear_lighting_node_simulator/main.cpp
    )

elseif(BODY_CONTROL_LIGHTING_TARGET_PLATFORM STREQUAL "stm32")

    # stm32h7xx_hal_msp.cpp and stm32h7xx_it.cpp are compiled directly into the
    # executable (not into the stm32h7xx_hal static library) so their strong
    # definitions of HAL_ETH_MspInit, HAL_UART_MspInit, HardFault_Handler, and
    # SysTick_Handler unconditionally override the __weak stubs in the HAL.
    # If these files were in the archive, the linker would silently keep the
    # weak no-op stubs because no undefined reference ever forces the archive
    # member to be extracted.
    set(_MSP_DIR ${PROJECT_SOURCE_DIR}/target/stm32_nucleo_h753zi)

    add_executable(body_control_stm32
        stm32_nucleo_h753zi/main.cpp
        ${_MSP_DIR}/stm32h7xx_hal_msp.cpp
        ${_MSP_DIR}/stm32h7xx_it.cpp
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
