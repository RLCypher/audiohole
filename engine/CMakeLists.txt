cmake_minimum_required(VERSION 3.25)

# This directory is deliberately buildable on its own. A future GUI host can
# add_subdirectory(engine) without introducing widget or simulator dependencies.
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
  project(AudioHoleEngine VERSION 0.6.0 LANGUAGES C CXX)
  if(MSVC)
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
  endif()
  include(CTest)
endif()

option(AUDIOHOLE_ENGINE_WITH_MINIAUDIO "Build the validation miniaudio adapter on Windows" ON)

if(NOT TARGET nlohmann_json::nlohmann_json)
  include(FetchContent)
  set(JSON_BuildTests OFF CACHE BOOL "" FORCE)
  set(JSON_Install OFF CACHE BOOL "" FORCE)
  FetchContent_Declare(nlohmann_json
    URL https://codeload.github.com/nlohmann/json/tar.gz/refs/tags/v3.11.3
    URL_HASH SHA256=0d8ef5af7f9794e3263480193c491549b2ba6cc74bb018906202ada498a79406
    DOWNLOAD_EXTRACT_TIMESTAMP FALSE)
  FetchContent_MakeAvailable(nlohmann_json)
endif()

find_package(Threads REQUIRED)
add_library(audiohole_engine STATIC src/runtime.cpp src/stream_pipeline.cpp src/wasapi.cpp src/miniaudio.cpp src/access_negotiation.cpp)
add_library(audiohole::engine ALIAS audiohole_engine)
target_compile_features(audiohole_engine PUBLIC cxx_std_20)
target_include_directories(audiohole_engine PUBLIC include)
target_link_libraries(audiohole_engine PUBLIC audiohole::engine_contract PRIVATE Threads::Threads)
if(WIN32)
  target_compile_definitions(audiohole_engine PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN _WIN32_WINNT=0x0A00)
  target_link_libraries(audiohole_engine PRIVATE ole32 uuid propsys avrt)
  if(AUDIOHOLE_ENGINE_WITH_MINIAUDIO)
    include(FetchContent)
    FetchContent_Declare(miniaudio
      URL https://codeload.github.com/mackron/miniaudio/tar.gz/9634bedb5b5a2ca38c1ee7108a9358a4e233f14d
      URL_HASH SHA256=1a3a79b80fc6f0b0cc155e28b954a598e0ddfa2db64e2afa8466be88c476fa55
      DOWNLOAD_EXTRACT_TIMESTAMP FALSE SOURCE_SUBDIR audiohole-no-upstream-cmake)
    FetchContent_MakeAvailable(miniaudio)
    add_library(audiohole_engine_miniaudio STATIC src/miniaudio_impl.c)
    target_include_directories(audiohole_engine_miniaudio PUBLIC "${miniaudio_SOURCE_DIR}")
    target_compile_definitions(audiohole_engine_miniaudio PUBLIC
      MA_ENABLE_ONLY_SPECIFIC_BACKENDS MA_ENABLE_WASAPI
      MA_NO_DECODING MA_NO_ENCODING MA_NO_ENGINE MA_NO_RESOURCE_MANAGER MA_NO_NODE_GRAPH)
    target_link_libraries(audiohole_engine PRIVATE audiohole_engine_miniaudio)
    target_compile_definitions(audiohole_engine PRIVATE AUDIOHOLE_ENGINE_MINIAUDIO)
    install(FILES "${miniaudio_SOURCE_DIR}/LICENSE" DESTINATION licenses RENAME miniaudio-LICENSE.txt COMPONENT Runtime)
  endif()
endif()
if(MSVC)
  target_compile_options(audiohole_engine PRIVATE /W4 /permissive- /utf-8)
else()
  target_compile_options(audiohole_engine PRIVATE -Wall -Wextra -Wpedantic)
endif()

add_library(audiohole_engine_contract STATIC
  src/profile.cpp src/planning.cpp src/channel_mapping.cpp src/backend.cpp)
add_library(audiohole::engine_contract ALIAS audiohole_engine_contract)
target_compile_features(audiohole_engine_contract PUBLIC cxx_std_20)
target_include_directories(audiohole_engine_contract PUBLIC include)
target_link_libraries(audiohole_engine_contract PRIVATE nlohmann_json::nlohmann_json)
if(MSVC)
  target_compile_options(audiohole_engine_contract PRIVATE /W4 /permissive- /utf-8)
else()
  target_compile_options(audiohole_engine_contract PRIVATE -Wall -Wextra -Wpedantic)
endif()

option(AUDIOHOLE_ENGINE_BUILD_VALIDATION "Build standalone profile validation/routing host" ON)
if(AUDIOHOLE_ENGINE_BUILD_VALIDATION)
  # Read-only scanner inventory adapter belongs to the validation HOST, not the
  # engine module. No model/controller, GUI, ZIP exporter or Deep fixture runner.
  set(AUDIOHOLE_ENGINE_REPOSITORY "${CMAKE_CURRENT_SOURCE_DIR}/..")
  add_library(audiohole_engine_registry STATIC
    "${AUDIOHOLE_ENGINE_REPOSITORY}/src/scanner/scanner.cpp"
    "${AUDIOHOLE_ENGINE_REPOSITORY}/src/scanner/store.cpp"
    "${AUDIOHOLE_ENGINE_REPOSITORY}/src/scanner/atomic_file.cpp")
  target_compile_features(audiohole_engine_registry PUBLIC cxx_std_20)
  target_include_directories(audiohole_engine_registry PUBLIC "${AUDIOHOLE_ENGINE_REPOSITORY}/include")
  target_link_libraries(audiohole_engine_registry PUBLIC nlohmann_json::nlohmann_json)
  if(WIN32)
    target_compile_definitions(audiohole_engine_registry PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN _WIN32_WINNT=0x0A00)
  endif()
  add_executable(audiohole-engine-validation validation/main.cpp)
  target_link_libraries(audiohole-engine-validation PRIVATE audiohole::engine audiohole_engine_registry)
  if(MINGW)
    target_link_options(audiohole-engine-validation PRIVATE -municode -static)
  endif()
  install(TARGETS audiohole-engine-validation RUNTIME DESTINATION .)
  install(FILES "${nlohmann_json_SOURCE_DIR}/LICENSE.MIT" DESTINATION licenses RENAME nlohmann-json-LICENSE.txt)
endif()

option(AUDIOHOLE_ENGINE_BUILD_BENCHMARKS "Build software-only PCM microbenchmarks" OFF)
if(AUDIOHOLE_ENGINE_BUILD_BENCHMARKS)
  add_executable(audiohole_pipeline_benchmark benchmarks/pipeline_benchmark.cpp)
  target_link_libraries(audiohole_pipeline_benchmark PRIVATE audiohole::engine)
  target_include_directories(audiohole_pipeline_benchmark PRIVATE src)
endif()

if(BUILD_TESTING)
  add_executable(audiohole_engine_contract_tests tests/contract_tests.cpp)
  target_link_libraries(audiohole_engine_contract_tests PRIVATE
    audiohole::engine_contract nlohmann_json::nlohmann_json)
  target_compile_definitions(audiohole_engine_contract_tests PRIVATE
    AUDIOHOLE_ENGINE_EXAMPLE="${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/profile-v1-example.json"
    AUDIOHOLE_ENGINE_V3_EXAMPLE="${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/profile-v3-corrected.json")
  add_test(NAME audiohole-engine-contract COMMAND audiohole_engine_contract_tests)
  add_executable(audiohole_engine_runtime_tests tests/runtime_tests.cpp)
  target_link_libraries(audiohole_engine_runtime_tests PRIVATE audiohole::engine)
  target_include_directories(audiohole_engine_runtime_tests PRIVATE src)
  add_test(NAME audiohole-engine-processing COMMAND audiohole_engine_runtime_tests)
  add_executable(audiohole_engine_access_tests tests/access_tests.cpp)
  target_link_libraries(audiohole_engine_access_tests PRIVATE audiohole::engine)
  target_include_directories(audiohole_engine_access_tests PRIVATE src)
  add_test(NAME audiohole-engine-access COMMAND audiohole_engine_access_tests)
  add_executable(audiohole_engine_startup_tests tests/startup_tests.cpp)
  target_link_libraries(audiohole_engine_startup_tests PRIVATE audiohole::engine)
  target_include_directories(audiohole_engine_startup_tests PRIVATE src)
  add_test(NAME audiohole-engine-startup COMMAND audiohole_engine_startup_tests)
  set_tests_properties(audiohole-engine-contract audiohole-engine-processing audiohole-engine-access audiohole-engine-startup PROPERTIES TIMEOUT 90)
endif()
