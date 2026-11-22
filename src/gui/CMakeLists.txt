# About/Update identifies the executable independently of adjacent data files.
add_executable(audiohole WIN32 branding.cpp controls.cpp appearance.cpp app.rc welcome.cpp license_dialog.cpp main.cpp frame.cpp plug_board_view.cpp plug_board_smoke.cpp profile_view.cpp device_view.cpp diagnostic_view.cpp tutorial_view.cpp engine_smoke.cpp diagnostics_view.cpp settings_view.cpp tray.cpp shell_actions.cpp device_list.cpp seconds_field.cpp native_theme.cpp)
target_include_directories(audiohole PRIVATE "${PROJECT_SOURCE_DIR}")
target_compile_definitions(audiohole PRIVATE AUDIOHOLE_BUILD_COMMIT="${audiohole_short_commit}")
target_compile_definitions(audiohole PRIVATE AUDIOHOLE_VERSION="${PROJECT_VERSION}")
target_compile_definitions(audiohole PRIVATE AUDIOHOLE_BUILD_VINTAGE="${audiohole_build_vintage}")
# uuid precedes wx's transitive oleacc import library to avoid duplicate GUID
# objects in MinGW; MSVC uses the same system GUID library.
target_link_libraries(audiohole PRIVATE audiohole::avatar_wx audiohole_model audiohole_windows audiohole::plug_board_windows uuid audiohole::plug_board_wx wx::core wx::base uxtheme dwmapi advapi32 windowscodecs shlwapi propsys shell32)
if(MSVC)
  target_compile_definitions(audiohole PRIVATE AUDIOHOLE_MSVC_MANIFEST)
  target_compile_options(audiohole PRIVATE "$<$<COMPILE_LANGUAGE:CXX>:/W4;/permissive-;/utf-8>")
  # Use wxWidgets' reviewed common-controls/asInvoker/per-monitor-v2 manifest.
  # The RC path is disabled under MSVC so mt.exe merges exactly one input.
  target_link_options(audiohole PRIVATE /MANIFEST:EMBED
    "/MANIFESTINPUT:${wxwidgets_SOURCE_DIR}/include/wx/msw/wx_dpi_aware_pmv2.manifest"
    "/MANIFESTUAC:level='asInvoker' uiAccess='false'")
endif()
set_target_properties(audiohole PROPERTIES OUTPUT_NAME "AudioHole")
if(TARGET plug_board_windows_tests)
  # The handover's icon test reads an executable's resource without launching it.
  # Embedding uses the real host, so no standalone fixture executable is needed.
  add_dependencies(plug_board_windows_tests audiohole)
  target_compile_definitions(plug_board_windows_tests PRIVATE PLUG_BOARD_ICON_FIXTURE_NAME="AudioHole.exe")
endif()
install(TARGETS audiohole RUNTIME DESTINATION . COMPONENT Runtime)
install(FILES "${PROJECT_SOURCE_DIR}/third_party/licenses/wxWidgets-LICENSE.txt"
  "${PROJECT_SOURCE_DIR}/third_party/licenses/wxWidgets-LGPL.txt" DESTINATION licenses COMPONENT Runtime)
install(FILES "${PROJECT_SOURCE_DIR}/modules/plug_board/resources/socket-icons/NOTICE.txt"
  DESTINATION licenses RENAME PlugBoard-MaterialSymbols-NOTICE.txt COMPONENT Runtime)
install(FILES "${PROJECT_SOURCE_DIR}/modules/plug_board/resources/socket-icons/LICENSE.txt"
  DESTINATION licenses RENAME PlugBoard-MaterialSymbols-LICENSE.txt COMPONENT Runtime)
add_custom_command(TARGET audiohole POST_BUILD COMMAND ${CMAKE_COMMAND} -E copy_directory
  "${PROJECT_SOURCE_DIR}/assets/tutorial" "$<TARGET_FILE_DIR:audiohole>/assets/tutorial")
install(DIRECTORY "${PROJECT_SOURCE_DIR}/assets/tutorial/" DESTINATION assets/tutorial COMPONENT Runtime
  PATTERN "provenance.json" EXCLUDE)

add_custom_target(audiohole_personality_files ALL
  # Remove only obsolete source-owned flat catalogs from an incremental build's
  # staging area. Fresh distributions contain folder packages, not both layouts.
  COMMAND ${CMAKE_COMMAND} -E rm -f "$<TARGET_FILE_DIR:audiohole>/personalities/default.json" "$<TARGET_FILE_DIR:audiohole>/personalities/uwu.json"
  COMMAND ${CMAKE_COMMAND} -E copy_directory "${PROJECT_SOURCE_DIR}/personalities" "$<TARGET_FILE_DIR:audiohole>/personalities"
  COMMENT "Stage editable personality files beside the GUI")
add_dependencies(audiohole audiohole_personality_files audiohole-probe)
install(DIRECTORY "${PROJECT_SOURCE_DIR}/personalities/" DESTINATION personalities COMPONENT Runtime
  FILES_MATCHING PATTERN "*.json" PATTERN "*.png")
install(FILES "${PROJECT_SOURCE_DIR}/personalities/tutorial/tutorial.txt"
  DESTINATION personalities/tutorial COMPONENT Runtime)
install(FILES "${PROJECT_SOURCE_DIR}/third_party/licenses/nlohmann-json-LICENSE.txt" DESTINATION licenses COMPONENT Runtime)

# Branding and fallback artwork are embedded; retain source dependencies without
# distributing redundant copies of the resources or their engineering evidence.
set_source_files_properties(app.rc PROPERTIES OBJECT_DEPENDS
  "${PROJECT_SOURCE_DIR}/assets/branding/audiohole-logo.png;${PROJECT_SOURCE_DIR}/assets/branding/audiohole.ico")

set_property(SOURCE app.rc APPEND PROPERTY OBJECT_DEPENDS
  "${PROJECT_SOURCE_DIR}/assets/presentation/placeholder-avatar.png")

# Apache-2.0 Material icon font, loaded privately for this process only.
add_custom_command(TARGET audiohole POST_BUILD COMMAND "${CMAKE_COMMAND}" -E copy_directory
  "${PROJECT_SOURCE_DIR}/assets/ui" "$<TARGET_FILE_DIR:audiohole>/assets/ui")
install(FILES "${PROJECT_SOURCE_DIR}/assets/ui/MaterialIcons-Regular.ttf" DESTINATION assets/ui COMPONENT Runtime)
install(FILES "${PROJECT_SOURCE_DIR}/assets/ui/MaterialIcons-LICENSE.txt" DESTINATION licenses COMPONENT Runtime)
install(FILES "${PROJECT_SOURCE_DIR}/assets/ui/NOTICE.txt" DESTINATION licenses RENAME MaterialIcons-NOTICE.txt COMPONENT Runtime)

install(FILES "${PROJECT_SOURCE_DIR}/docs/legal/AudioHole License.rtf" DESTINATION . COMPONENT Runtime)
install(FILES "${PROJECT_SOURCE_DIR}/LICENSE" DESTINATION licenses RENAME GPL-3.0.txt COMPONENT Runtime)
install(FILES "${PROJECT_SOURCE_DIR}/docs/legal/assets.txt" DESTINATION licenses COMPONENT Runtime)
install(FILES "${PROJECT_SOURCE_DIR}/docs/development/SOURCE.md"
  DESTINATION licenses RENAME SOURCE.txt COMPONENT Runtime)
install(FILES "${PROJECT_SOURCE_DIR}/third_party/licenses/InnoSetup-LICENSE.txt"
  DESTINATION licenses COMPONENT Runtime)
install(FILES "${PROJECT_SOURCE_DIR}/docs/guides/AI-DISCLOSURE.txt"
  "${PROJECT_SOURCE_DIR}/docs/guides/Advanced Buffer Tuning Guide.txt"
  "${PROJECT_SOURCE_DIR}/docs/guides/AudioHole User Manual.txt"
  "${PROJECT_SOURCE_DIR}/docs/guides/Speedrunning the Tutorial.txt"
  "${PROJECT_SOURCE_DIR}/docs/guides/The AudioHole Manifesto.txt"
  DESTINATION "Guides and Manuals" COMPONENT Runtime)

set_property(SOURCE app.rc APPEND PROPERTY OBJECT_DEPENDS "${PROJECT_SOURCE_DIR}/docs/legal/AudioHole License.rtf")
