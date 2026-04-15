set(BCZL_INTEGRATION_TEST_SOURCES
)

if (BCZL_INTEGRATION_TEST_SOURCES)
    add_executable(bczl_integration_tests
        ${BCZL_INTEGRATION_TEST_SOURCES})

    target_link_libraries(
        bczl_integration_tests
        PRIVATE
            bczl::core)

    add_test(
        NAME bczl_integration_tests
        COMMAND bczl_integration_tests)
endif()