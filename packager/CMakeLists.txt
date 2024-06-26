# Copyright 2022 Google LLC. All rights reserved.
#
# Use of this source code is governed by a BSD-style
# license that can be found in the LICENSE file or at
# https://developers.google.com/open-source/licenses/bsd

# Packager CMake build file.

# Include a module to define standard install directories.
include(GNUInstallDirs)

# Build static libs by default, or shared if BUILD_SHARED_LIBS is on.
if(BUILD_SHARED_LIBS)
  add_definitions(-DSHARED_LIBRARY_BUILD)
  set(CMAKE_POSITION_INDEPENDENT_CODE ON)
endif()

# Global C++ flags.
if(MSVC)
  # Warning level 4 and all warnings as errors.
  add_compile_options(/W4 /WX)
  # Silence a warning from an absl header about alignment in boolean flags.
  add_compile_options(/wd4324)
  # Silence a warning about STL types in exported classes.
  add_compile_options(/wd4251)
  # Silence a warning about constant conditional expressions.
  add_compile_options(/wd4127)
  # We use long-jumps in subtitle_composer.cc due to API of libpng
  add_compile_options(/wd4611)
  # need /bigobj for box definitions
  add_compile_options(/bigobj)

  # Packager's macro for Windows-specific code.
  add_definitions(-DOS_WIN)

  # Suppress Microsoft's min() and max() macros, which will conflict with
  # things like std::numeric_limits::max() and std::min().
  add_definitions(-DNOMINMAX)
  # Define this so that we can use fopen() without warnings.
  add_definitions(-D_CRT_SECURE_NO_WARNINGS)
  # Don't automatically include winsock.h in windows.h.  This is needed for us
  # to use winsock2.h, which contains definitions that conflict with the
  # ancient winsock 1.1 interface in winsock.h.
  add_definitions(-DWIN32_LEAN_AND_MEAN)
else()
  # Lots of warnings and all warnings as errors.
  # Note that we can't use -Wpedantic due to absl's int128 headers.
  add_compile_options(-Wall -Wextra -Werror)
  # Several warning suppression flags are required on one compiler version and
  # not understood by another.  Do not treat these as errors.
  add_compile_options(-Wno-unknown-warning-option)
endif()

if(USE_WORKAROUND_FOR_TRUN_VERSION_0)
  add_definitions(-DUSE_WORKAROUND_FOR_TRUN_VERSION_0=1)
endif()

# Global include paths.
# Project root, to reference internal headers as packager/foo/bar/...
include_directories(..)
# Public include folder, to reference public headers as packager/foo.h
include_directories(../include)

# Include settings for optional fully-static binaries.
include("fully-static.cmake")

# Include our module for gtest-based testing.
include("gtest.cmake")

# Include our module for building protos.
include("protobuf.cmake")

# Find Python3 used by integration tests, license notice and version string
if(NOT Python3_EXECUTABLE)
  find_package(Python3 COMPONENTS Interpreter REQUIRED)
endif()

# Subdirectories with their own CMakeLists.txt, all of whose targets are built.
add_subdirectory(file)
add_subdirectory(kv_pairs)
add_subdirectory(media)
add_subdirectory(hls)
add_subdirectory(mpd)
add_subdirectory(status)
add_subdirectory(third_party)
add_subdirectory(tools)
add_subdirectory(utils)
add_subdirectory(version)

set(libpackager_sources
  app/job_manager.cc
  app/job_manager.h
  app/muxer_factory.cc
  app/muxer_factory.h
  app/packager_util.cc
  app/packager_util.h
  app/single_thread_job_manager.cc
  app/single_thread_job_manager.h
  packager.cc
  live_packager.cc
  live_packager_export.cc
  ../include/packager/packager.h
  ../include/packager/live_packager.h
  ../include/packager/live_packager_export.h
)

set(libpackager_deps
  file
  hls_builder
  media_chunking
  media_codecs
  media_crypto
  demuxer
  media_event
  dvb
  mp2t
  mp4
  packed_audio
  ttml
  formats_webm
  wvm
  media_replicator
  media_trick_play
  mpd_builder
  mbedtls
  string_utils
  version
)

# A static library target is always built.

if(BUILD_SHARED_LIBS)
  add_library(libpackager SHARED ${libpackager_sources})
  target_link_libraries(libpackager ${libpackager_deps})
  target_compile_definitions(libpackager PUBLIC SHAKA_IMPLEMENTATION)
else()
  add_library(libpackager STATIC ${libpackager_sources})
  target_link_libraries(libpackager ${libpackager_deps})
endif()

# The library is always installed as
# libpackager.so / libpackager.dll / libpackager.a / libpackager.lib:
if(NOT MSVC)
  # The "lib" prefix is implied outside of MSVC.
  set_property(TARGET libpackager PROPERTY OUTPUT_NAME packager)
else()
  set_property(TARGET libpackager PROPERTY OUTPUT_NAME libpackager)
endif()

add_executable(packager
  app/ad_cue_generator_flags.cc
  app/ad_cue_generator_flags.h
  app/crypto_flags.cc
  app/crypto_flags.h
  app/hls_flags.cc
  app/hls_flags.h
  app/manifest_flags.cc
  app/manifest_flags.h
  app/mpd_flags.cc
  app/mpd_flags.h
  app/muxer_flags.cc
  app/muxer_flags.h
  app/packager_main.cc
  app/playready_key_encryption_flags.cc
  app/playready_key_encryption_flags.h
  app/raw_key_encryption_flags.cc
  app/raw_key_encryption_flags.h
  app/protection_system_flags.cc
  app/protection_system_flags.h
  app/retired_flags.cc
  app/retired_flags.h
  app/stream_descriptor.cc
  app/stream_descriptor.h
  app/validate_flag.cc
  app/validate_flag.h
  app/widevine_encryption_flags.cc
  app/widevine_encryption_flags.h
)
target_link_libraries(packager
  absl::flags
  absl::flags_parse
  absl::log
  # See https://github.com/abseil/abseil-cpp/blob/c14dfbf9/absl/log/CMakeLists.txt#L464-L467
  $<LINK_LIBRARY:WHOLE_ARCHIVE,absl::log_flags>
  absl::strings
  hex_bytes_flags
  libpackager
  license_notice
  string_utils
  ${EXTRA_EXE_LIBRARIES}
)

add_executable(mpd_generator
  app/mpd_generator.cc
  app/mpd_generator_flags.h
)
target_link_libraries(mpd_generator
  absl::flags
  absl::flags_parse
  absl::log
  # See https://github.com/abseil/abseil-cpp/blob/c14dfbf9/absl/log/CMakeLists.txt#L464-L467
  $<LINK_LIBRARY:WHOLE_ARCHIVE,absl::log_flags>
  absl::strings
  license_notice
  mpd_builder
  mpd_util
  ${EXTRA_EXE_LIBRARIES}
  )

add_executable(packager_test
  packager_test.cc
  )
target_link_libraries(packager_test
  libpackager
  gmock
  gtest
  gtest_main)

# Disabled by default, otherwise build live packager unit tests if BUILD_LIVE_TEST is on.
if(BUILD_LIVE_TEST AND NOT MSVC)
  add_executable(live_packager_test
          live_packager_test.cc
  )
  target_compile_definitions(live_packager_test
          PRIVATE
          # We used to build off of __FILE__, but that is not always an absolute
          # path, depending on the version of CMake.  This is consistent.
          TEST_DATA_DIR="${CMAKE_CURRENT_SOURCE_DIR}/media/test/data/live_packager_tests")
  target_link_libraries(live_packager_test
    libpackager
    gmock
    gtest
    gtest_main)
  add_test(NAME live_packager_test COMMAND live_packager_test)
endif()

list(APPEND packager_test_py_sources
  "${CMAKE_CURRENT_SOURCE_DIR}/app/test/packager_app.py"
  "${CMAKE_CURRENT_SOURCE_DIR}/app/test/packager_test.py"
  "${CMAKE_CURRENT_SOURCE_DIR}/app/test/test_env.py")

list(APPEND packager_test_py_output
  "${CMAKE_CURRENT_BINARY_DIR}/packager_app.py"
  "${CMAKE_CURRENT_BINARY_DIR}/packager_test.py"
  "${CMAKE_CURRENT_BINARY_DIR}/test_env.py")

add_custom_command(
  OUTPUT ${packager_test_py_output}
  COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_CURRENT_SOURCE_DIR}/app/test/packager_app.py ${CMAKE_CURRENT_BINARY_DIR}/packager_app.py
  COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_CURRENT_SOURCE_DIR}/app/test/packager_test.py ${CMAKE_CURRENT_BINARY_DIR}/packager_test.py
  COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_CURRENT_SOURCE_DIR}/app/test/test_env.py ${CMAKE_CURRENT_BINARY_DIR}/test_env.py
  DEPENDS ${packager_test_py_sources}
)

add_custom_target(packager_test_py_copy ALL
  DEPENDS ${packager_test_py_output} packager
  SOURCES ${packager_test_py_sources}
)

if(NOT SKIP_INTEGRATION_TESTS)
  add_test (NAME packager_test_py
    COMMAND "${Python3_EXECUTABLE}" packager_test.py
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
  )

  set(test_environment_vars "PACKAGER_SRC_DIR=${CMAKE_SOURCE_DIR}")
  list(APPEND test_environment_vars "PACKAGER_BIN=$<TARGET_FILE:packager>")
  list(APPEND test_environment_vars "MPD_GENERATOR_BIN=$<TARGET_FILE:mpd_generator>")
  if(BUILD_SHARED_LIBS)
    list(APPEND test_environment_vars "BUILD_TYPE=shared")
  else()
    list(APPEND test_environment_vars "BUILD_TYPE=static")
  endif()

  set_tests_properties(packager_test_py PROPERTIES
          ENVIRONMENT "${test_environment_vars}"
  )
endif()

configure_file(packager.pc.in packager.pc @ONLY)

# Always install the binaries.
install(TARGETS mpd_generator packager)

# With shared libraries, also install the library, headers, and pkgconfig.
# The static library isn't usable as a standalone because it doesn't include
# its static dependencies (zlib, absl, etc).
if(BUILD_SHARED_LIBS)
  install(TARGETS libpackager)
  install(DIRECTORY ../include/packager
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
  install(FILES ${CMAKE_CURRENT_BINARY_DIR}/packager.pc
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/pkgconfig)
endif()
