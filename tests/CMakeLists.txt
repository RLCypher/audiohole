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
