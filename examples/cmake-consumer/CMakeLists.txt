cmake_minimum_required(VERSION 3.23)

project(skyuv_cmake_consumer LANGUAGES C)

include(CTest)

# 必须与 skyuv 及其静态依赖使用相同的 MSVC CRT，避免模块边界出现
# 不同 CRT 的内存分配和释放冲突。
if(MSVC)
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
    C_EXTENSIONS TRUE
    OUTPUT_NAME skyuv_consumer
    PREFIX ""
)
target_link_libraries(skyuv_consumer PRIVATE skyuv::lua skyuv::platform)

add_executable(skyuv_consumer_smoke skyuv_consumer_host.c skyuv_consumer.c)
target_link_libraries(skyuv_consumer_smoke PRIVATE skyuv::lua skyuv::platform)

if(BUILD_TESTING)
  add_test(NAME skyuv.cmake_consumer.require COMMAND skyuv_consumer_smoke)
endif()
