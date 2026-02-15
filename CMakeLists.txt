cmake_minimum_required(VERSION 3.10)
project(stegify C)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Debug CACHE STRING "" FORCE)
endif()


add_executable(stegify src/main.c)

# Link libraries
target_link_libraries(stegify)
