cmake_minimum_required(VERSION 3.25)

project(AudioHole VERSION 0.1.0 LANGUAGES CXX)

option(AUDIOHOLE_MODEL_ONLY "Build portable simulator tests only (no application)" OFF)

if(NOT WIN32 AND NOT AUDIOHOLE_MODEL_ONLY)
  message(FATAL_ERROR "AudioHole requires a Windows x64 toolchain. See BUILDING.md.")
endif()
if(NOT CMAKE_SIZEOF_VOID_P EQUAL 8)
  message(FATAL_ERROR "AudioHole requires a 64-bit x64 target.")
endif()
if(MSVC AND NOT CMAKE_CXX_COMPILER_ARCHITECTURE_ID STREQUAL "x64")
  message(FATAL_ERROR "Select the x64 MSVC target (the windows-msvc preset does this).")
endif()

# Static MSVC CRT: the development harness does not require a VC runtime DLL.
# GoogleTest inherits the same selection; no additional runtime libraries are shipped.
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")

include(CTest)
add_subdirectory(src/model)
if(NOT AUDIOHOLE_MODEL_ONLY)
  add_subdirectory(src/platform/windows)
  add_subdirectory(src/app)
  include(cmake/WxWidgets.cmake)
  add_subdirectory(src/gui)
endif()

if(BUILD_TESTING)
  include(FetchContent)
  # GoogleTest v1.16.0, commit 6910c9d9165801d8827d628cb72eb7ea9dd538c5.
  # Test-only dependency. Download integrity is checked before extraction.
  FetchContent_Declare(googletest
    URL https://codeload.github.com/google/googletest/tar.gz/refs/tags/v1.16.0
    URL_HASH SHA256=78c676fc63881529bf97bf9d45948d905a66833fbfa5318ea2cd7478cb98f399
    DOWNLOAD_EXTRACT_TIMESTAMP FALSE
  )
  set(INSTALL_GTEST OFF CACHE BOOL "" FORCE)
  set(BUILD_GMOCK OFF CACHE BOOL "" FORCE)
  FetchContent_MakeAvailable(googletest)
  add_subdirectory(tests)
endif()
