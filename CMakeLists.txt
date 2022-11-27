cmake_minimum_required(VERSION 2.8)
project(CN_lab7_socket)

set(CMAKE_CXX_STANDARD 14)

add_executable(CN_lab7_socket_server server.cpp defs.h)

add_executable(CN_lab7_socket_client client.cpp defs.h)

target_link_libraries(CN_lab7_socket_server ws2_32)
target_link_libraries(CN_lab7_socket_client ws2_32)
