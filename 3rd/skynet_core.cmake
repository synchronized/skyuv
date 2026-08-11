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
set_target_properties(
  skyuv_skynet
  PROPERTIES
    ENABLE_EXPORTS TRUE
    OUTPUT_NAME skynet
)
target_include_directories(skyuv_skynet PRIVATE "${SKYUV_SKYNET_SOURCE_DIR}")
target_link_libraries(
  skyuv_skynet
  PRIVATE
    skyuv::lua
    skyuv::allocator
    Threads::Threads
    ${CMAKE_DL_LIBS}
    m
    rt
)
