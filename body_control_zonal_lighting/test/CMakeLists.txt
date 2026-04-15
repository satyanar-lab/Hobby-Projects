file(GLOB_RECURSE BODY_CONTROL_LIGHTING_TEST_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/unit/*.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/integration/*.cpp
)

if(BODY_CONTROL_LIGHTING_TEST_SOURCES)
    add_library(body_control_lighting_test_objects OBJECT
        ${BODY_CONTROL_LIGHTING_TEST_SOURCES}
    )

    target_link_libraries(body_control_lighting_test_objects
        PRIVATE
            body_control_lighting_core
    )

    target_include_directories(body_control_lighting_test_objects
        PRIVATE
            ${PROJECT_SOURCE_DIR}/include
    )

    target_compile_features(body_control_lighting_test_objects
        PRIVATE
            cxx_std_17
    )

    target_compile_options(body_control_lighting_test_objects
        PRIVATE
            -Wall
            -Wextra
            -Wpedantic
    )
endif()