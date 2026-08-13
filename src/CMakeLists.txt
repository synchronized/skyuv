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
