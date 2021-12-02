cmake_minimum_required(VERSION 3.20)
project(win-fpm C)

link_libraries(-lws2_32)
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -o win-fpm -pthread")
include_directories(.)

add_executable(win-fpm main.c)
