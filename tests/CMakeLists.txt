include(GoogleTest)
add_executable(audiohole_model_tests model_test.cpp avatar_host_test.cpp text_layout_test.cpp personality_files_test.cpp icon_outline_test.cpp dfa_test.cpp persistence_test.cpp application_controller_test.cpp)
target_compile_definitions(audiohole_model_tests PRIVATE AUDIOHOLE_TEST_PERSONALITIES_DIR="${PROJECT_SOURCE_DIR}/personalities")
target_link_libraries(audiohole_model_tests PRIVATE audiohole_model GTest::gtest_main)
if(MSVC)
  target_compile_options(audiohole_model_tests PRIVATE /W4 /permissive- /utf-8)
endif()
gtest_discover_tests(audiohole_model_tests DISCOVERY_MODE PRE_TEST)

if(NOT AUDIOHOLE_MODEL_ONLY)
  add_executable(audiohole_tests com_apartment_test.cpp)
  target_link_libraries(audiohole_tests PRIVATE audiohole_windows GTest::gtest_main)
  if(MSVC)
    target_compile_options(audiohole_tests PRIVATE /W4 /permissive- /utf-8)
  endif()

  include(GoogleTest)
  # Discover at test time so configuring/building never needs to execute a target
  # binary (useful for a diagnostic cross-build). Windows execution is still needed.
  gtest_discover_tests(audiohole_tests DISCOVERY_MODE PRE_TEST)
  add_test(NAME Foundation.DeveloperHarnessLaunch COMMAND audiohole_dev)
  set_tests_properties(Foundation.DeveloperHarnessLaunch PROPERTIES
    TIMEOUT 10
  )

  add_test(NAME Gui.Smoke COMMAND audiohole --smoke-test --screenshot-dir "${CMAKE_BINARY_DIR}/gui-smoke/$<CONFIG>")
  set_tests_properties(Gui.Smoke PROPERTIES TIMEOUT 45)
  add_test(NAME Gui.TestModeSmoke COMMAND audiohole --test-mode-smoke --screenshot-dir "${CMAKE_BINARY_DIR}/gui-smoke/$<CONFIG>/test-mode")
  set_tests_properties(Gui.TestModeSmoke PROPERTIES TIMEOUT 45)
endif()

# Optional locally; the Windows CI workflow explicitly runs the same dependency-
# free Python check. Regenerating icons alone additionally needs Pillow.
find_package(Python3 COMPONENTS Interpreter QUIET)
if(Python3_Interpreter_FOUND)
  add_test(NAME Branding.AssetIntegrity
    COMMAND "${Python3_EXECUTABLE}" "${PROJECT_SOURCE_DIR}/scripts/branding/verify_assets.py")
  add_test(NAME Avatar.AssetIntegrity COMMAND "${Python3_EXECUTABLE}" "${PROJECT_SOURCE_DIR}/scripts/branding/verify_avatar_assets.py")
endif()

if(Python3_Interpreter_FOUND AND TARGET avatar_tests)
  add_test(NAME Avatar.NativePixels COMMAND "${Python3_EXECUTABLE}"
    "${PROJECT_SOURCE_DIR}/scripts/branding/verify_avatar_assets.py"
    --decoded-report "${CMAKE_BINARY_DIR}/assets/personalities/runtime/validation/native-tests.json")
  set_tests_properties(Avatar.NativePixels PROPERTIES DEPENDS avatar_engine TIMEOUT 30)
endif()
