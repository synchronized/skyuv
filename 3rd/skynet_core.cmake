if(NOT CMAKE_SYSTEM_NAME STREQUAL "Linux")
  return()
endif()

find_package(Threads REQUIRED)

set(SKYUV_SKYNET_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/skynet/skynet-src")
set(
  SKYUV_SKYNET_CORE_SOURCES
  "${SKYUV_SKYNET_SOURCE_DIR}/skynet_main.c"
  "${SKYUV_SKYNET_SOURCE_DIR}/skynet_handle.c"
  "${SKYUV_SKYNET_SOURCE_DIR}/skynet_module.c"
  "${SKYUV_SKYNET_SOURCE_DIR}/skynet_mq.c"
  "${SKYUV_SKYNET_SOURCE_DIR}/skynet_server.c"
  "${SKYUV_SKYNET_SOURCE_DIR}/skynet_start.c"
  "${SKYUV_SKYNET_SOURCE_DIR}/skynet_timer.c"
  "${SKYUV_SKYNET_SOURCE_DIR}/skynet_error.c"
  "${SKYUV_SKYNET_SOURCE_DIR}/skynet_harbor.c"
  "${SKYUV_SKYNET_SOURCE_DIR}/skynet_env.c"
  "${SKYUV_SKYNET_SOURCE_DIR}/skynet_monitor.c"
  "${SKYUV_SKYNET_SOURCE_DIR}/skynet_socket.c"
  "${SKYUV_SKYNET_SOURCE_DIR}/socket_server.c"
  "${SKYUV_SKYNET_SOURCE_DIR}/mem_info.c"
  "${SKYUV_SKYNET_SOURCE_DIR}/malloc_hook.c"
  "${SKYUV_SKYNET_SOURCE_DIR}/skynet_daemon.c"
  "${SKYUV_SKYNET_SOURCE_DIR}/skynet_log.c"
)

# 此目标用于建立未经平台替换的 Linux 行为基线，不修改第三方源码。
add_executable(skyuv_skynet ${SKYUV_SKYNET_CORE_SOURCES})
set_source_files_properties(
  "${SKYUV_SKYNET_SOURCE_DIR}/skynet_timer.c"
  PROPERTIES
    COMPILE_OPTIONS "-include${PROJECT_SOURCE_DIR}/src/compat/skynet/skyuv_time.h"
)
set_target_properties(
  skyuv_skynet
  PROPERTIES
    C_EXTENSIONS TRUE
    ENABLE_EXPORTS TRUE
    OUTPUT_NAME skynet
)
set(SKYUV_SKYNET_COMPAT_DIR "${PROJECT_SOURCE_DIR}/src/compat/skynet")
target_include_directories(
  skyuv_skynet
  PRIVATE
    "${SKYUV_SKYNET_COMPAT_DIR}"
    "${SKYUV_SKYNET_SOURCE_DIR}"
)
target_compile_options(
  skyuv_skynet
  PRIVATE
    "-include${SKYUV_SKYNET_COMPAT_DIR}/atomic.h"
    "-include${SKYUV_SKYNET_COMPAT_DIR}/spinlock.h"
)
target_link_libraries(
  skyuv_skynet
  PRIVATE
    skyuv::lua
    skyuv::platform
    skyuv::allocator
    Threads::Threads
    ${CMAKE_DL_LIBS}
    m
    rt
)

set(SKYUV_SKYNET_SERVICE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/skynet/service-src")
set(SKYUV_SKYNET_CSERVICE_OUTPUT_DIR "${CMAKE_CURRENT_BINARY_DIR}/cservice")

function(skyuv_add_skynet_service service_name)
  add_library(
    "skyuv_service_${service_name}"
    MODULE
    "${SKYUV_SKYNET_SERVICE_DIR}/service_${service_name}.c"
  )
  set_target_properties(
    "skyuv_service_${service_name}"
    PROPERTIES
      C_EXTENSIONS TRUE
      LIBRARY_OUTPUT_DIRECTORY "${SKYUV_SKYNET_CSERVICE_OUTPUT_DIR}"
      OUTPUT_NAME "${service_name}"
      PREFIX ""
  )
  target_include_directories(
    "skyuv_service_${service_name}"
    PRIVATE
      "${SKYUV_SKYNET_SOURCE_DIR}"
      "${CMAKE_CURRENT_SOURCE_DIR}/skynet/3rd/lua"
  )
endfunction()

# 最小启动链路首先需要 Lua 服务容器和日志服务。
skyuv_add_skynet_service(snlua)
skyuv_add_skynet_service(logger)
skyuv_add_skynet_service(harbor)
set_source_files_properties(
  "${SKYUV_SKYNET_SERVICE_DIR}/service_snlua.c"
  PROPERTIES
    COMPILE_OPTIONS "-include${PROJECT_SOURCE_DIR}/src/compat/skynet/skyuv_time.h"
)
target_link_libraries(skyuv_service_snlua PRIVATE skyuv::platform)

set(SKYUV_SKYNET_LUALIB_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/skynet/lualib-src")
set(
  SKYUV_SKYNET_LUA_MODULE_SOURCES
  "${SKYUV_SKYNET_LUALIB_SOURCE_DIR}/lua-skynet.c"
  "${SKYUV_SKYNET_LUALIB_SOURCE_DIR}/lua-seri.c"
  "${SKYUV_SKYNET_LUALIB_SOURCE_DIR}/lua-socket.c"
  "${SKYUV_SKYNET_LUALIB_SOURCE_DIR}/lua-mongo.c"
  "${SKYUV_SKYNET_LUALIB_SOURCE_DIR}/lua-netpack.c"
  "${SKYUV_SKYNET_LUALIB_SOURCE_DIR}/lua-memory.c"
  "${SKYUV_SKYNET_LUALIB_SOURCE_DIR}/lua-multicast.c"
  "${SKYUV_SKYNET_LUALIB_SOURCE_DIR}/lua-cluster.c"
  "${SKYUV_SKYNET_LUALIB_SOURCE_DIR}/lua-crypt.c"
  "${SKYUV_SKYNET_LUALIB_SOURCE_DIR}/lsha1.c"
  "${SKYUV_SKYNET_LUALIB_SOURCE_DIR}/lua-sharedata.c"
  "${SKYUV_SKYNET_LUALIB_SOURCE_DIR}/lua-stm.c"
  "${SKYUV_SKYNET_LUALIB_SOURCE_DIR}/lua-debugchannel.c"
  "${SKYUV_SKYNET_LUALIB_SOURCE_DIR}/lua-datasheet.c"
  "${SKYUV_SKYNET_LUALIB_SOURCE_DIR}/lua-sharetable.c"
)

add_library(skyuv_lua_module_skynet MODULE ${SKYUV_SKYNET_LUA_MODULE_SOURCES})
set_source_files_properties(
  "${SKYUV_SKYNET_LUALIB_SOURCE_DIR}/lua-skynet.c"
  PROPERTIES
    COMPILE_OPTIONS "-include${PROJECT_SOURCE_DIR}/src/compat/skynet/skyuv_time.h"
)
set_target_properties(
  skyuv_lua_module_skynet
  PROPERTIES
    C_EXTENSIONS TRUE
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/luaclib"
    OUTPUT_NAME skynet
    PREFIX ""
)
target_include_directories(
  skyuv_lua_module_skynet
  PRIVATE
    "${SKYUV_SKYNET_SOURCE_DIR}"
    "${CMAKE_CURRENT_SOURCE_DIR}/skynet/3rd/lua"
    "${SKYUV_SKYNET_SERVICE_DIR}"
    "${SKYUV_SKYNET_LUALIB_SOURCE_DIR}"
)
target_link_libraries(skyuv_lua_module_skynet PRIVATE skyuv::platform)

file(TO_CMAKE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/skynet" SKYUV_SKYNET_RUNTIME_SOURCE_DIR)
file(TO_CMAKE_PATH "${CMAKE_SOURCE_DIR}/examples" SKYUV_EXAMPLE_SOURCE_DIR)
file(TO_CMAKE_PATH "${CMAKE_CURRENT_BINARY_DIR}" SKYUV_SKYNET_RUNTIME_BINARY_DIR)
set(SKYUV_SKYNET_START_SERVICE skyuv_smoke)
configure_file(
  "${CMAKE_SOURCE_DIR}/examples/skyuv-smoke.conf.in"
  "${CMAKE_CURRENT_BINARY_DIR}/skyuv-smoke.conf"
  @ONLY
)
set(SKYUV_SKYNET_START_SERVICE skyuv_echo)
configure_file(
  "${CMAKE_SOURCE_DIR}/examples/skyuv-smoke.conf.in"
  "${CMAKE_CURRENT_BINARY_DIR}/skyuv-echo.conf"
  @ONLY
)
set(SKYUV_SKYNET_START_SERVICE skyuv_baseline)
configure_file(
  "${CMAKE_SOURCE_DIR}/examples/skyuv-smoke.conf.in"
  "${CMAKE_CURRENT_BINARY_DIR}/skyuv-baseline.conf"
  @ONLY
)
