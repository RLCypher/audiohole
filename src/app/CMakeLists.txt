# Temporary developer harness, not the final GUI or a public CLI.
add_executable(audiohole_dev main.cpp)
target_link_libraries(audiohole_dev PRIVATE audiohole_windows)
if(MSVC)
  target_compile_options(audiohole_dev PRIVATE /W4 /permissive- /utf-8)
  target_link_options(audiohole_dev PRIVATE
    /MANIFEST:EMBED
    "/MANIFESTUAC:level='asInvoker' uiAccess='false'"
  )
endif()
