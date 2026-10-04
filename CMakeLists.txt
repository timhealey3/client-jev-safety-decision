cmake_minimum_required(VERSION 4.1)

project(tcp_client)

set(CMAKE_CXX_STANDARD 20)

find_package(CURL REQUIRED)
find_package(nlohmann_json REQUIRED)

add_executable(tcp_client client/main.cpp)

target_link_libraries(
        tcp_client
        PRIVATE
        CURL::libcurl
        nlohmann_json::nlohmann_json
)