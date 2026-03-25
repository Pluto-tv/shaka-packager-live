# Copyright 2022 Google LLC. All rights reserved.
#
# Use of this source code is governed by a BSD-style
# license that can be found in the LICENSE file or at
# https://developers.google.com/open-source/licenses/bsd

# Root-level CMake build file.

# Minimum CMake version.  This must be in the root level CMakeLists.txt.
cmake_minimum_required(VERSION 3.24)

# These policy settings should be included before the project definition.
include("packager/policies.cmake")

# Project name.  May not contain spaces.  Versioning is managed elsewhere.
project(shaka-packager VERSION "")

# std::filesystem (used in packager/file) requires macOS 10.15+. CMake's Apple
# toolchain init may still inject -mmacosx-version-min=10.9; bump so native
# macOS builds compile and link consistently.
if(APPLE)
  set(CMAKE_OSX_DEPLOYMENT_TARGET "10.15" CACHE STRING "Minimum macOS version" FORCE)
  foreach(_shaka_osx_flag IN ITEMS CMAKE_C_FLAGS CMAKE_CXX_FLAGS CMAKE_EXE_LINKER_FLAGS
                                    CMAKE_MODULE_LINKER_FLAGS CMAKE_SHARED_LINKER_FLAGS)
    if(DEFINED ${_shaka_osx_flag})
      string(REPLACE "-mmacosx-version-min=10.9" "-mmacosx-version-min=10.15"
             ${_shaka_osx_flag} "${${_shaka_osx_flag}}")
    endif()
  endforeach()
endif()

# Whether to build a shared libpackager library.  By default, don't.
option(BUILD_SHARED_LIBS "Build libpackager as a shared library" OFF)

# Whether to attempt a static linking of the command line front-end.
# Only supported on Linux and with musl.  This will also cause us to link
# against mimalloc to replace the standard allocator in musl, which is slow.
option(FULLY_STATIC "Attempt fully static linking of all CLI apps" OFF)

# Enable CMake's test infrastructure.
enable_testing()

option(SKIP_INTEGRATION_TESTS "Skip the packager integration tests" OFF)

# Subdirectories with their own CMakeLists.txt
add_subdirectory(packager)
add_subdirectory(link-test)
