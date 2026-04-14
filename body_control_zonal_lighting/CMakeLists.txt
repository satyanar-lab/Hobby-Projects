cmake_minimum_required(VERSION 3.10)
project(SDV_Project)

set(CMAKE_CXX_STANDARD 17)

find_package(vsomeip3 REQUIRED)
find_package(Boost REQUIRED COMPONENTS system thread log)

include_directories(${VSOMEIP_INCLUDE_DIRS})


add_executable(service_app src/server.cpp)
target_link_libraries(service_app vsomeip3 ${Boost_LIBRARIES})


add_executable(client_app src/client.cpp)
target_link_libraries(client_app vsomeip3 ${Boost_LIBRARIES})