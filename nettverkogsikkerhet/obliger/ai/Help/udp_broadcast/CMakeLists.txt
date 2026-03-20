cmake_minimum_required(VERSION 3.20)
project(udp_broadcast_win LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(udp_broadcast main.cpp)

# Winsock library
target_link_libraries(udp_broadcast PRIVATE ws2_32)
