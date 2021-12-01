cmake_minimum_required(VERSION 3.20)
project(cwork C)

link_libraries(-lws2_32)
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -pthread")
include_directories(.)

add_executable(cwork main.c)
