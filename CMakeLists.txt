cmake_minimum_required(VERSION 3.14)
project(stegify C)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Debug CACHE STRING "" FORCE)
endif()

# Warning flags applied to first-party targets only. The third-party stb
# implementation is compiled as a separate target (below) without these flags.
option(STEGIFY_WERROR "Treat first-party warnings as errors" OFF)

set(STEGIFY_WARNING_FLAGS
  $<$<C_COMPILER_ID:GNU,Clang,AppleClang>:-Wall;-Wextra>
  $<$<C_COMPILER_ID:MSVC>:/W4>
  $<$<AND:$<BOOL:${STEGIFY_WERROR}>,$<C_COMPILER_ID:GNU,Clang,AppleClang>>:-Werror>
  $<$<AND:$<BOOL:${STEGIFY_WERROR}>,$<C_COMPILER_ID:MSVC>>:/WX>)

include(FetchContent)

FetchContent_Declare(
  stb
  GIT_REPOSITORY https://github.com/nothings/stb.git
  GIT_TAG master
)

FetchContent_MakeAvailable(stb)

# Third-party stb image decode/encode implementation. Its include directory is
# exported as SYSTEM so warnings from stb headers do not reach first-party code.
add_library(stb_image STATIC src/stbi_impl.c)
target_include_directories(stb_image SYSTEM PUBLIC ${stb_SOURCE_DIR})

# Use the portable C99 fopen without MSVC's non-standard "unsafe" deprecation.
set(STEGIFY_MSVC_DEFS $<$<C_COMPILER_ID:MSVC>:_CRT_SECURE_NO_WARNINGS>)

add_library(stegify_core STATIC src/stegify.c)
target_include_directories(stegify_core PUBLIC src)
target_link_libraries(stegify_core PUBLIC stb_image)
target_compile_options(stegify_core PRIVATE ${STEGIFY_WARNING_FLAGS})
target_compile_definitions(stegify_core PRIVATE ${STEGIFY_MSVC_DEFS})

add_executable(stegify src/main.c)
target_link_libraries(stegify PRIVATE stegify_core)
target_compile_options(stegify PRIVATE ${STEGIFY_WARNING_FLAGS})
target_compile_definitions(stegify PRIVATE ${STEGIFY_MSVC_DEFS})
