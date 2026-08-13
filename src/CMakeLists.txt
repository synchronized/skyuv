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

if(UNIX)
  target_compile_definitions(skyuv_socket_core PRIVATE SKYUV_SOCKET_EXTERNAL_FD=1)
else()
  target_compile_definitions(skyuv_socket_core PRIVATE SKYUV_SOCKET_EXTERNAL_FD=0)
endif()

add_library(skyuv_socket_server STATIC socket/socket_server_compat.c)
add_library(skyuv::socket_server ALIAS skyuv_socket_server)
target_include_directories(
  skyuv_socket_server
  PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/socket"
    "${PROJECT_SOURCE_DIR}/include"
    "${PROJECT_SOURCE_DIR}/3rd/skynet/skynet-src"
)
add_library(skyuv_socket_info STATIC socket/socket_info_compat.c)
target_include_directories(
  skyuv_socket_info
  PRIVATE "${PROJECT_SOURCE_DIR}/3rd/skynet/skynet-src"
)
skyuv_target_compile_warnings(skyuv_socket_info)

target_link_libraries(
  skyuv_socket_server
  PUBLIC skyuv::socket_core
  PRIVATE skyuv_socket_info skyuv::libuv
)
skyuv_target_compile_warnings(skyuv_socket_server)

set(SKYUV_SKYNET_DAEMON_SOURCE compat/skynet/start/skyuv_daemon.c)
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

set(SKYUV_PORTABLE_SKYNET_ROOT "${PROJECT_SOURCE_DIR}/3rd/skynet")
  set(SKYUV_PORTABLE_SKYNET_SOURCE_DIR "${SKYUV_PORTABLE_SKYNET_ROOT}/skynet-src")
  set(SKYUV_PORTABLE_SERVICE_DIR "${PROJECT_BINARY_DIR}/3rd/skynet-patched/service-src")
  set(SKYUV_PORTABLE_LUALIB_SOURCE_DIR "${PROJECT_BINARY_DIR}/3rd/skynet-patched/lualib-src")

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
        LIBRARY_OUTPUT_DIRECTORY "${PROJECT_BINARY_DIR}/3rd/portable/cservice"
        RUNTIME_OUTPUT_DIRECTORY "${PROJECT_BINARY_DIR}/3rd/portable/cservice"
        OUTPUT_NAME "${service_name}"
        PREFIX ""
        WINDOWS_EXPORT_ALL_SYMBOLS TRUE
    )
    foreach(config IN ITEMS DEBUG RELEASE RELWITHDEBINFO MINSIZEREL)
      set_target_properties(
        "skyuv_portable_service_${service_name}"
        PROPERTIES
          "LIBRARY_OUTPUT_DIRECTORY_${config}" "${PROJECT_BINARY_DIR}/3rd/portable/cservice"
          "RUNTIME_OUTPUT_DIRECTORY_${config}" "${PROJECT_BINARY_DIR}/3rd/portable/cservice"
      )
    endforeach()
    target_include_directories(
      "skyuv_portable_service_${service_name}"
      PRIVATE
        "${SKYUV_PORTABLE_SKYNET_SOURCE_DIR}"
        "${SKYUV_PORTABLE_SKYNET_ROOT}/3rd/lua"
    )
    target_compile_options(
      "skyuv_portable_service_${service_name}"
      PRIVATE "$<$<COMPILE_LANG_AND_ID:C,MSVC>:/FIstdlib.h>"
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
      "$<$<COMPILE_LANG_AND_ID:C,MSVC>:/FI${PROJECT_SOURCE_DIR}/src/compat/skynet/atomic.h>"
      "$<$<NOT:$<COMPILE_LANG_AND_ID:C,MSVC>>:-include${PROJECT_SOURCE_DIR}/src/compat/skynet/atomic.h>"
      "$<$<COMPILE_LANG_AND_ID:C,MSVC>:/FI${PROJECT_SOURCE_DIR}/src/compat/skynet/skyuv_time.h>"
      "$<$<NOT:$<COMPILE_LANG_AND_ID:C,MSVC>>:-include${PROJECT_SOURCE_DIR}/src/compat/skynet/skyuv_time.h>"
  )
  target_link_libraries(skyuv_portable_service_snlua PRIVATE skyuv::platform)
  target_include_directories(
    skyuv_portable_service_harbor
    PRIVATE "${PROJECT_SOURCE_DIR}/src/compat/skynet/start"
  )
  target_link_libraries(skyuv_portable_service_harbor PRIVATE skyuv::libuv)

  add_library(
    skyuv_portable_lua_module_client
    MODULE
      "${SKYUV_PORTABLE_LUALIB_SOURCE_DIR}/lua-clientsocket.c"
  )
  set_target_properties(
    skyuv_portable_lua_module_client
    PROPERTIES
      C_EXTENSIONS TRUE
      LIBRARY_OUTPUT_DIRECTORY "${PROJECT_BINARY_DIR}/3rd/portable/luaclib/client"
      RUNTIME_OUTPUT_DIRECTORY "${PROJECT_BINARY_DIR}/3rd/portable/luaclib/client"
      OUTPUT_NAME socket
      PREFIX ""
      WINDOWS_EXPORT_ALL_SYMBOLS TRUE
  )
  foreach(config IN ITEMS DEBUG RELEASE RELWITHDEBINFO MINSIZEREL)
    set_target_properties(
      skyuv_portable_lua_module_client
      PROPERTIES
        "LIBRARY_OUTPUT_DIRECTORY_${config}" "${PROJECT_BINARY_DIR}/3rd/portable/luaclib/client"
        "RUNTIME_OUTPUT_DIRECTORY_${config}" "${PROJECT_BINARY_DIR}/3rd/portable/luaclib/client"
    )
  endforeach()
  target_include_directories(
    skyuv_portable_lua_module_client
    PRIVATE
      "${PROJECT_SOURCE_DIR}/src/compat/skynet/start"
      "${SKYUV_PORTABLE_SKYNET_ROOT}/3rd/lua"
  )
  target_link_libraries(
    skyuv_portable_lua_module_client PRIVATE skyuv::lua skyuv::platform skyuv::libuv
  )

  add_library(
    skyuv_portable_lua_module_bson
    MODULE "${SKYUV_PORTABLE_LUALIB_SOURCE_DIR}/lua-bson.c"
  )
  set_target_properties(
    skyuv_portable_lua_module_bson
    PROPERTIES
      C_EXTENSIONS TRUE
      LIBRARY_OUTPUT_DIRECTORY "${PROJECT_BINARY_DIR}/3rd/portable/luaclib"
      RUNTIME_OUTPUT_DIRECTORY "${PROJECT_BINARY_DIR}/3rd/portable/luaclib"
      OUTPUT_NAME bson
      PREFIX ""
      WINDOWS_EXPORT_ALL_SYMBOLS TRUE
  )
  foreach(config IN ITEMS DEBUG RELEASE RELWITHDEBINFO MINSIZEREL)
    set_target_properties(
      skyuv_portable_lua_module_bson
      PROPERTIES
        "LIBRARY_OUTPUT_DIRECTORY_${config}" "${PROJECT_BINARY_DIR}/3rd/portable/luaclib"
        "RUNTIME_OUTPUT_DIRECTORY_${config}" "${PROJECT_BINARY_DIR}/3rd/portable/luaclib"
    )
  endforeach()
  target_include_directories(
    skyuv_portable_lua_module_bson
    PRIVATE
      "${PROJECT_SOURCE_DIR}/src/compat/skynet/start"
      "${PROJECT_SOURCE_DIR}/src/compat/skynet"
      "${SKYUV_PORTABLE_SKYNET_ROOT}/3rd/lua"
  )
  target_compile_options(
    skyuv_portable_lua_module_bson
    PRIVATE
      "$<$<COMPILE_LANG_AND_ID:C,MSVC>:/FI${PROJECT_SOURCE_DIR}/src/compat/skynet/atomic.h>"
      "$<$<NOT:$<COMPILE_LANG_AND_ID:C,MSVC>>:-include${PROJECT_SOURCE_DIR}/src/compat/skynet/atomic.h>"
  )
  target_link_libraries(
    skyuv_portable_lua_module_bson PRIVATE skyuv::lua skyuv::platform skyuv::libuv
  )

  set(
    SKYUV_PORTABLE_LUA_MODULE_SOURCES
    "${SKYUV_PORTABLE_LUALIB_SOURCE_DIR}/lua-skynet.c"
    "${SKYUV_PORTABLE_LUALIB_SOURCE_DIR}/lua-seri.c"
    "${SKYUV_PORTABLE_LUALIB_SOURCE_DIR}/lua-socket.c"
  )
  add_library(skyuv_portable_lua_module_skynet MODULE ${SKYUV_PORTABLE_LUA_MODULE_SOURCES})
  set_target_properties(
    skyuv_portable_lua_module_skynet
    PROPERTIES
      C_EXTENSIONS TRUE
      LIBRARY_OUTPUT_DIRECTORY "${PROJECT_BINARY_DIR}/3rd/portable/luaclib"
      RUNTIME_OUTPUT_DIRECTORY "${PROJECT_BINARY_DIR}/3rd/portable/luaclib"
      OUTPUT_NAME skynet
      PREFIX ""
      WINDOWS_EXPORT_ALL_SYMBOLS TRUE
  )
  foreach(config IN ITEMS DEBUG RELEASE RELWITHDEBINFO MINSIZEREL)
    set_target_properties(
      skyuv_portable_lua_module_skynet
      PROPERTIES
        "LIBRARY_OUTPUT_DIRECTORY_${config}" "${PROJECT_BINARY_DIR}/3rd/portable/luaclib"
        "RUNTIME_OUTPUT_DIRECTORY_${config}" "${PROJECT_BINARY_DIR}/3rd/portable/luaclib"
    )
  endforeach()
  target_include_directories(
    skyuv_portable_lua_module_skynet
    PRIVATE
      "${SKYUV_PORTABLE_SKYNET_SOURCE_DIR}"
      "${SKYUV_PORTABLE_SKYNET_ROOT}/3rd/lua"
      "${SKYUV_PORTABLE_SERVICE_DIR}"
      "${SKYUV_PORTABLE_LUALIB_SOURCE_DIR}"
  )
  target_compile_options(
    skyuv_portable_lua_module_skynet
    PRIVATE "$<$<COMPILE_LANG_AND_ID:C,MSVC>:/FIstdlib.h>"
  )
  set_source_files_properties(
    "${SKYUV_PORTABLE_LUALIB_SOURCE_DIR}/lua-skynet.c"
    PROPERTIES COMPILE_OPTIONS
      "$<$<COMPILE_LANG_AND_ID:C,MSVC>:/FI${PROJECT_SOURCE_DIR}/src/compat/skynet/skyuv_time.h>;$<$<NOT:$<COMPILE_LANG_AND_ID:C,MSVC>>:-include${PROJECT_SOURCE_DIR}/src/compat/skynet/skyuv_time.h>"
  )
  target_link_libraries(
    skyuv_portable_lua_module_skynet
    PRIVATE
      skyuv_skynet_portable
      skyuv_socket_info
      skyuv::allocator
      skyuv::lua
      skyuv::platform
  )

  file(TO_CMAKE_PATH "${SKYUV_PORTABLE_SKYNET_ROOT}" SKYUV_PORTABLE_SOURCE_DIR)
  file(TO_CMAKE_PATH "${PROJECT_BINARY_DIR}/3rd/portable" SKYUV_PORTABLE_BINARY_DIR)
  set(SKYUV_PORTABLE_MODULE_SUFFIX "${CMAKE_SHARED_MODULE_SUFFIX}")
  file(TO_CMAKE_PATH "${PROJECT_SOURCE_DIR}/examples" SKYUV_EXAMPLE_SOURCE_DIR)
  set(SKYUV_PORTABLE_START_SERVICE skyuv_smoke)
  configure_file(
    "${PROJECT_SOURCE_DIR}/examples/skyuv-portable-smoke.conf.in"
    "${PROJECT_BINARY_DIR}/3rd/skyuv-portable-smoke.conf"
    @ONLY
  )
  set(SKYUV_PORTABLE_START_SERVICE skyuv_echo)
  configure_file(
    "${PROJECT_SOURCE_DIR}/examples/skyuv-portable-smoke.conf.in"
    "${PROJECT_BINARY_DIR}/3rd/skyuv-portable-echo.conf"
    @ONLY
  )
  set(SKYUV_PORTABLE_START_SERVICE skyuv_tcp_events)
  configure_file(
    "${PROJECT_SOURCE_DIR}/examples/skyuv-portable-smoke.conf.in"
    "${PROJECT_BINARY_DIR}/3rd/skyuv-portable-tcp-events.conf"
    @ONLY
  )
  set(SKYUV_PORTABLE_START_SERVICE skyuv_socket_controls)
  configure_file(
    "${PROJECT_SOURCE_DIR}/examples/skyuv-portable-smoke.conf.in"
    "${PROJECT_BINARY_DIR}/3rd/skyuv-portable-socket-controls.conf"
    @ONLY
  )
  set(SKYUV_PORTABLE_START_SERVICE skyuv_udp_events)
  configure_file(
    "${PROJECT_SOURCE_DIR}/examples/skyuv-portable-smoke.conf.in"
    "${PROJECT_BINARY_DIR}/3rd/skyuv-portable-udp-events.conf"
    @ONLY
  )
  set(SKYUV_PORTABLE_START_SERVICE skyuv_client_module)
  configure_file(
    "${PROJECT_SOURCE_DIR}/examples/skyuv-portable-smoke.conf.in"
    "${PROJECT_BINARY_DIR}/3rd/skyuv-portable-client-module.conf"
    @ONLY
  )
  set(SKYUV_PORTABLE_START_SERVICE skyuv_client_socket)
  configure_file(
    "${PROJECT_SOURCE_DIR}/examples/skyuv-portable-smoke.conf.in"
    "${PROJECT_BINARY_DIR}/3rd/skyuv-portable-client-socket.conf"
    @ONLY
  )
  set(SKYUV_PORTABLE_START_SERVICE skyuv_client_stdin)
  configure_file(
    "${PROJECT_SOURCE_DIR}/examples/skyuv-portable-smoke.conf.in"
    "${PROJECT_BINARY_DIR}/3rd/skyuv-portable-client-stdin.conf"
    @ONLY
  )
  set(SKYUV_PORTABLE_START_SERVICE skyuv_bson)
  configure_file(
    "${PROJECT_SOURCE_DIR}/examples/skyuv-portable-smoke.conf.in"
    "${PROJECT_BINARY_DIR}/3rd/skyuv-portable-bson.conf"
    @ONLY
  )
  if(WIN32 AND BUILD_TESTING)
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
    add_test(
      NAME skynet.portable.echo
      COMMAND
        pwsh -NoProfile -File "${PROJECT_SOURCE_DIR}/scripts/portable-echo.ps1"
        -Executable "$<TARGET_FILE:skyuv_skynet_portable>"
        -Config "3rd/skyuv-portable-echo.conf"
    )
    add_test(
      NAME skynet.portable.client_module
      COMMAND
        pwsh -NoProfile -File "${PROJECT_SOURCE_DIR}/scripts/portable-smoke.ps1"
        -Executable "$<TARGET_FILE:skyuv_skynet_portable>"
        -Config "3rd/skyuv-portable-client-module.conf"
    )
    set_tests_properties(
      skynet.portable.client_module
      PROPERTIES
        TIMEOUT 10
        WORKING_DIRECTORY "${PROJECT_BINARY_DIR}"
    )
    add_test(
      NAME skynet.portable.client_socket
      COMMAND
        pwsh -NoProfile -File "${PROJECT_SOURCE_DIR}/scripts/portable-client-socket.ps1"
        -Executable "$<TARGET_FILE:skyuv_skynet_portable>"
        -Config "3rd/skyuv-portable-client-socket.conf"
    )
    set_tests_properties(
      skynet.portable.client_socket
      PROPERTIES
        TIMEOUT 15
        WORKING_DIRECTORY "${PROJECT_BINARY_DIR}"
    )
    add_test(
      NAME skynet.portable.client_stdin
      COMMAND
        pwsh -NoProfile -File "${PROJECT_SOURCE_DIR}/scripts/portable-client-stdin.ps1"
        -Executable "$<TARGET_FILE:skyuv_skynet_portable>"
        -Config "3rd/skyuv-portable-client-stdin.conf"
    )
    set_tests_properties(
      skynet.portable.client_stdin
      PROPERTIES
        TIMEOUT 15
        WORKING_DIRECTORY "${PROJECT_BINARY_DIR}"
    )
    add_test(
      NAME skynet.portable.bson
      COMMAND
        pwsh -NoProfile -File "${PROJECT_SOURCE_DIR}/scripts/portable-smoke.ps1"
        -Executable "$<TARGET_FILE:skyuv_skynet_portable>"
        -Config "3rd/skyuv-portable-bson.conf"
    )
    set_tests_properties(
      skynet.portable.bson
      PROPERTIES
        TIMEOUT 10
        WORKING_DIRECTORY "${PROJECT_BINARY_DIR}"
    )
    set_tests_properties(
      skynet.portable.echo
      PROPERTIES
        TIMEOUT 15
        WORKING_DIRECTORY "${PROJECT_BINARY_DIR}"
    )
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
  target_compile_definitions(skyuv_socket_server PRIVATE _POSIX_C_SOURCE=200112L)
endif()
skyuv_target_compile_warnings(skyuv_platform)
