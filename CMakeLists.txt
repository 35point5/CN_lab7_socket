cmake_minimum_required(VERSION 2.8)
project(CN_lab7_socket)

set(CMAKE_CXX_STANDARD 14)

add_executable(CN_lab7_socket server.cpp defs.h)

target_link_libraries(CN_lab7_socket ws2_32)
