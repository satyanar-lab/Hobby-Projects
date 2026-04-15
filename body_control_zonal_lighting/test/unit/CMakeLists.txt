set(BCZL_UNIT_TEST_SOURCES
)

if (BCZL_UNIT_TEST_SOURCES)
    add_executable(bczl_unit_tests
        ${BCZL_UNIT_TEST_SOURCES})

    target_link_libraries(
        bczl_unit_tests
        PRIVATE
            bczl::core)

    add_test(
        NAME bczl_unit_tests
        COMMAND bczl_unit_tests)
endif()