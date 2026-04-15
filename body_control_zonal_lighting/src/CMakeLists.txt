set(BODY_CONTROL_LIGHTING_CORE_SOURCES
    application/command_arbitrator.cpp
    application/lamp_state_manager.cpp
    application/node_health_monitor.cpp
    application/central_zone_controller.cpp
    application/rear_lighting_function_manager.cpp

    domain/lighting_payload_codec.cpp

    hmi/hmi_command_mapper.cpp
    hmi/hmi_view_model.cpp
    hmi/main_window.cpp

    platform/linux/linux_clock.cpp
    platform/linux/linux_logger.cpp
    platform/linux/process_signal_handler.cpp

    platform/stm32/ethernet_link_supervisor.cpp
    platform/stm32/gpio_output_driver.cpp
    platform/stm32/stm32_diagnostic_logger.cpp

    service/rear_lighting_service_consumer.cpp
    service/rear_lighting_service_provider.cpp

    transport/ethernet/ethernet_frame_adapter.cpp
    transport/ethernet/udp_transport_adapter.cpp
    transport/someip_message_builder.cpp
    transport/someip_message_parser.cpp
    transport/vsomeip/vsomeip_client_adapter.cpp
    transport/vsomeip/vsomeip_runtime_manager.cpp
    transport/vsomeip/vsomeip_server_adapter.cpp
)

add_library(body_control_lighting_core STATIC
    ${BODY_CONTROL_LIGHTING_CORE_SOURCES}
)

add_library(body_control_lighting::core ALIAS body_control_lighting_core)

target_include_directories(body_control_lighting_core
    PUBLIC
        ${PROJECT_SOURCE_DIR}/include
)

target_link_libraries(body_control_lighting_core
    PUBLIC
        body_control_lighting_project_options
)

target_compile_definitions(body_control_lighting_core
    PUBLIC
        BODY_CONTROL_LIGHTING_PROJECT_NAME="${PROJECT_NAME}"
        BODY_CONTROL_LIGHTING_PROJECT_VERSION="${PROJECT_VERSION}"
)