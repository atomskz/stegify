# stegify command-line frontend: a thin adapter over the core library.

add_executable(stegify_cli src/main.c)
set_target_properties(stegify_cli PROPERTIES
  OUTPUT_NAME stegify
  RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})
target_link_libraries(stegify_cli PRIVATE stegify_core)
stegify_configure_target(stegify_cli)

install(TARGETS stegify_cli RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})

# --- Tests ---
add_test(
  NAME cli_roundtrip
  COMMAND ${CMAKE_COMMAND}
    -DSTEGIFY_EXE=$<TARGET_FILE:stegify_cli>
    -DFIXTURE=${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/cover.png
    -DWORKDIR=${CMAKE_CURRENT_BINARY_DIR}
    -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/cli_roundtrip.cmake)

add_test(NAME cli_help COMMAND stegify_cli --help)
add_test(NAME cli_version COMMAND stegify_cli --version)
