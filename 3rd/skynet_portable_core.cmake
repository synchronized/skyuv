find_package(Git REQUIRED)

set(SKYUV_SKYNET_PATCH_ROOT "${CMAKE_CURRENT_BINARY_DIR}/skynet-patched")
set(SKYUV_SKYNET_SOURCE_DIR "${SKYUV_SKYNET_PATCH_ROOT}/skynet-src")
set(SKYUV_SKYNET_COMPAT_DIR "${PROJECT_SOURCE_DIR}/src/compat/skynet")
file(RELATIVE_PATH SKYUV_SKYNET_PATCH_RELATIVE "${PROJECT_SOURCE_DIR}" "${SKYUV_SKYNET_PATCH_ROOT}")
file(REMOVE_RECURSE "${SKYUV_SKYNET_PATCH_ROOT}")
file(MAKE_DIRECTORY "${SKYUV_SKYNET_PATCH_ROOT}")
file(COPY "${CMAKE_CURRENT_SOURCE_DIR}/skynet/skynet-src" DESTINATION "${SKYUV_SKYNET_PATCH_ROOT}")
file(COPY "${CMAKE_CURRENT_SOURCE_DIR}/skynet/service-src" DESTINATION "${SKYUV_SKYNET_PATCH_ROOT}")
execute_process(
  COMMAND
    "${GIT_EXECUTABLE}" apply "--directory=${SKYUV_SKYNET_PATCH_RELATIVE}"
    "${PROJECT_SOURCE_DIR}/patches/skynet/0001-Fix-ownership-of-variable-temporary-buffers.patch"
  WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}"
  RESULT_VARIABLE SKYUV_SKYNET_PATCH_RESULT
  ERROR_VARIABLE SKYUV_SKYNET_PATCH_ERROR
)
if(NOT SKYUV_SKYNET_PATCH_RESULT EQUAL 0)
  message(FATAL_ERROR "应用 Skynet 可变缓冲区所有权补丁失败：${SKYUV_SKYNET_PATCH_ERROR}")
endif()
execute_process(
  COMMAND
    "${GIT_EXECUTABLE}" apply "--directory=${SKYUV_SKYNET_PATCH_RELATIVE}"
    "${PROJECT_SOURCE_DIR}/patches/skynet/0002-Replace-start-VLAs-with-heap-buffers.patch"
  WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}"
  RESULT_VARIABLE SKYUV_SKYNET_START_PATCH_RESULT
  ERROR_VARIABLE SKYUV_SKYNET_START_PATCH_ERROR
)
if(NOT SKYUV_SKYNET_START_PATCH_RESULT EQUAL 0)
  message(FATAL_ERROR "应用 Skynet 启动 VLA 补丁失败：${SKYUV_SKYNET_START_PATCH_ERROR}")
endif()
execute_process(
  COMMAND
    "${GIT_EXECUTABLE}" apply "--directory=${SKYUV_SKYNET_PATCH_RELATIVE}"
    "${PROJECT_SOURCE_DIR}/patches/skynet/0003-Replace-harbor-VLAs-with-heap-buffers.patch"
  WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}"
  RESULT_VARIABLE SKYUV_SKYNET_HARBOR_PATCH_RESULT
  ERROR_VARIABLE SKYUV_SKYNET_HARBOR_PATCH_ERROR
)
if(NOT SKYUV_SKYNET_HARBOR_PATCH_RESULT EQUAL 0)
  message(FATAL_ERROR "应用 Skynet harbor VLA 补丁失败：${SKYUV_SKYNET_HARBOR_PATCH_ERROR}")
endif()
file(READ "${SKYUV_SKYNET_SOURCE_DIR}/skynet_module.c" SKYUV_SKYNET_MODULE_SOURCE)
if(NOT SKYUV_SKYNET_MODULE_SOURCE MATCHES "skynet_malloc\\(sz\\)")
  message(FATAL_ERROR "Skynet 可变缓冲区所有权补丁未生效")
endif()

set(
  SKYUV_SKYNET_PORTABLE_CORE_SOURCES
  "${SKYUV_SKYNET_SOURCE_DIR}/skynet_handle.c"
  "${SKYUV_SKYNET_SOURCE_DIR}/skynet_module.c"
  "${SKYUV_SKYNET_SOURCE_DIR}/skynet_mq.c"
  "${SKYUV_SKYNET_SOURCE_DIR}/skynet_server.c"
  "${SKYUV_SKYNET_SOURCE_DIR}/skynet_timer.c"
  "${SKYUV_SKYNET_SOURCE_DIR}/skynet_error.c"
  "${SKYUV_SKYNET_SOURCE_DIR}/skynet_harbor.c"
  "${SKYUV_SKYNET_SOURCE_DIR}/skynet_env.c"
  "${SKYUV_SKYNET_SOURCE_DIR}/skynet_monitor.c"
  "${SKYUV_SKYNET_SOURCE_DIR}/skynet_socket.c"
  "${SKYUV_SKYNET_SOURCE_DIR}/skynet_start.c"
  "${SKYUV_SKYNET_SOURCE_DIR}/skynet_main.c"
  "${SKYUV_SKYNET_SOURCE_DIR}/skynet_log.c"
)

# 只验证已完成平台迁移的 Actor 核心；旧 socket、daemon 和 Unix 入口留在 Linux 基线目标。
add_library(skyuv_skynet_portable_core OBJECT ${SKYUV_SKYNET_PORTABLE_CORE_SOURCES})
set_target_properties(skyuv_skynet_portable_core PROPERTIES C_EXTENSIONS TRUE)
target_include_directories(
  skyuv_skynet_portable_core
  PRIVATE
    "${SKYUV_SKYNET_COMPAT_DIR}/dynamic"
    "${SKYUV_SKYNET_COMPAT_DIR}"
    "${SKYUV_SKYNET_SOURCE_DIR}"
    "${CMAKE_CURRENT_SOURCE_DIR}/skynet/3rd/lua"
)
set_source_files_properties(
  "${SKYUV_SKYNET_SOURCE_DIR}/skynet_start.c"
  "${SKYUV_SKYNET_SOURCE_DIR}/skynet_main.c"
  PROPERTIES
    COMPILE_OPTIONS "-I${SKYUV_SKYNET_COMPAT_DIR}/start"
)
target_compile_options(
  skyuv_skynet_portable_core
  PRIVATE
    "$<$<COMPILE_LANG_AND_ID:C,MSVC>:/FI${SKYUV_SKYNET_COMPAT_DIR}/atomic.h>"
    "$<$<COMPILE_LANG_AND_ID:C,MSVC>:/FI${SKYUV_SKYNET_COMPAT_DIR}/spinlock.h>"
    "$<$<NOT:$<COMPILE_LANG_AND_ID:C,MSVC>>:-include${SKYUV_SKYNET_COMPAT_DIR}/atomic.h>"
    "$<$<NOT:$<COMPILE_LANG_AND_ID:C,MSVC>>:-include${SKYUV_SKYNET_COMPAT_DIR}/spinlock.h>"
)
set_source_files_properties(
  "${SKYUV_SKYNET_SOURCE_DIR}/skynet_timer.c"
  PROPERTIES
    COMPILE_OPTIONS
      "$<$<COMPILE_LANG_AND_ID:C,MSVC>:/FI${SKYUV_SKYNET_COMPAT_DIR}/skyuv_time.h>;$<$<NOT:$<COMPILE_LANG_AND_ID:C,MSVC>>:-include${SKYUV_SKYNET_COMPAT_DIR}/skyuv_time.h>"
)
set_source_files_properties(
  "${SKYUV_SKYNET_SOURCE_DIR}/skynet_server.c"
  PROPERTIES
    COMPILE_OPTIONS
      "-I${SKYUV_SKYNET_COMPAT_DIR}/tls;$<$<COMPILE_LANG_AND_ID:C,MSVC>:/FI${SKYUV_SKYNET_COMPAT_DIR}/skyuv_string.h>"
)
target_link_libraries(
  skyuv_skynet_portable_core
  PRIVATE skyuv::lua skyuv::libuv skyuv::platform skyuv::allocator skyuv::socket_server
)
