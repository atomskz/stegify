# Shared build settings and a helper that applies them to first-party targets.
# Included once at the top level; the variables and function are then visible to
# every component subdirectory.

option(STEGIFY_WERROR   "Treat first-party warnings as errors" OFF)
option(STEGIFY_SANITIZE "Build first-party targets with sanitizers" OFF)

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

# Apply the first-party warning flags, definitions, and sanitizer options to a
# target. Third-party targets should NOT be passed to this function.
function(stegify_configure_target target)
  target_compile_options(${target} PRIVATE ${STEGIFY_WARNING_FLAGS} ${STEGIFY_SANITIZE_COMPILE})
  target_compile_definitions(${target} PRIVATE ${STEGIFY_MSVC_DEFS})
  if(STEGIFY_SANITIZE_LINK)
    target_link_options(${target} PRIVATE ${STEGIFY_SANITIZE_LINK})
  endif()
endfunction()
