# The title identifies the executable itself, even if an older artifact is extracted
# beside newer personality files. CI configures afresh for each checked-out revision.
find_package(Git QUIET)
set(audiohole_build_commit "unknown")
if(Git_FOUND)
  execute_process(COMMAND "${GIT_EXECUTABLE}" rev-parse --short=12 HEAD
    WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}" OUTPUT_VARIABLE audiohole_build_commit
    OUTPUT_STRIP_TRAILING_WHITESPACE RESULT_VARIABLE audiohole_git_result)
  if(NOT audiohole_git_result EQUAL 0)
    set(audiohole_build_commit "unknown")
  endif()
endif()
add_executable(audiohole WIN32 branding.cpp controls.cpp appearance.cpp app.rc welcome.cpp main.cpp frame.cpp profile_view.cpp device_view.cpp diagnostic_view.cpp tutorial_view.cpp engine_smoke.cpp diagnostics_view.cpp settings_view.cpp tray.cpp shell_actions.cpp device_list.cpp seconds_field.cpp native_theme.cpp)
target_include_directories(audiohole PRIVATE "${PROJECT_SOURCE_DIR}")
target_compile_definitions(audiohole PRIVATE AUDIOHOLE_BUILD_COMMIT="${audiohole_build_commit}")
# uuid precedes wx's transitive oleacc import library to avoid duplicate GUID
# objects in MinGW; MSVC uses the same system GUID library.
target_link_libraries(audiohole PRIVATE audiohole::avatar_wx audiohole_model audiohole_windows wx::core wx::base uuid uxtheme dwmapi advapi32 windowscodecs shlwapi propsys shell32)
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
install(TARGETS audiohole RUNTIME DESTINATION . COMPONENT Runtime)
install(FILES "${PROJECT_SOURCE_DIR}/third_party/licenses/wxWidgets-LICENSE.txt"
  "${PROJECT_SOURCE_DIR}/third_party/licenses/wxWidgets-LGPL.txt" DESTINATION licenses COMPONENT Runtime)
install(FILES "${PROJECT_SOURCE_DIR}/CONFIGURATION_BUNDLES.md"
  "${PROJECT_SOURCE_DIR}/PROFILE_BUILDER.md"
  "${PROJECT_SOURCE_DIR}/MANUAL_GUI_CHECKLIST.md" DESTINATION docs COMPONENT Runtime)
install(FILES "${PROJECT_SOURCE_DIR}/TUTORIAL.md" "${PROJECT_SOURCE_DIR}/ENGINE_INTEGRATION.md" DESTINATION docs COMPONENT Runtime)
install(FILES "${PROJECT_SOURCE_DIR}/docs/tutorial/Norm-script.txt" DESTINATION docs/tutorial COMPONENT Runtime)

add_custom_target(audiohole_personality_files ALL
  # Remove only obsolete source-owned flat catalogs from an incremental build's
  # staging area. Fresh distributions contain folder packages, not both layouts.
  COMMAND ${CMAKE_COMMAND} -E rm -f "$<TARGET_FILE_DIR:audiohole>/personalities/default.json" "$<TARGET_FILE_DIR:audiohole>/personalities/uwu.json"
  COMMAND ${CMAKE_COMMAND} -E copy_directory "${PROJECT_SOURCE_DIR}/personalities" "$<TARGET_FILE_DIR:audiohole>/personalities"
  COMMENT "Stage editable personality files beside the GUI")
add_dependencies(audiohole audiohole_personality_files audiohole-probe)
install(DIRECTORY "${PROJECT_SOURCE_DIR}/personalities/" DESTINATION personalities COMPONENT Runtime
  FILES_MATCHING PATTERN "*.json" PATTERN "*.png" PATTERN "README.md")
install(FILES "${PROJECT_SOURCE_DIR}/third_party/licenses/nlohmann-json-LICENSE.txt" DESTINATION licenses COMPONENT Runtime)

# Original and ICO are embedded for offline/isolated launch; copies in the ZIP
# make the canonical artwork and its provenance available to maintainers/users.
install(FILES "${PROJECT_SOURCE_DIR}/assets/branding/audiohole-logo.png"
  "${PROJECT_SOURCE_DIR}/assets/branding/audiohole.ico"
  DESTINATION assets/branding COMPONENT Runtime)

set_source_files_properties(app.rc PROPERTIES OBJECT_DEPENDS
  "${PROJECT_SOURCE_DIR}/assets/branding/audiohole-logo.png;${PROJECT_SOURCE_DIR}/assets/branding/audiohole.ico")

install(FILES "${PROJECT_SOURCE_DIR}/assets/presentation/placeholder-avatar.png"
  DESTINATION assets/presentation COMPONENT Runtime)
set_property(SOURCE app.rc APPEND PROPERTY OBJECT_DEPENDS
  "${PROJECT_SOURCE_DIR}/assets/presentation/placeholder-avatar.png")

install(FILES "${PROJECT_SOURCE_DIR}/assets/personalities/provenance/deployed-assets.json"
  DESTINATION assets RENAME avatar-assets.json COMPONENT Runtime)

# Apache-2.0 Material icon font, loaded privately for this process only.
add_custom_command(TARGET audiohole POST_BUILD COMMAND "${CMAKE_COMMAND}" -E copy_directory
  "${PROJECT_SOURCE_DIR}/assets/ui" "$<TARGET_FILE_DIR:audiohole>/assets/ui")
install(FILES "${PROJECT_SOURCE_DIR}/assets/ui/MaterialIcons-Regular.ttf" DESTINATION assets/ui COMPONENT Runtime)
install(FILES "${PROJECT_SOURCE_DIR}/assets/ui/MaterialIcons-LICENSE.txt" DESTINATION licenses COMPONENT Runtime)
install(FILES "${PROJECT_SOURCE_DIR}/assets/ui/NOTICE.txt" DESTINATION licenses RENAME MaterialIcons-NOTICE.txt COMPONENT Runtime)
