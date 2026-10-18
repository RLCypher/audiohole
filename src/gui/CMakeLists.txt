add_executable(audiohole WIN32 app.rc main.cpp frame.cpp profile_view.cpp device_view.cpp diagnostics_view.cpp settings_view.cpp tray.cpp)
target_link_libraries(audiohole PRIVATE audiohole_model audiohole_windows wx::core wx::base)
if(MSVC)
  target_compile_definitions(audiohole PRIVATE AUDIOHOLE_MSVC_MANIFEST)
  target_compile_options(audiohole PRIVATE "$<$<COMPILE_LANGUAGE:CXX>:/W4;/permissive-;/utf-8>")
  # Use wxWidgets' reviewed common-controls/asInvoker/per-monitor-v2 manifest.
  # The RC path is disabled under MSVC so mt.exe merges exactly one input.
  target_link_options(audiohole PRIVATE /MANIFEST:EMBED
    "/MANIFESTINPUT:${wxwidgets_SOURCE_DIR}/include/wx/msw/wx_dpi_aware_pmv2.manifest"
    "/MANIFESTUAC:level='asInvoker' uiAccess='false'")
endif()
install(TARGETS audiohole RUNTIME DESTINATION . COMPONENT Runtime)
install(FILES "${PROJECT_SOURCE_DIR}/third_party/licenses/wxWidgets-LICENSE.txt"
  "${PROJECT_SOURCE_DIR}/third_party/licenses/wxWidgets-LGPL.txt"
  "${PROJECT_SOURCE_DIR}/MANUAL_GUI_CHECKLIST.md" DESTINATION . COMPONENT Runtime)
