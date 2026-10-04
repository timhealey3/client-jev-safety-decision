cmake_minimum_required(VERSION 4.1)

project(server)

set(CMAKE_CXX_STANDARD 20)
list(APPEND CMAKE_PREFIX_PATH "/opt/homebrew")

find_package(CURL REQUIRED)
find_package(nlohmann_json REQUIRED)

add_executable(server main.cpp)

# 👇 ADD THIS SECTION RIGHT HERE
target_include_directories(
        server
        PRIVATE
        ${CURL_INCLUDE_DIRS}
        ${nlohmann_json_INCLUDE_DIRS}
)

target_link_libraries(
        server
        PRIVATE
        CURL::libcurl
        nlohmann_json::nlohmann_json
)