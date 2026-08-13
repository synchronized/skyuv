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
set_target_properties(
  skyuv_skynet_portable
  PROPERTIES
    ENABLE_EXPORTS TRUE
    WINDOWS_EXPORT_ALL_SYMBOLS TRUE
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

if(WIN32)
  set(SKYUV_PORTABLE_SKYNET_ROOT "${PROJECT_SOURCE_DIR}/3rd/skynet")
  set(SKYUV_PORTABLE_SKYNET_SOURCE_DIR "${SKYUV_PORTABLE_SKYNET_ROOT}/skynet-src")
  set(SKYUV_PORTABLE_SERVICE_DIR "${PROJECT_BINARY_DIR}/3rd/skynet-patched/service-src")
  set(SKYUV_PORTABLE_LUALIB_SOURCE_DIR "${SKYUV_PORTABLE_SKYNET_ROOT}/lualib-src")

  function(skyuv_add_portable_skynet_service service_name)
    add_library(
      "skyuv_portable_service_${service_name}"
      MODULE
      "${SKYUV_PORTABLE_SERVICE_DIR}/service_${service_name}.c"
    )
    set_target_properties(
      "skyuv_portable_service_${service_name}"
      PROPERTIES
        C_EXTENSIONS TRUE
        LIBRARY_OUTPUT_DIRECTORY "${PROJECT_BINARY_DIR}/3rd/cservice"
        RUNTIME_OUTPUT_DIRECTORY "${PROJECT_BINARY_DIR}/3rd/cservice"
        OUTPUT_NAME "${service_name}"
        PREFIX ""
        WINDOWS_EXPORT_ALL_SYMBOLS TRUE
    )
    target_include_directories(
      "skyuv_portable_service_${service_name}"
      PRIVATE
        "${SKYUV_PORTABLE_SKYNET_SOURCE_DIR}"
        "${SKYUV_PORTABLE_SKYNET_ROOT}/3rd/lua"
    )
    target_compile_options(
      "skyuv_portable_service_${service_name}"
      PRIVATE "/FIstdlib.h"
    )
    target_link_libraries(
      "skyuv_portable_service_${service_name}"
      PRIVATE
        skyuv_skynet_portable
        skyuv_skynet_runtime_compat
        skyuv::allocator
        skyuv::lua
    )
  endfunction()

  skyuv_add_portable_skynet_service(snlua)
  skyuv_add_portable_skynet_service(logger)
  skyuv_add_portable_skynet_service(harbor)
  target_compile_options(
    skyuv_portable_service_snlua
    PRIVATE
      "/FI${PROJECT_SOURCE_DIR}/src/compat/skynet/atomic.h"
      "/FI${PROJECT_SOURCE_DIR}/src/compat/skynet/skyuv_time.h"
  )
  target_link_libraries(skyuv_portable_service_snlua PRIVATE skyuv::platform)
  target_include_directories(
    skyuv_portable_service_harbor
    PRIVATE "${PROJECT_SOURCE_DIR}/src/compat/skynet/start"
  )
  target_link_libraries(skyuv_portable_service_harbor PRIVATE skyuv::libuv)

  set(
    SKYUV_PORTABLE_LUA_MODULE_SOURCES
    "${SKYUV_PORTABLE_LUALIB_SOURCE_DIR}/lua-skynet.c"
    "${SKYUV_PORTABLE_LUALIB_SOURCE_DIR}/lua-seri.c"
  )
  add_library(skyuv_portable_lua_module_skynet MODULE ${SKYUV_PORTABLE_LUA_MODULE_SOURCES})
  set_target_properties(
    skyuv_portable_lua_module_skynet
    PROPERTIES
      C_EXTENSIONS TRUE
      LIBRARY_OUTPUT_DIRECTORY "${PROJECT_BINARY_DIR}/3rd/luaclib"
      RUNTIME_OUTPUT_DIRECTORY "${PROJECT_BINARY_DIR}/3rd/luaclib"
      OUTPUT_NAME skynet
      PREFIX ""
      WINDOWS_EXPORT_ALL_SYMBOLS TRUE
  )
  target_include_directories(
    skyuv_portable_lua_module_skynet
    PRIVATE
      "${SKYUV_PORTABLE_SKYNET_SOURCE_DIR}"
      "${SKYUV_PORTABLE_SKYNET_ROOT}/3rd/lua"
      "${SKYUV_PORTABLE_SERVICE_DIR}"
      "${SKYUV_PORTABLE_LUALIB_SOURCE_DIR}"
  )
  target_compile_options(skyuv_portable_lua_module_skynet PRIVATE "/FIstdlib.h")
  set_source_files_properties(
    "${SKYUV_PORTABLE_LUALIB_SOURCE_DIR}/lua-skynet.c"
    PROPERTIES COMPILE_OPTIONS "/FI${PROJECT_SOURCE_DIR}/src/compat/skynet/skyuv_time.h"
  )
  target_link_libraries(
    skyuv_portable_lua_module_skynet
    PRIVATE skyuv_skynet_portable skyuv::allocator skyuv::lua skyuv::platform
  )

  file(TO_CMAKE_PATH "${SKYUV_PORTABLE_SKYNET_ROOT}" SKYUV_PORTABLE_SOURCE_DIR)
  file(TO_CMAKE_PATH "${PROJECT_BINARY_DIR}/3rd" SKYUV_PORTABLE_BINARY_DIR)
  file(TO_CMAKE_PATH "${PROJECT_SOURCE_DIR}/examples" SKYUV_EXAMPLE_SOURCE_DIR)
  configure_file(
    "${PROJECT_SOURCE_DIR}/examples/skyuv-portable-smoke.conf.in"
    "${PROJECT_BINARY_DIR}/3rd/skyuv-portable-smoke.conf"
    @ONLY
  )
  if(BUILD_TESTING)
    add_test(
      NAME skynet.portable.smoke
      COMMAND
        pwsh -NoProfile -File "${PROJECT_SOURCE_DIR}/scripts/portable-smoke.ps1"
        -Executable "$<TARGET_FILE:skyuv_skynet_portable>"
        -Config "3rd/skyuv-portable-smoke.conf"
    )
    set_tests_properties(
      skynet.portable.smoke
      PROPERTIES
        TIMEOUT 10
        WORKING_DIRECTORY "${PROJECT_BINARY_DIR}"
    )
  endif()
endif()

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
