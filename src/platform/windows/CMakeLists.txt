add_library(audiohole_windows STATIC com_apartment.cpp diagnostics.cpp)
target_sources(audiohole_windows PRIVATE update_service.cpp)
target_include_directories(audiohole_windows PUBLIC "${PROJECT_SOURCE_DIR}/src")
target_compile_features(audiohole_windows PUBLIC cxx_std_20)
target_compile_definitions(audiohole_windows PUBLIC
  WIN32_LEAN_AND_MEAN NOMINMAX UNICODE _UNICODE
  _WIN32_WINNT=0x0A00 WINVER=0x0A00
)
target_link_libraries(audiohole_windows PUBLIC ole32 uuid)
target_link_libraries(audiohole_windows PUBLIC audiohole_model winhttp bcrypt)
if(MSVC)
  target_compile_options(audiohole_windows PRIVATE /W4 /permissive- /utf-8)
else()
  target_compile_options(audiohole_windows PRIVATE -Wall -Wextra -Wpedantic)
endif()
