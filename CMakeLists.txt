cmake_minimum_required(VERSION 3.14)
project(stegify C)

# C++ is used only by the header C++-linkage test in the core component.
enable_language(CXX)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Debug CACHE STRING "" FORCE)
endif()

option(STEGIFY_BUILD_CLI "Build the command-line frontend" ON)

list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/cmake)
include(StegifyHelpers)
include(GNUInstallDirs)

enable_testing()

# Components. The core library is always built; frontends are opt-in and depend
# only on the core (a future gui/ component slots in the same way).
add_subdirectory(core)

if(STEGIFY_BUILD_CLI)
  add_subdirectory(cli)
endif()
