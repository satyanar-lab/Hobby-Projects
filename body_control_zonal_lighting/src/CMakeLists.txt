file(GLOB_RECURSE BCZL_CORE_SOURCES
    CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp")

list(FILTER BCZL_CORE_SOURCES EXCLUDE REGEX ".*/transport/ethernet/.*\\.cpp$")

if (NOT BCZL_CORE_SOURCES)
    message(FATAL_ERROR "No source files found under src/")
endif()

add_library(bczl_core STATIC
    ${BCZL_CORE_SOURCES})

add_library(bczl::core ALIAS bczl_core)

target_include_directories(
    bczl_core
    PUBLIC
        "${PROJECT_SOURCE_DIR}/include")

target_link_libraries(
    bczl_core
    PUBLIC
        bczl_project_options)

target_compile_definitions(
    bczl_core
    PUBLIC
        BCZL_PROJECT_NAME="${PROJECT_NAME}"
        BCZL_PROJECT_VERSION="${PROJECT_VERSION}")

set_target_properties(
    bczl_core
    PROPERTIES
        OUTPUT_NAME "bczl_core")