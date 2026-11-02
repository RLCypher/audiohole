# PROPOSED parent integration; this file is not applied to AudioHole's build.
# Include from the root AFTER add_subdirectory(src/gui), when wx and the
# existing audiohole target are defined. No changes are applied by this file here.
set(AUDIOHOLE_AVATAR_BUILD_VIEWER OFF CACHE BOOL "Build independent avatar viewer")
set(AUDIOHOLE_AVATAR_BUILD_TESTS OFF CACHE BOOL "Run separately in the asset workspace")
add_subdirectory(assets/personalities/runtime)
target_link_libraries(audiohole PRIVATE audiohole::avatar_wx)
target_include_directories(audiohole PRIVATE "${PROJECT_SOURCE_DIR}/assets/personalities/integration")

# Stage this layout beside the application; pass this folder as the asset root.
set(avatar_root "${PROJECT_SOURCE_DIR}/assets/personalities")
install(FILES "${avatar_root}/registry.json" "${avatar_root}/states.json"
        DESTINATION assets/personalities COMPONENT Runtime)
install(DIRECTORY "${avatar_root}/schemas" DESTINATION assets/personalities COMPONENT Runtime)
foreach(pack Norm UwU Vanilla)
  install(FILES "${avatar_root}/${pack}/manifest.json"
          DESTINATION "assets/personalities/${pack}" COMPONENT Runtime)
  install(DIRECTORY "${avatar_root}/${pack}/Generated"
          DESTINATION "assets/personalities/${pack}" COMPONENT Runtime)
endforeach()
