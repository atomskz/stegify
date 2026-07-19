cmake_minimum_required(VERSION 3.14)
project(stegify C)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Debug CACHE STRING "" FORCE)
endif()

include(FetchContent)

FetchContent_Declare(
  stb
  GIT_REPOSITORY https://github.com/nothings/stb.git
  GIT_TAG master
)

FetchContent_MakeAvailable(stb)

add_library(stegify_core STATIC src/stegify.c src/stbi_impl.c)
target_include_directories(stegify_core PUBLIC src)
target_include_directories(stegify_core SYSTEM PUBLIC ${stb_SOURCE_DIR})

add_executable(stegify src/main.c)
target_link_libraries(stegify PRIVATE stegify_core)
