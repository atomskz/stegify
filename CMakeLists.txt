cmake_minimum_required(VERSION 3.10)
project(stegify C)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

add_executable(stegify src/main.c)
