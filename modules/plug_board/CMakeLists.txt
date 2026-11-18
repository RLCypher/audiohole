cmake_minimum_required(VERSION 3.25)
project(AudioHolePlugBoard VERSION 0.2.6 LANGUAGES CXX)

# This module also has an independent CMake entry point. The AudioHole host
# disables its standalone launcher and links the embeddable view/model targets.
# Module code consumes value contracts without depending on host controllers.
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
option(PLUG_BOARD_BUILD_TESTS "Build portable contract tests" ON)
option(PLUG_BOARD_BUILD_GUI "Build the embeddable Windows view" ON)
option(PLUG_BOARD_BUILD_WINDOWS_BACKEND "Build native Windows adapters and evidence" ON)
option(PLUG_BOARD_BUILD_STANDALONE "Build/install the independent launcher" ON)
option(PLUG_BOARD_BUILD_DEVELOPER_TOOLS "Build legacy CLI fixture tools (never installed)" OFF)

add_library(plug_board_core src/core/observations.cpp src/core/policy_experiment.cpp)
add_library(audiohole::plug_board_core ALIAS plug_board_core)
target_include_directories(plug_board_core PUBLIC include)
target_compile_features(plug_board_core PUBLIC cxx_std_20)

function(plug_board_warnings target)
    if(MSVC)
        target_compile_options(${target} PRIVATE /W4 /permissive- /utf-8)
    else()
        target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic)
    endif()
endfunction()
plug_board_warnings(plug_board_core)

include(FetchContent)
if(NOT TARGET nlohmann_json::nlohmann_json)
    # Shared by the portable library and native evidence, using the existing pin.
    set(JSON_BuildTests OFF CACHE BOOL "" FORCE)
    set(JSON_Install OFF CACHE BOOL "" FORCE)
    FetchContent_Declare(nlohmann_json
        URL https://codeload.github.com/nlohmann/json/tar.gz/refs/tags/v3.11.3
        URL_HASH SHA256=0d8ef5af7f9794e3263480193c491549b2ba6cc74bb018906202ada498a79406
        DOWNLOAD_EXTRACT_TIMESTAMP FALSE)
    FetchContent_MakeAvailable(nlohmann_json)
endif()
find_package(Threads REQUIRED)
add_library(plug_board_model src/core/library.cpp src/core/storage.cpp src/core/diagnostics.cpp src/core/presentation.cpp
    src/core/module.cpp src/core/module_state.cpp src/core/module_persistence.cpp src/core/simulation.cpp)
add_library(audiohole::plug_board_model ALIAS plug_board_model)
target_link_libraries(plug_board_model PUBLIC plug_board_core nlohmann_json::nlohmann_json Threads::Threads)
plug_board_warnings(plug_board_model)
if(PLUG_BOARD_BUILD_TESTS)
    enable_testing()
    if(NOT TARGET GTest::gtest_main)
        # Match the host's existing test-only dependency and integrity pin.
        set(INSTALL_GTEST OFF CACHE BOOL "" FORCE)
        set(BUILD_GMOCK OFF CACHE BOOL "" FORCE)
        FetchContent_Declare(googletest
            URL https://codeload.github.com/google/googletest/tar.gz/refs/tags/v1.16.0
            URL_HASH SHA256=78c676fc63881529bf97bf9d45948d905a66833fbfa5318ea2cd7478cb98f399
            DOWNLOAD_EXTRACT_TIMESTAMP FALSE)
        FetchContent_MakeAvailable(googletest)
    endif()
    add_executable(plug_board_tests tests/main.cpp tests/observations_test.cpp tests/policy_experiment_test.cpp
        tests/icon_path_test.cpp)
    target_include_directories(plug_board_tests PRIVATE src/windows)
    target_link_libraries(plug_board_tests PRIVATE plug_board_core GTest::gtest)
    if(MINGW)
        target_link_options(plug_board_tests PRIVATE -static)
    endif()
    plug_board_warnings(plug_board_tests)
    # One invocation works both natively and with an explicitly supplied cross emulator.
    # It fails if no tests execute; a build alone is never recorded as a test run.
    add_test(NAME PlugBoard.Contracts COMMAND plug_board_tests)
    add_executable(plug_board_model_tests tests/main.cpp tests/library_test.cpp tests/storage_test.cpp tests/module_test.cpp tests/presentation_test.cpp)
    target_link_libraries(plug_board_model_tests PRIVATE plug_board_model GTest::gtest)
    plug_board_warnings(plug_board_model_tests)
    if(MINGW)
        target_link_options(plug_board_model_tests PRIVATE -static)
    endif()
    add_test(NAME PlugBoard.Library COMMAND plug_board_model_tests)
endif()

if(WIN32 AND PLUG_BOARD_BUILD_WINDOWS_BACKEND)
    if(NOT CMAKE_SIZEOF_VOID_P EQUAL 8)
        message(FATAL_ERROR "The private Windows ABI probe supports x64 only.")
    endif()
    if(NOT TARGET nlohmann_json::nlohmann_json)
        set(JSON_BuildTests OFF CACHE BOOL "" FORCE)
        set(JSON_Install OFF CACHE BOOL "" FORCE)
        FetchContent_Declare(nlohmann_json
            URL https://codeload.github.com/nlohmann/json/tar.gz/refs/tags/v3.11.3
            URL_HASH SHA256=0d8ef5af7f9794e3263480193c491549b2ba6cc74bb018906202ada498a79406
            DOWNLOAD_EXTRACT_TIMESTAMP FALSE)
        FetchContent_MakeAvailable(nlohmann_json)
    endif()
    add_library(plug_board_windows src/windows/guids.cpp src/windows/environment.cpp src/windows/policy.cpp
        src/windows/icons.cpp src/windows/process_watcher.cpp src/windows/backend.cpp
        src/windows/session_monitor.cpp src/windows/live_icons.cpp src/windows/live_processes.cpp
        src/windows/live_process_watcher.cpp src/windows/window_icon.cpp src/windows/shortcut_icon.cpp
        src/windows/taskbar_icon_cache.cpp)
    add_library(audiohole::plug_board_windows ALIAS plug_board_windows)
    target_include_directories(plug_board_windows PRIVATE src/windows)
    target_compile_definitions(plug_board_windows PUBLIC UNICODE _UNICODE NOMINMAX
        WIN32_LEAN_AND_MEAN _WIN32_WINNT=0x0A00 WINVER=0x0A00)
    target_link_libraries(plug_board_windows PUBLIC plug_board_model PRIVATE
        ole32 oleaut32 uuid runtimeobject propsys shell32 shlwapi windowscodecs wbemuuid gdi32 advapi32 psapi dwmapi)
    plug_board_warnings(plug_board_windows)

    # Legacy fixture tools are a source-only developer option. The application
    # does not link their runner, offer their menus or install their executables.
    if(PLUG_BOARD_BUILD_DEVELOPER_TOOLS)
        add_library(plug_board_harness STATIC harness/runner.cpp harness/evidence.cpp harness/fixture_process.cpp)
        target_include_directories(plug_board_harness PUBLIC harness PRIVATE src/windows)
        target_link_libraries(plug_board_harness PUBLIC plug_board_windows nlohmann_json::nlohmann_json)
        plug_board_warnings(plug_board_harness)
        add_executable(plug_board_probe harness/main.cpp)
        target_include_directories(plug_board_probe PRIVATE src/windows)
        target_link_libraries(plug_board_probe PRIVATE plug_board_harness)
        add_executable(plug_board_fixture harness/silent_fixture.cpp src/windows/guids.cpp)
        target_include_directories(plug_board_fixture PRIVATE include src/windows)
        target_compile_definitions(plug_board_fixture PRIVATE UNICODE _UNICODE NOMINMAX
            WIN32_LEAN_AND_MEAN _WIN32_WINNT=0x0A00)
        target_link_libraries(plug_board_fixture PRIVATE ole32 uuid runtimeobject)
        foreach(target plug_board_probe plug_board_fixture)
            plug_board_warnings(${target})
            if(MINGW)
                # The handoff executables should not require the cross-toolchain DLLs.
                target_link_options(${target} PRIVATE -static -municode)
            endif()
        endforeach()
    endif()
    if(PLUG_BOARD_BUILD_GUI)
        if(NOT TARGET wx::core)
            include(cmake/WxWidgets.cmake)
        endif()
        enable_language(RC)
        # Embedding depends on this view and model, not the standalone frame or
        # its native host shim. Hosts supply inventory, profiles and visibility.
        add_library(plug_board_wx STATIC src/wx/controls.cpp src/wx/native_theme.cpp src/wx/dialogs.cpp src/wx/view.cpp
            src/wx/socket_view.cpp src/wx/socket_style.cpp src/wx/settings_drawer.cpp)
        add_library(audiohole::plug_board_wx ALIAS plug_board_wx)
        target_include_directories(plug_board_wx PUBLIC include PRIVATE src/wx)
        target_compile_definitions(plug_board_wx PUBLIC UNICODE _UNICODE NOMINMAX WIN32_LEAN_AND_MEAN)
        target_link_libraries(plug_board_wx PUBLIC plug_board_model wx::core wx::base PRIVATE uxtheme dwmapi)
        # MSW wx builds omit the generic file-dialog implementation. Compile it
        # from the same pinned framework source for a themed file browser.
        if(NOT wxwidgets_SOURCE_DIR AND FETCHCONTENT_SOURCE_DIR_WXWIDGETS)
            set(wxwidgets_SOURCE_DIR "${FETCHCONTENT_SOURCE_DIR_WXWIDGETS}")
        endif()
        if(NOT EXISTS "${wxwidgets_SOURCE_DIR}/src/generic/filedlgg.cpp")
            message(FATAL_ERROR "The themed file picker needs matching wx source; set FETCHCONTENT_SOURCE_DIR_WXWIDGETS.")
        endif()
        target_sources(plug_board_wx PRIVATE "${wxwidgets_SOURCE_DIR}/src/generic/filedlgg.cpp")
        plug_board_warnings(plug_board_wx)
        if(PLUG_BOARD_BUILD_STANDALONE)
        add_executable(plug_board_launcher WIN32 src/wx/main.cpp src/wx/board_frame.cpp
            src/wx/board_smoke.cpp src/wx/popup_smoke.cpp resources/launcher.rc)
        target_include_directories(plug_board_launcher PRIVATE src/wx resources)
        # uuid precedes wx's oleacc dependency, matching the host's MinGW link order.
        target_link_libraries(plug_board_launcher PRIVATE plug_board_windows uuid plug_board_wx)
        plug_board_warnings(plug_board_launcher)
        set_target_properties(plug_board_launcher PROPERTIES OUTPUT_NAME "AudioHole-PlugBoard")
        if(MINGW)
            target_link_options(plug_board_launcher PRIVATE -static)
            set_source_files_properties(resources/launcher.rc PROPERTIES
                COMPILE_OPTIONS "--preprocessor=${CMAKE_CXX_COMPILER};--preprocessor-arg=-E;--preprocessor-arg=-xc;--preprocessor-arg=-DRC_INVOKED")
        endif()
        install(TARGETS plug_board_launcher RUNTIME DESTINATION .)
        if(EXISTS "${wxwidgets_SOURCE_DIR}/docs/licence.txt")
            install(FILES "${wxwidgets_SOURCE_DIR}/docs/licence.txt" "${wxwidgets_SOURCE_DIR}/docs/lgpl.txt"
                DESTINATION licenses)
        endif()
        if(PLUG_BOARD_BUILD_TESTS)
            add_test(NAME PlugBoard.BoardSmoke COMMAND plug_board_launcher --board-smoke
                --smoke-output "${CMAKE_BINARY_DIR}/board-smoke")
            set_tests_properties(PlugBoard.BoardSmoke PROPERTIES TIMEOUT 120)
        endif()
        endif()
    endif()
    if(PLUG_BOARD_BUILD_TESTS)
        add_executable(plug_board_windows_tests tests/main.cpp tests/windows_evidence_test.cpp tests/window_icon_test.cpp harness/evidence.cpp)
        target_include_directories(plug_board_windows_tests PRIVATE harness src/windows)
        target_link_libraries(plug_board_windows_tests PRIVATE plug_board_windows
            nlohmann_json::nlohmann_json GTest::gtest)
        if(TARGET plug_board_launcher)
            # Read the launcher's known embedded icon as test data; never launch it.
            add_dependencies(plug_board_windows_tests plug_board_launcher)
            target_compile_definitions(plug_board_windows_tests PRIVATE PLUG_BOARD_HAS_ICON_FIXTURE)
        endif()
        plug_board_warnings(plug_board_windows_tests)
        if(MINGW)
            target_link_options(plug_board_windows_tests PRIVATE -static)
        endif()
        add_test(NAME PlugBoard.WindowsEvidence COMMAND plug_board_windows_tests)
        # This suite observes the real 30-second icon startup-refresh window,
        # then checks that it stays idle. The host presets' 30-second default
        # would kill that check before it can finish; retain a bounded CI budget.
        set_tests_properties(PlugBoard.WindowsEvidence PROPERTIES TIMEOUT 90)
        if(PLUG_BOARD_BUILD_DEVELOPER_TOOLS)
            add_test(NAME PlugBoard.ProbeHelp COMMAND plug_board_probe --help)
        endif()
    endif()
    if(PLUG_BOARD_BUILD_STANDALONE)
    install(FILES README.md START_HERE.txt DESTINATION .)
    install(FILES resources/socket-icons/LICENSE.txt DESTINATION licenses RENAME Material-Symbols-LICENSE.txt)
    install(FILES resources/socket-icons/PROVENANCE.json DESTINATION licenses RENAME Material-Symbols-PROVENANCE.json)
    install(FILES resources/socket-icons/NOTICE.txt DESTINATION licenses RENAME Material-Symbols-NOTICE.txt)
    install(FILES docs/NATIVE_VALIDATION.md docs/MILESTONE_1.md docs/NATIVE_INSPECTION_8852.md
        docs/CONFIGURATION.md docs/API.md docs/COMPATIBILITY.md docs/IMPLEMENTATION_STATUS.md
        docs/library-v1.schema.json docs/library-v1.example.json
        DESTINATION docs)
    endif()
    if(EXISTS "${nlohmann_json_SOURCE_DIR}/LICENSE.MIT")
        install(FILES "${nlohmann_json_SOURCE_DIR}/LICENSE.MIT"
            DESTINATION licenses RENAME nlohmann-json-LICENSE.txt)
    endif()
    if(PLUG_BOARD_BUILD_TESTS AND EXISTS "${googletest_SOURCE_DIR}/LICENSE")
        install(FILES "${googletest_SOURCE_DIR}/LICENSE"
            DESTINATION licenses RENAME googletest-LICENSE.txt)
    endif()
endif()
