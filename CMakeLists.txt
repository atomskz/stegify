cmake_minimum_required(VERSION 3.14)
project(stegify C)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Debug CACHE STRING "" FORCE)
endif()

add_executable(stegify src/main.c src/stegify.c)

include(FetchContent)

FetchContent_Declare(
  stb
  GIT_REPOSITORY https://github.com/nothings/stb.git
  GIT_TAG master
)

FetchContent_MakeAvailable(stb)

target_sources(stegify PRIVATE src/stbi_impl.c)
target_include_directories(stegify PRIVATE ${stb_SOURCE_DIR})
target_link_libraries(stegify PRIVATE m)
