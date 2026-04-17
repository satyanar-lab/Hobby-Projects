# ============================================================================
# Core library for the body control zonal lighting project.
#
# The core library bundles application, domain, HMI, service, and transport
# sources. Platform-specific sources are selected via
# BODY_CONTROL_LIGHTING_TARGET_PLATFORM so the same library target works for
# Linux simulation and for the STM32 NUCLEO-H753ZI target.
# ============================================================================

set(BODY_CONTROL_LIGHTING_CORE_SOURCES_COMMON
    application/command_arbitrator.cpp
    application/lamp_state_manager.cpp
    application/node_health_monitor.cpp
    application/central_zone_controller.cpp
    application/rear_lighting_function_manager.cpp

    domain/domain_type_validators.cpp
    domain/lighting_payload_codec.cpp

    hmi/hmi_command_mapper.cpp
    hmi/hmi_display_strings.cpp
    hmi/hmi_view_model.cpp
    hmi/main_window.cpp

    service/operator_service_provider.cpp
    service/rear_lighting_service_consumer.cpp
    service/rear_lighting_service_provider.cpp

    transport/someip_message_builder.cpp
    transport/someip_message_parser.cpp
)

# ----------------------------------------------------------------------------
# Platform-specific source selection
#
# Linux:
#   - POSIX clock / logger / signal handler
#   - Berkeley-sockets UDP transport backend + ethernet framing
#   - vsomeip facade (currently delegates to the UDP backend; upgraded later)
#
# STM32:
#   - GPIO-backed output driver, link supervisor, UART-style diagnostic logger
#   - No POSIX sockets; LwIP-based transport backend to be wired in a later
#     phase (kept out of the translation-unit list for now to avoid pretending
#     the feature is ready).
# ----------------------------------------------------------------------------
set(BODY_CONTROL_LIGHTING_CORE_SOURCES_LINUX
    platform/linux/linux_clock.cpp
    platform/linux/linux_logger.cpp
    platform/linux/process_signal_handler.cpp

    transport/ethernet/ethernet_frame_adapter.cpp
    transport/ethernet/udp_transport_adapter.cpp

    transport/vsomeip/vsomeip_client_adapter.cpp
    transport/vsomeip/vsomeip_operator_client_adapter.cpp
    transport/vsomeip/vsomeip_operator_server_adapter.cpp
    transport/vsomeip/vsomeip_runtime_manager.cpp
    transport/vsomeip/vsomeip_server_adapter.cpp
)

set(BODY_CONTROL_LIGHTING_CORE_SOURCES_STM32
    platform/stm32/ethernet_link_supervisor.cpp
    platform/stm32/gpio_output_driver.cpp
    platform/stm32/stm32_diagnostic_logger.cpp
)

if(BODY_CONTROL_LIGHTING_TARGET_PLATFORM STREQUAL "linux")
    set(BODY_CONTROL_LIGHTING_CORE_SOURCES
        ${BODY_CONTROL_LIGHTING_CORE_SOURCES_COMMON}
        ${BODY_CONTROL_LIGHTING_CORE_SOURCES_LINUX}
    )
elseif(BODY_CONTROL_LIGHTING_TARGET_PLATFORM STREQUAL "stm32")
    set(BODY_CONTROL_LIGHTING_CORE_SOURCES
        ${BODY_CONTROL_LIGHTING_CORE_SOURCES_COMMON}
        ${BODY_CONTROL_LIGHTING_CORE_SOURCES_STM32}
    )
else()
    message(FATAL_ERROR
        "Unknown BODY_CONTROL_LIGHTING_TARGET_PLATFORM: "
        "${BODY_CONTROL_LIGHTING_TARGET_PLATFORM} "
        "(expected 'linux' or 'stm32')")
endif()

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
        body_control_lighting::project_options
)

target_compile_definitions(body_control_lighting_core
    PUBLIC
        BODY_CONTROL_LIGHTING_PROJECT_NAME="${PROJECT_NAME}"
        BODY_CONTROL_LIGHTING_PROJECT_VERSION="${PROJECT_VERSION}"
)

# Linux platform-specific link dependencies (pthread for the UDP receiver thread).
if(BODY_CONTROL_LIGHTING_TARGET_PLATFORM STREQUAL "linux")
    find_package(Threads REQUIRED)
    target_link_libraries(body_control_lighting_core
        PUBLIC
            Threads::Threads
    )
endif()
