cmake_minimum_required(VERSION 3.23)

project(skyuv_cmake_consumer LANGUAGES C)

include(CTest)
include("${CMAKE_CURRENT_LIST_DIR}/../../cmake/skyuv_warnings.cmake")
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

# 必须与 skyuv 及其静态依赖使用相同的 MSVC CRT，避免模块边界出现
# 不同 CRT 的内存分配和释放冲突。
if(MSVC OR CMAKE_C_SIMULATE_ID STREQUAL "MSVC")
  set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif()

if(DEFINED SKYUV_SOURCE_DIR)
  set(SKYUV_BUILD_TESTS OFF CACHE BOOL "" FORCE)
  add_subdirectory("${SKYUV_SOURCE_DIR}" skyuv)
else()
  find_package(skyuv CONFIG REQUIRED)
endif()

add_library(skyuv_consumer MODULE skyuv_consumer.c)
set_target_properties(
  skyuv_consumer
  PROPERTIES
    C_EXTENSIONS OFF
    WINDOWS_EXPORT_ALL_SYMBOLS TRUE
    OUTPUT_NAME skyuv_consumer
    PREFIX ""
)
target_link_libraries(skyuv_consumer PRIVATE skyuv::lua skyuv::platform)
skyuv_target_compile_warnings(skyuv_consumer)

add_executable(skyuv_consumer_smoke skyuv_consumer_host.c skyuv_consumer.c)
target_link_libraries(skyuv_consumer_smoke PRIVATE skyuv::lua skyuv::platform)
skyuv_target_compile_warnings(skyuv_consumer_smoke)

if(BUILD_TESTING)
  add_test(NAME skyuv.cmake_consumer.static_registration COMMAND skyuv_consumer_smoke)
  if(NOT DEFINED SKYUV_LUA_EXECUTABLE AND TARGET skyuv_lua_interpreter)
    set(SKYUV_LUA_EXECUTABLE "$<TARGET_FILE:skyuv_lua_interpreter>")
  endif()
  if(DEFINED SKYUV_LUA_EXECUTABLE)
    file(GENERATE OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/$<CONFIG>/dynamic-require.lua" CONTENT
"package.cpath = [==[$<TARGET_FILE_DIR:skyuv_consumer>/?$<TARGET_FILE_SUFFIX:skyuv_consumer>]==]
assert(package.loaded.skyuv_consumer == nil)
local module = require('skyuv_consumer')
assert(module.version() == 'skyuv-cmake-consumer')
package.loaded.skyuv_consumer = nil
package.cpath = ''
local ok = pcall(require, 'skyuv_consumer')
assert(not ok, '缺失模块搜索路径被错误接受')
print('SKYUV_DYNAMIC_REQUIRE_OK')
")
    add_test(NAME skyuv.cmake_consumer.dynamic_require COMMAND "${SKYUV_LUA_EXECUTABLE}"
      "${CMAKE_CURRENT_BINARY_DIR}/$<CONFIG>/dynamic-require.lua")
  endif()
endif()
