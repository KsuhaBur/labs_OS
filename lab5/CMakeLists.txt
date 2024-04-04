cmake_minimum_required(VERSION 3.0.0)
project(laba5 VERSION 0.1.0)
set(CMAKE_CXX_STANDARD 17)


find_package(SQLite3 REQUIRED)
find_package(Threads REQUIRED)
find_package(Boost 1.82.0 REQUIRED)

set(client
    client.cpp
)
add_executable(client ${client})
if(WIN32)
    target_link_libraries(client PRIVATE ws2_32)
endif()

target_link_libraries(client PRIVATE ${Boost_LIBRARIES} SQLite::SQLite3)


target_include_directories(client PRIVATE ${Boost_INCLUDE_DIRS})