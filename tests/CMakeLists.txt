if(NOT SKYUV_BUILD_TESTS OR NOT BUILD_TESTING)
  return()
endif()

if(NOT TARGET skyuv_lua)
  return()
endif()

add_executable(skyuv_test_lua test_lua.c)
target_link_libraries(skyuv_test_lua PRIVATE skyuv::lua cmocka::cmocka)
skyuv_target_compile_warnings(skyuv_test_lua)

add_test(NAME lua.basic COMMAND skyuv_test_lua)
