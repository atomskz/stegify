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

# stb publishes no tagged releases, so pin an exact commit for reproducible
# builds. Pinned 2026-07-19; bump deliberately after verifying the new revision.
# (GIT_SHALLOW is intentionally omitted: a shallow clone cannot resolve an
# arbitrary commit hash, only a branch or tag.)
FetchContent_Declare(
  stb
  GIT_REPOSITORY https://github.com/nothings/stb.git
  GIT_TAG 31c1ad37456438565541f4919958214b6e762fb4
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

enable_testing()

add_executable(test_roundtrip tests/test_roundtrip.c)
target_link_libraries(test_roundtrip PRIVATE stegify_core)
target_compile_options(test_roundtrip PRIVATE ${STEGIFY_WARNING_FLAGS})
target_compile_definitions(test_roundtrip PRIVATE ${STEGIFY_MSVC_DEFS})

foreach(case
    roundtrip_header
    roundtrip_noheader
    capacity_boundary
    capacity_underflow
    no_payload_detected
    zero_payload
    tiny_image_embed
    tiny_image_extract
    invalid_input
    load_errors
    encoder_from_output_path
    png_file
    bmp_file)
  add_test(NAME ${case} COMMAND test_roundtrip ${case})
endforeach()

add_test(
  NAME cli_roundtrip
  COMMAND ${CMAKE_COMMAND}
    -DSTEGIFY_EXE=$<TARGET_FILE:stegify>
    -DFIXTURE=${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/cover.png
    -DWORKDIR=${CMAKE_CURRENT_BINARY_DIR}
    -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/cli_roundtrip.cmake)

add_test(NAME cli_help COMMAND stegify --help)
add_test(NAME cli_version COMMAND stegify --version)
