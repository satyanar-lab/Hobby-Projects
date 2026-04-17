# ============================================================================
# Test build.
#
# Tests are built against body_control_lighting::core using GoogleTest, which
# is acquired via FetchContent so the project does not require a system
# install. Each test source file becomes its own executable so CTest can
# identify failing cases granularly.
# ============================================================================

include(FetchContent)

# Pin a specific GoogleTest release for reproducible builds.
FetchContent_Declare(
    googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG        v1.14.0
)

# Force GoogleTest's own runtime library selection to match ours on Windows.
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)

FetchContent_MakeAvailable(googletest)

# ----------------------------------------------------------------------------
# Helper: register one test source file as its own executable + CTest entry.
# ----------------------------------------------------------------------------
function(body_control_lighting_add_test test_source_file)
    get_filename_component(test_target_name "${test_source_file}" NAME_WE)

    add_executable(${test_target_name} "${test_source_file}")

    target_link_libraries(${test_target_name}
        PRIVATE
            body_control_lighting::core
            body_control_lighting::project_options
            GTest::gtest
            GTest::gtest_main
    )

    add_test(
        NAME ${test_target_name}
        COMMAND ${test_target_name}
    )
endfunction()

add_subdirectory(unit)
add_subdirectory(integration)
