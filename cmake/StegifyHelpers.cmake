# Shared build settings and a helper that applies them to first-party targets.
# Included once at the top level; the variables and function are then visible to
# every component subdirectory.

option(STEGIFY_WERROR      "Treat first-party warnings as errors" OFF)
option(STEGIFY_SANITIZE    "Build first-party targets with sanitizers" OFF)
option(STEGIFY_GC_SECTIONS "Drop unreferenced code/data at link time" ON)

set(STEGIFY_WARNING_FLAGS
  $<$<C_COMPILER_ID:GNU,Clang,AppleClang>:-Wall;-Wextra>
  $<$<C_COMPILER_ID:MSVC>:/W4>
  $<$<AND:$<BOOL:${STEGIFY_WERROR}>,$<C_COMPILER_ID:GNU,Clang,AppleClang>>:-Werror>
  $<$<AND:$<BOOL:${STEGIFY_WERROR}>,$<C_COMPILER_ID:MSVC>>:/WX>)

# Use the portable C99 fopen without MSVC's non-standard "unsafe" deprecation.
set(STEGIFY_MSVC_DEFS $<$<C_COMPILER_ID:MSVC>:_CRT_SECURE_NO_WARNINGS>)

# Sanitizer flags, empty unless enabled. Applied per target (via the helper
# below) so third-party code such as stb stays uninstrumented.
set(STEGIFY_SANITIZE_COMPILE "")
set(STEGIFY_SANITIZE_LINK "")
if(STEGIFY_SANITIZE)
  if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    set(STEGIFY_SANITIZE_COMPILE -fsanitize=address,undefined -fno-sanitize-recover=all)
    set(STEGIFY_SANITIZE_LINK -fsanitize=address,undefined)
  elseif(MSVC)
    set(STEGIFY_SANITIZE_COMPILE /fsanitize=address)
  endif()
endif()

# Dead-code elimination: emit each function/datum into its own section at
# compile time, then let the linker discard the sections nothing references.
# This drops, for example, stb's unused TGA/HDR/JPEG encoders from the final
# binaries. The compile flags must reach the third-party stb target too (see
# core/CMakeLists.txt), or its functions cannot be stripped individually. MSVC
# already does this in Release; the flags extend it to Debug, which forces
# non-incremental linking.
set(STEGIFY_GC_COMPILE "")
set(STEGIFY_GC_LINK "")
if(STEGIFY_GC_SECTIONS)
  set(STEGIFY_GC_COMPILE
    $<$<C_COMPILER_ID:GNU,Clang,AppleClang>:-ffunction-sections;-fdata-sections>
    $<$<C_COMPILER_ID:MSVC>:/Gy;/Gw>)
  set(STEGIFY_GC_LINK
    $<$<C_COMPILER_ID:GNU,Clang>:-Wl,--gc-sections>
    $<$<C_COMPILER_ID:AppleClang>:-Wl,-dead_strip>
    $<$<C_COMPILER_ID:MSVC>:/INCREMENTAL:NO;/OPT:REF>)
endif()

# Apply the first-party warning flags, definitions, and sanitizer options to a
# target. Third-party targets should NOT be passed to this function.
function(stegify_configure_target target)
  target_compile_options(${target} PRIVATE ${STEGIFY_WARNING_FLAGS} ${STEGIFY_SANITIZE_COMPILE} ${STEGIFY_GC_COMPILE})
  target_compile_definitions(${target} PRIVATE ${STEGIFY_MSVC_DEFS})
  target_link_options(${target} PRIVATE ${STEGIFY_GC_LINK})
  if(STEGIFY_SANITIZE_LINK)
    target_link_options(${target} PRIVATE ${STEGIFY_SANITIZE_LINK})
  endif()
endfunction()
