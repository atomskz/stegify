# stegify core: the LSB steganography algorithm, image I/O, and the UI-agnostic
# file operations. Depends on nothing outside its own third-party stb.

# Third-party stb, owned by the core component. Pinned to an exact commit for
# reproducible builds (bump deliberately after verifying a new revision).
include(FetchContent)
FetchContent_Declare(
  stb
  GIT_REPOSITORY https://github.com/nothings/stb.git
  GIT_TAG 31c1ad37456438565541f4919958214b6e762fb4
)
FetchContent_MakeAvailable(stb)

add_library(stb_image STATIC src/stbi_impl.c)
target_include_directories(stb_image SYSTEM PUBLIC ${stb_SOURCE_DIR})
# No libm needed: stbi_impl.c disables stb's HDR/float loaders, the only stb
# code that calls pow().

add_library(stegify_core STATIC src/core.c src/ops.c)
target_include_directories(stegify_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(stegify_core PUBLIC stb_image)
stegify_configure_target(stegify_core)

# When the core is built with sanitizers, every executable that links it must
# also link the sanitizer runtime; propagate that as a usage requirement so no
# consumer (tests, frontends) has to remember it.
if(STEGIFY_SANITIZE_LINK)
  target_link_options(stegify_core INTERFACE ${STEGIFY_SANITIZE_LINK})
endif()

install(TARGETS stegify_core stb_image
  RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
  ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
  LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR})
install(DIRECTORY include/stegify DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})

# Compiled so the README library example is guaranteed to build.
add_executable(example_usage examples/usage.c)
target_link_libraries(example_usage PRIVATE stegify_core)
stegify_configure_target(example_usage)

# --- Tests ---
add_executable(test_roundtrip tests/test_roundtrip.c)
target_link_libraries(test_roundtrip PRIVATE stegify_core)
stegify_configure_target(test_roundtrip)
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

add_executable(test_ops tests/test_ops.c)
target_link_libraries(test_ops PRIVATE stegify_core)
stegify_configure_target(test_ops)
foreach(case
    file_io
    ops_roundtrip
    ops_roundtrip_no_header
    ops_capacity
    ops_no_payload)
  add_test(NAME ${case} COMMAND test_ops ${case})
endforeach()

# Confirm the public header is usable from C++ (extern "C" linkage).
add_executable(test_cpp_link tests/test_cpp_link.cpp)
target_link_libraries(test_cpp_link PRIVATE stegify_core)
add_test(NAME cpp_link COMMAND test_cpp_link)
