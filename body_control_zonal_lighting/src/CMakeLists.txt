add_library(body_control_lighting_core
    application/central_zone_controller.cpp
    application/rear_lighting_function_manager.cpp
    hmi/hmi_command_mapper.cpp
    hmi/hmi_view_model.cpp
    hmi/main_window.cpp
    service/rear_lighting_service_consumer.cpp
    service/rear_lighting_service_provider.cpp
    transport/someip_message_builder.cpp
    transport/someip_message_parser.cpp
)

target_include_directories(body_control_lighting_core
    PUBLIC
        ${PROJECT_SOURCE_DIR}/include
)

target_compile_features(body_control_lighting_core
    PUBLIC
        cxx_std_17
)

target_compile_options(body_control_lighting_core
    PRIVATE
        -Wall
        -Wextra
        -Wpedantic
)

set_target_properties(body_control_lighting_core
    PROPERTIES
        OUTPUT_NAME body_control_lighting_core
)