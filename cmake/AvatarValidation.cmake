# The unmodified upstream validation executable expects its original registry
# layout. Assemble that fixture in the build tree from the single owned copies;
# the application/deployment uses independent per-personality registries instead.
if(BUILD_TESTING AND NOT AUDIOHOLE_MODEL_ONLY)
  set(AUDIOHOLE_AVATAR_BUILD_TESTS ON CACHE BOOL "Standalone avatar validation" FORCE)
  set(AUDIOHOLE_AVATAR_TEST_ROOT "${CMAKE_BINARY_DIR}/avatar-test-assets" CACHE PATH "Avatar test fixtures" FORCE)
  file(MAKE_DIRECTORY "${AUDIOHOLE_AVATAR_TEST_ROOT}")
  configure_file("${PROJECT_SOURCE_DIR}/assets/personalities/registry.json"
    "${AUDIOHOLE_AVATAR_TEST_ROOT}/registry.json" COPYONLY)
  set(avatar_ids default uwu vanilla)
  set(avatar_names Norm UwU Vanilla)
  foreach(index RANGE 0 2)
    list(GET avatar_ids ${index} identity)
    list(GET avatar_names ${index} name)
    file(COPY "${PROJECT_SOURCE_DIR}/personalities/${identity}/avatar/pack/"
      DESTINATION "${AUDIOHOLE_AVATAR_TEST_ROOT}/${name}")
  endforeach()
endif()
