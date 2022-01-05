cmake_minimum_required(VERSION 3.21)
project(Web_Server C)

set(CMAKE_C_STANDARD 11)

include_directories(.)

add_executable(Web_Server
        webserver.c webserver.c rio_package.c rio_package.h)
