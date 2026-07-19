# Drives a full CLI embed -> extract round-trip and checks the recovered
# payload byte-for-byte. Invoked by CTest with:
#   -DSTEGIFY_EXE=<path to stegify>
#   -DFIXTURE=<path to a cover image>
#   -DWORKDIR=<writable directory>

set(secret "${WORKDIR}/cli_secret.bin")
set(stego "${WORKDIR}/cli_stego.png")
set(recovered "${WORKDIR}/cli_recovered.bin")

file(WRITE "${secret}" "cli round-trip payload 0123456789")

execute_process(
  COMMAND "${STEGIFY_EXE}" embed "${FIXTURE}" -f "${secret}" -o "${stego}"
  RESULT_VARIABLE embed_result)
if(NOT embed_result EQUAL 0)
  message(FATAL_ERROR "embed failed (exit ${embed_result})")
endif()

execute_process(
  COMMAND "${STEGIFY_EXE}" extract "${stego}" -o "${recovered}"
  RESULT_VARIABLE extract_result)
if(NOT extract_result EQUAL 0)
  message(FATAL_ERROR "extract failed (exit ${extract_result})")
endif()

execute_process(
  COMMAND "${CMAKE_COMMAND}" -E compare_files "${secret}" "${recovered}"
  RESULT_VARIABLE compare_result)
if(NOT compare_result EQUAL 0)
  message(FATAL_ERROR "recovered payload does not match the original")
endif()

message(STATUS "CLI round-trip OK")
