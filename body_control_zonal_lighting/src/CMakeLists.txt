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
    application/fault_manager.cpp
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

    service/operator_service_consumer.cpp
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

    transport/ethernet/direct_udp_transport_adapter.cpp
    transport/ethernet/ethernet_frame_adapter.cpp
    transport/ethernet/udp_transport_adapter.cpp
)

# STM32 source list is intentionally standalone — it excludes
# CentralZoneController (uses std::thread) and HMI/operator sources.
# The STM32 node role only needs domain, codec, service-provider, and
# the platform/transport drivers.
set(BODY_CONTROL_LIGHTING_CORE_SOURCES_STM32
    application/command_arbitrator.cpp
    application/fault_manager.cpp
    application/lamp_state_manager.cpp
    application/rear_lighting_function_manager.cpp

    domain/domain_type_validators.cpp
    domain/lighting_payload_codec.cpp

    service/rear_lighting_service_provider.cpp

    transport/someip_message_builder.cpp
    transport/someip_message_parser.cpp
    transport/lwip/lwip_udp_transport_adapter.cpp

    platform/stm32/ethernet_link_supervisor.cpp
    platform/stm32/ethernetif.cpp
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

# STM32 platform-specific dependencies.
if(BODY_CONTROL_LIGHTING_TARGET_PLATFORM STREQUAL "stm32")
    set(lwip_SOURCE_DIR "${STM32CUBEH7_DIR}/Middlewares/Third_Party/LwIP")

    # Build only the LwIP modules needed for bare-metal UDP with ARP.
    add_library(lwip_stm32 STATIC
        ${lwip_SOURCE_DIR}/src/core/def.c
        ${lwip_SOURCE_DIR}/src/core/inet_chksum.c
        ${lwip_SOURCE_DIR}/src/core/init.c
        ${lwip_SOURCE_DIR}/src/core/ip.c
        ${lwip_SOURCE_DIR}/src/core/mem.c
        ${lwip_SOURCE_DIR}/src/core/memp.c
        ${lwip_SOURCE_DIR}/src/core/netif.c
        ${lwip_SOURCE_DIR}/src/core/pbuf.c
        ${lwip_SOURCE_DIR}/src/core/stats.c
        ${lwip_SOURCE_DIR}/src/core/sys.c
        ${lwip_SOURCE_DIR}/src/core/timeouts.c
        ${lwip_SOURCE_DIR}/src/core/udp.c
        ${lwip_SOURCE_DIR}/src/core/ipv4/etharp.c
        ${lwip_SOURCE_DIR}/src/core/ipv4/icmp.c
        ${lwip_SOURCE_DIR}/src/core/ipv4/ip4.c
        ${lwip_SOURCE_DIR}/src/core/ipv4/ip4_addr.c
        ${lwip_SOURCE_DIR}/src/core/ipv4/ip4_frag.c
        ${lwip_SOURCE_DIR}/src/netif/ethernet.c
    )

    target_include_directories(lwip_stm32
        PUBLIC
            ${lwip_SOURCE_DIR}/src/include
            ${PROJECT_SOURCE_DIR}/target/stm32_nucleo_h753zi   # lwipopts.h
    )

    # Suppress warnings from LwIP source (external dependency).
    target_compile_options(lwip_stm32 PRIVATE -w)

    target_link_libraries(body_control_lighting_core
        PUBLIC
            lwip_stm32
            stm32h7xx_hal
    )
endif()

# Linux platform-specific link dependencies.
if(BODY_CONTROL_LIGHTING_TARGET_PLATFORM STREQUAL "linux")
    find_package(Threads REQUIRED)
    target_link_libraries(body_control_lighting_core
        PUBLIC
            Threads::Threads
    )

    # ----------------------------------------------------------------------------
    # vsomeip3 transport — compiled as its own static library so that the
    # vsomeip3 shared-library dependency is scoped to this target only.
    # ----------------------------------------------------------------------------
    list(APPEND CMAKE_PREFIX_PATH "/usr/local")
    find_package(vsomeip3 REQUIRED)

    # Suppress warnings from vsomeip3 headers (external dependency).
    get_target_property(_vsomeip3_inc vsomeip3 INTERFACE_INCLUDE_DIRECTORIES)
    set_target_properties(vsomeip3 PROPERTIES
        INTERFACE_SYSTEM_INCLUDE_DIRECTORIES "${_vsomeip3_inc}"
    )

    add_library(body_control_lighting_vsomeip_transport STATIC
        transport/vsomeip/vsomeip_client_adapter.cpp
        transport/vsomeip/vsomeip_operator_client_adapter.cpp
        transport/vsomeip/vsomeip_operator_server_adapter.cpp
        transport/vsomeip/vsomeip_runtime_manager.cpp
        transport/vsomeip/vsomeip_server_adapter.cpp
    )
    add_library(body_control_lighting::vsomeip_transport
        ALIAS body_control_lighting_vsomeip_transport)

    target_include_directories(body_control_lighting_vsomeip_transport
        PUBLIC
            ${PROJECT_SOURCE_DIR}/include
    )

    target_link_libraries(body_control_lighting_vsomeip_transport
        PUBLIC
            body_control_lighting::project_options
            vsomeip3
    )

    target_link_libraries(body_control_lighting_core
        PUBLIC
            body_control_lighting_vsomeip_transport
    )
endif()
