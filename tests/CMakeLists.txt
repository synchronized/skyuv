if(NOT SKYUV_BUILD_TESTS OR NOT BUILD_TESTING)
  return()
endif()

add_executable(skyuv_test_allocator test_allocator.c)
target_link_libraries(skyuv_test_allocator PRIVATE skyuv::allocator cmocka::cmocka)
skyuv_target_compile_warnings(skyuv_test_allocator)
add_test(NAME allocator.basic COMMAND skyuv_test_allocator)

add_executable(skyuv_test_thread test_thread.c)
target_link_libraries(skyuv_test_thread PRIVATE skyuv::platform cmocka::cmocka)
skyuv_target_compile_warnings(skyuv_test_thread)
add_test(NAME platform.thread COMMAND skyuv_test_thread)

add_executable(skyuv_test_time test_time.c)
target_link_libraries(skyuv_test_time PRIVATE skyuv::platform cmocka::cmocka)
skyuv_target_compile_warnings(skyuv_test_time)
add_test(NAME platform.time COMMAND skyuv_test_time)

add_library(skyuv_test_dynamic_module SHARED dynamic_module.c)
set_target_properties(skyuv_test_dynamic_module PROPERTIES WINDOWS_EXPORT_ALL_SYMBOLS ON)
skyuv_target_compile_warnings(skyuv_test_dynamic_module)

add_executable(skyuv_test_dynamic test_dynamic.c)
target_compile_definitions(
  skyuv_test_dynamic
  PRIVATE SKYUV_TEST_MODULE_PATH="$<TARGET_FILE:skyuv_test_dynamic_module>"
)
target_link_libraries(skyuv_test_dynamic PRIVATE skyuv::platform cmocka::cmocka)
add_dependencies(skyuv_test_dynamic skyuv_test_dynamic_module)
skyuv_target_compile_warnings(skyuv_test_dynamic)
add_test(NAME platform.dynamic COMMAND skyuv_test_dynamic)

add_executable(skyuv_test_atomic test_atomic.c)
target_link_libraries(skyuv_test_atomic PRIVATE skyuv::platform cmocka::cmocka)
skyuv_target_compile_warnings(skyuv_test_atomic)
add_test(NAME platform.atomic COMMAND skyuv_test_atomic)

if(TARGET skyuv_lua)
  add_executable(skyuv_test_lua test_lua.c)
  target_link_libraries(skyuv_test_lua PRIVATE skyuv::lua cmocka::cmocka)
  skyuv_target_compile_warnings(skyuv_test_lua)
  add_test(NAME lua.basic COMMAND skyuv_test_lua)
endif()
