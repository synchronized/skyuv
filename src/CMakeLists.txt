add_library(skyuv_platform STATIC
  platform/atomic.c
  platform/dynamic.c
  platform/thread.c
  platform/time.c
)
add_library(skyuv::platform ALIAS skyuv_platform)

add_library(skyuv_socket_core STATIC socket/socket_internal.c)
add_library(skyuv::socket_core ALIAS skyuv_socket_core)
target_include_directories(skyuv_socket_core PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/socket")
skyuv_target_compile_warnings(skyuv_socket_core)

add_library(skyuv_socket_server STATIC socket/socket_server_compat.c)
add_library(skyuv::socket_server ALIAS skyuv_socket_server)
target_include_directories(
  skyuv_socket_server
  PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/socket"
    "${PROJECT_SOURCE_DIR}/include"
    "${PROJECT_SOURCE_DIR}/3rd/skynet/skynet-src"
)
target_link_libraries(skyuv_socket_server PUBLIC skyuv::socket_core)
skyuv_target_compile_warnings(skyuv_socket_server)

if(WIN32)
  set(SKYUV_SKYNET_DAEMON_SOURCE compat/skynet/start/skyuv_daemon.c)
else()
  set(SKYUV_SKYNET_DAEMON_SOURCE "${PROJECT_SOURCE_DIR}/3rd/skynet/skynet-src/skynet_daemon.c")
endif()
add_library(skyuv_skynet_runtime_compat STATIC ${SKYUV_SKYNET_DAEMON_SOURCE})
target_include_directories(
  skyuv_skynet_runtime_compat
  PRIVATE "${PROJECT_SOURCE_DIR}/3rd/skynet/skynet-src"
)
skyuv_target_compile_warnings(skyuv_skynet_runtime_compat)

add_executable(
  skyuv_skynet_portable
  $<TARGET_OBJECTS:skyuv_skynet_portable_core>
)
target_link_libraries(
  skyuv_skynet_portable
  PRIVATE
    skyuv::allocator
    skyuv::lua
    skyuv::platform
    skyuv::socket_server
    skyuv_skynet_runtime_compat
)
skyuv_target_compile_warnings(skyuv_skynet_portable)

target_include_directories(
  skyuv_platform
  PUBLIC
    "${PROJECT_SOURCE_DIR}/include"
)
target_link_libraries(skyuv_platform PRIVATE skyuv::libuv)
target_link_libraries(skyuv_socket_core PRIVATE skyuv::platform skyuv::libuv)
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
  target_compile_definitions(skyuv_platform PRIVATE _POSIX_C_SOURCE=200112L)
  target_compile_definitions(skyuv_socket_core PRIVATE _POSIX_C_SOURCE=200112L)
endif()
skyuv_target_compile_warnings(skyuv_platform)
