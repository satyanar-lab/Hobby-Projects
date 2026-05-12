cmake_minimum_required(VERSION 3.14)

project(automotive_ethernet_fundamentals
	VERSION 1.0
	LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_compile_options(-Wall -Wextra)

add_executable(someip_server someip/someip_server.cpp)
add_executable(someip_client someip/someip_client.cpp)
add_executable(uds_server    uds/uds_server.cpp)
add_executable(uds_client    uds/uds_client.cpp)

# ============================================================
# Tests
# ============================================================

enable_testing()
find_package(GTest REQUIRED)

add_executable(test_byte_order tests/test_byte_order.cpp)
target_link_libraries(test_byte_order PRIVATE GTest::gtest GTest::gtest_main pthread)

add_executable(test_someip_codec tests/test_someip_codec.cpp)
target_link_libraries(test_someip_codec PRIVATE GTest::gtest GTest::gtest_main pthread)

add_executable(test_uds_codec tests/test_uds_codec.cpp)
target_link_libraries(test_uds_codec PRIVATE GTest::gtest GTest::gtest_main pthread)

include(GoogleTest)
gtest_discover_tests(test_byte_order)
gtest_discover_tests(test_someip_codec)
gtest_discover_tests(test_uds_codec)
