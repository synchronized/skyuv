add_library(skyuv_platform STATIC
  platform/atomic.c
  platform/dynamic.c
  platform/thread.c
  platform/time.c
)
add_library(skyuv::platform ALIAS skyuv_platform)

add_library(skyuv_memory OBJECT memory/memory.c)
add_library(skyuv::memory ALIAS skyuv_memory)
target_include_directories(skyuv_memory PUBLIC "${PROJECT_SOURCE_DIR}/include")
target_link_libraries(skyuv_memory PRIVATE skyuv::allocator)
if(UNIX)
  target_compile_definitions(skyuv_memory PRIVATE _POSIX_C_SOURCE=200112L)
endif()
skyuv_target_compile_warnings(skyuv_memory)

add_library(skyuv_socket_core STATIC socket/socket_internal.c)
add_library(skyuv::socket_core ALIAS skyuv_socket_core)
target_include_directories(
  skyuv_socket_core PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/socket" "${PROJECT_SOURCE_DIR}/include"
)
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

add_library(skyuv_skynet_control STATIC compat/skynet/skyuv_control.c)
set_target_properties(skyuv_skynet_control PROPERTIES POSITION_INDEPENDENT_CODE ON)
target_include_directories(
  skyuv_skynet_control
  PRIVATE "${PROJECT_SOURCE_DIR}/3rd/skynet/skynet-src"
)
skyuv_target_compile_warnings(skyuv_skynet_control)
target_include_directories(
  skyuv_socket_info
  PRIVATE "${PROJECT_SOURCE_DIR}/3rd/skynet/skynet-src" "${PROJECT_SOURCE_DIR}/include"
)
skyuv_target_compile_warnings(skyuv_socket_info)

target_link_libraries(
  skyuv_socket_server
  PUBLIC skyuv::socket_core
	PRIVATE skyuv_socket_info skyuv::libuv
	PRIVATE skyuv_skynet_control
)
skyuv_target_compile_warnings(skyuv_socket_server)

set(
  SKYUV_SKYNET_RUNTIME_COMPAT_SOURCES
  compat/skynet/skyuv_memory.c
  compat/skynet/skyuv_runtime.c
)
if(WIN32)
  list(APPEND SKYUV_SKYNET_RUNTIME_COMPAT_SOURCES compat/skynet/start/skyuv_daemon.c)
else()
  add_library(
    skyuv_skynet_daemon_upstream STATIC
    "${PROJECT_BINARY_DIR}/3rd/skynet-patched/skynet-src/skynet_daemon.c"
  )
  target_include_directories(
    skyuv_skynet_daemon_upstream
    PRIVATE "${PROJECT_SOURCE_DIR}/3rd/skynet/skynet-src"
  )
  if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    target_compile_definitions(skyuv_skynet_daemon_upstream PRIVATE _DEFAULT_SOURCE)
  endif()
endif()
add_library(skyuv_skynet_runtime_compat STATIC ${SKYUV_SKYNET_RUNTIME_COMPAT_SOURCES})
target_include_directories(
  skyuv_skynet_runtime_compat
  PRIVATE
    "${PROJECT_SOURCE_DIR}/include"
    "${PROJECT_SOURCE_DIR}/3rd/skynet/skynet-src"
    "${PROJECT_SOURCE_DIR}/3rd/skynet/3rd/lua"
)
skyuv_target_compile_warnings(skyuv_skynet_runtime_compat)

add_executable(
  skyuv_skynet_portable
  runtime/launcher.c
  $<TARGET_OBJECTS:skyuv_skynet_portable_core>
)
set_target_properties(
  skyuv_skynet_portable
  PROPERTIES
    ENABLE_EXPORTS TRUE
    WINDOWS_EXPORT_ALL_SYMBOLS TRUE
)
if(APPLE)
  set_property(TARGET skyuv_skynet_portable PROPERTY INSTALL_RPATH "@loader_path")
elseif(UNIX)
  set_property(TARGET skyuv_skynet_portable PROPERTY INSTALL_RPATH "\$ORIGIN")
endif()
target_link_libraries(
  skyuv_skynet_portable
  PRIVATE
    skyuv::allocator
    skyuv::memory
    skyuv::lua
    skyuv::libuv
    skyuv::platform
    skyuv::socket_server
    skyuv_skynet_runtime_compat
)
if(NOT WIN32)
  target_link_libraries(skyuv_skynet_portable PRIVATE skyuv_skynet_daemon_upstream)
endif()
skyuv_target_compile_warnings(skyuv_skynet_portable)

set(SKYUV_PORTABLE_SKYNET_ROOT "${PROJECT_SOURCE_DIR}/3rd/skynet")
  set(SKYUV_PORTABLE_SKYNET_SOURCE_DIR "${PROJECT_BINARY_DIR}/3rd/skynet-patched/skynet-src")
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
        "${PROJECT_SOURCE_DIR}/include"
    )
    if(SKYUV_USES_MSVC_FRONTEND)
      skyuv_target_force_include("skyuv_portable_service_${service_name}" stdlib.h)
    endif()
    target_link_libraries(
      "skyuv_portable_service_${service_name}"
      PRIVATE
        skyuv_skynet_portable
        skyuv_skynet_runtime_compat
        skyuv::lua
    )
  endfunction()

  skyuv_add_portable_skynet_service(snlua)
  skyuv_add_portable_skynet_service(logger)
  skyuv_add_portable_skynet_service(harbor)
  skyuv_add_portable_skynet_service(gate)
  if(SKYUV_USES_MSVC_FRONTEND)
    skyuv_target_force_include(
      skyuv_portable_service_gate "${PROJECT_SOURCE_DIR}/src/compat/skynet/skyuv_string.h"
    )
  endif()
  skyuv_target_force_include(
    skyuv_portable_service_snlua "${PROJECT_SOURCE_DIR}/src/compat/skynet/atomic.h"
  )
  skyuv_target_force_include(
    skyuv_portable_service_snlua "${PROJECT_SOURCE_DIR}/src/compat/skynet/skyuv_time.h"
  )
  skyuv_target_force_include(
    skyuv_portable_service_logger "${PROJECT_SOURCE_DIR}/src/compat/skynet/skyuv_runtime.h"
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
      "${SKYUV_PORTABLE_SKYNET_ROOT}/3rd/lua"
  )
  if(WIN32)
    target_include_directories(
      skyuv_portable_lua_module_client
      PRIVATE "${PROJECT_SOURCE_DIR}/src/compat/skynet/start"
    )
  endif()
  target_link_libraries(
    skyuv_portable_lua_module_client PRIVATE skyuv::lua skyuv::platform skyuv::libuv
  )

  add_library(
    skyuv_portable_lua_module_control MODULE compat/skynet/lua-control.c
  )
  set_target_properties(
    skyuv_portable_lua_module_control
    PROPERTIES
      LIBRARY_OUTPUT_DIRECTORY "${PROJECT_BINARY_DIR}/3rd/portable/luaclib/skyuv"
      RUNTIME_OUTPUT_DIRECTORY "${PROJECT_BINARY_DIR}/3rd/portable/luaclib/skyuv"
      OUTPUT_NAME control
      PREFIX ""
      WINDOWS_EXPORT_ALL_SYMBOLS TRUE
  )
  foreach(config IN ITEMS DEBUG RELEASE RELWITHDEBINFO MINSIZEREL)
    set_target_properties(
      skyuv_portable_lua_module_control
      PROPERTIES
        "LIBRARY_OUTPUT_DIRECTORY_${config}" "${PROJECT_BINARY_DIR}/3rd/portable/luaclib/skyuv"
        "RUNTIME_OUTPUT_DIRECTORY_${config}" "${PROJECT_BINARY_DIR}/3rd/portable/luaclib/skyuv"
    )
  endforeach()
  target_include_directories(
    skyuv_portable_lua_module_control
    PRIVATE
      "${PROJECT_SOURCE_DIR}/src/compat/skynet"
      "${PROJECT_SOURCE_DIR}/3rd/skynet/3rd/lua"
  )
  target_link_libraries(
    skyuv_portable_lua_module_control
    PRIVATE skyuv::lua skyuv_skynet_control skyuv_skynet_portable
  )
  skyuv_target_compile_warnings(skyuv_portable_lua_module_control)

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
  skyuv_target_force_include(
    skyuv_portable_lua_module_bson "${PROJECT_SOURCE_DIR}/src/compat/skynet/atomic.h"
  )
  target_link_libraries(
    skyuv_portable_lua_module_bson PRIVATE skyuv::lua skyuv::platform skyuv::libuv
  )

  add_library(
    skyuv_portable_lua_module_sproto
    MODULE
      "${SKYUV_PORTABLE_LUALIB_SOURCE_DIR}/sproto/sproto.c"
      "${SKYUV_PORTABLE_LUALIB_SOURCE_DIR}/sproto/lsproto.c"
  )
  set_target_properties(
    skyuv_portable_lua_module_sproto
    PROPERTIES
      C_EXTENSIONS TRUE
      ARCHIVE_OUTPUT_DIRECTORY "${PROJECT_BINARY_DIR}/3rd/portable/luaclib/sproto"
      LIBRARY_OUTPUT_DIRECTORY "${PROJECT_BINARY_DIR}/3rd/portable/luaclib/sproto"
      RUNTIME_OUTPUT_DIRECTORY "${PROJECT_BINARY_DIR}/3rd/portable/luaclib/sproto"
      OUTPUT_NAME core
      PREFIX ""
      WINDOWS_EXPORT_ALL_SYMBOLS TRUE
  )
  foreach(config IN ITEMS DEBUG RELEASE RELWITHDEBINFO MINSIZEREL)
    set_target_properties(
      skyuv_portable_lua_module_sproto
      PROPERTIES
        "LIBRARY_OUTPUT_DIRECTORY_${config}" "${PROJECT_BINARY_DIR}/3rd/portable/luaclib/sproto"
        "RUNTIME_OUTPUT_DIRECTORY_${config}" "${PROJECT_BINARY_DIR}/3rd/portable/luaclib/sproto"
        "ARCHIVE_OUTPUT_DIRECTORY_${config}" "${PROJECT_BINARY_DIR}/3rd/portable/luaclib/sproto"
    )
  endforeach()
  target_include_directories(
    skyuv_portable_lua_module_sproto
    PRIVATE
      "${SKYUV_PORTABLE_SKYNET_ROOT}/3rd/lua"
      "${SKYUV_PORTABLE_LUALIB_SOURCE_DIR}/sproto"
  )
  target_link_libraries(skyuv_portable_lua_module_sproto PRIVATE skyuv::lua)

  add_library(
    skyuv_portable_lua_module_lpeg
    MODULE
      "${SKYUV_PORTABLE_SKYNET_ROOT}/3rd/lpeg/lpcap.c"
      "${SKYUV_PORTABLE_SKYNET_ROOT}/3rd/lpeg/lpcode.c"
      "${SKYUV_PORTABLE_SKYNET_ROOT}/3rd/lpeg/lpcset.c"
      "${SKYUV_PORTABLE_SKYNET_ROOT}/3rd/lpeg/lpprint.c"
      "${SKYUV_PORTABLE_SKYNET_ROOT}/3rd/lpeg/lptree.c"
      "${SKYUV_PORTABLE_SKYNET_ROOT}/3rd/lpeg/lpvm.c"
  )
  set_target_properties(
    skyuv_portable_lua_module_lpeg
    PROPERTIES
      C_EXTENSIONS TRUE
      LIBRARY_OUTPUT_DIRECTORY "${PROJECT_BINARY_DIR}/3rd/portable/luaclib"
      RUNTIME_OUTPUT_DIRECTORY "${PROJECT_BINARY_DIR}/3rd/portable/luaclib"
      OUTPUT_NAME lpeg
      PREFIX ""
      WINDOWS_EXPORT_ALL_SYMBOLS TRUE
  )
  foreach(config IN ITEMS DEBUG RELEASE RELWITHDEBINFO MINSIZEREL)
    set_target_properties(
      skyuv_portable_lua_module_lpeg
      PROPERTIES
        "LIBRARY_OUTPUT_DIRECTORY_${config}" "${PROJECT_BINARY_DIR}/3rd/portable/luaclib"
        "RUNTIME_OUTPUT_DIRECTORY_${config}" "${PROJECT_BINARY_DIR}/3rd/portable/luaclib"
    )
  endforeach()
  target_include_directories(
    skyuv_portable_lua_module_lpeg
    PRIVATE
      "${SKYUV_PORTABLE_SKYNET_ROOT}/3rd/lua"
      "${SKYUV_PORTABLE_SKYNET_ROOT}/3rd/lpeg"
  )
  target_link_libraries(skyuv_portable_lua_module_lpeg PRIVATE skyuv::lua)

  add_library(
    skyuv_portable_lua_module_md5
    MODULE
      "${SKYUV_PORTABLE_SKYNET_ROOT}/3rd/lua-md5/md5.c"
      "${SKYUV_PORTABLE_SKYNET_ROOT}/3rd/lua-md5/md5lib.c"
      "${SKYUV_PORTABLE_SKYNET_ROOT}/3rd/lua-md5/compat-5.2.c"
  )
  set_target_properties(
    skyuv_portable_lua_module_md5
    PROPERTIES
      C_EXTENSIONS TRUE
      ARCHIVE_OUTPUT_DIRECTORY "${PROJECT_BINARY_DIR}/3rd/portable/luaclib/md5"
      LIBRARY_OUTPUT_DIRECTORY "${PROJECT_BINARY_DIR}/3rd/portable/luaclib/md5"
      RUNTIME_OUTPUT_DIRECTORY "${PROJECT_BINARY_DIR}/3rd/portable/luaclib/md5"
      OUTPUT_NAME core
      PREFIX ""
      WINDOWS_EXPORT_ALL_SYMBOLS TRUE
  )
  foreach(config IN ITEMS DEBUG RELEASE RELWITHDEBINFO MINSIZEREL)
    set_target_properties(
      skyuv_portable_lua_module_md5
      PROPERTIES
        "LIBRARY_OUTPUT_DIRECTORY_${config}" "${PROJECT_BINARY_DIR}/3rd/portable/luaclib/md5"
        "RUNTIME_OUTPUT_DIRECTORY_${config}" "${PROJECT_BINARY_DIR}/3rd/portable/luaclib/md5"
        "ARCHIVE_OUTPUT_DIRECTORY_${config}" "${PROJECT_BINARY_DIR}/3rd/portable/luaclib/md5"
    )
  endforeach()
  target_include_directories(
    skyuv_portable_lua_module_md5
    PRIVATE
      "${SKYUV_PORTABLE_SKYNET_ROOT}/3rd/lua"
      "${SKYUV_PORTABLE_SKYNET_ROOT}/3rd/lua-md5"
  )
  target_link_libraries(skyuv_portable_lua_module_md5 PRIVATE skyuv::lua)

  set(
    SKYUV_PORTABLE_LUA_MODULE_SOURCES
    "${SKYUV_PORTABLE_LUALIB_SOURCE_DIR}/lua-skynet.c"
    "${SKYUV_PORTABLE_LUALIB_SOURCE_DIR}/lua-seri.c"
    "${SKYUV_PORTABLE_LUALIB_SOURCE_DIR}/lua-socket.c"
    "${SKYUV_PORTABLE_LUALIB_SOURCE_DIR}/lua-crypt.c"
    "${SKYUV_PORTABLE_LUALIB_SOURCE_DIR}/lsha1.c"
    "${SKYUV_PORTABLE_LUALIB_SOURCE_DIR}/lua-stm.c"
    "${SKYUV_PORTABLE_LUALIB_SOURCE_DIR}/lua-sharetable.c"
    "${SKYUV_PORTABLE_LUALIB_SOURCE_DIR}/lua-sharedata.c"
    "${SKYUV_PORTABLE_LUALIB_SOURCE_DIR}/lua-datasheet.c"
    "${SKYUV_PORTABLE_LUALIB_SOURCE_DIR}/lua-netpack.c"
    "${SKYUV_PORTABLE_LUALIB_SOURCE_DIR}/lua-memory.c"
    "${SKYUV_PORTABLE_LUALIB_SOURCE_DIR}/lua-multicast.c"
    "${SKYUV_PORTABLE_LUALIB_SOURCE_DIR}/lua-cluster.c"
    "${SKYUV_PORTABLE_LUALIB_SOURCE_DIR}/lua-debugchannel.c"
    "${SKYUV_PORTABLE_LUALIB_SOURCE_DIR}/lua-mongo.c"
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
      "${PROJECT_SOURCE_DIR}/src/compat/skynet/start"
      "${SKYUV_PORTABLE_SKYNET_SOURCE_DIR}"
      "${SKYUV_PORTABLE_SKYNET_ROOT}/3rd/lua"
      "${SKYUV_PORTABLE_SERVICE_DIR}"
      "${SKYUV_PORTABLE_LUALIB_SOURCE_DIR}"
  )
  if(SKYUV_USES_MSVC_FRONTEND)
    skyuv_target_force_include(skyuv_portable_lua_module_skynet stdlib.h)
  endif()
  skyuv_target_force_include(
    skyuv_portable_lua_module_skynet "${PROJECT_SOURCE_DIR}/src/compat/skynet/atomic.h"
  )
  skyuv_target_force_include(
    skyuv_portable_lua_module_skynet "${PROJECT_SOURCE_DIR}/src/compat/skynet/skyuv_time.h"
  )
  skyuv_target_force_include(
    skyuv_portable_lua_module_skynet "${PROJECT_SOURCE_DIR}/src/compat/skynet/spinlock.h"
  )
  target_link_libraries(
    skyuv_portable_lua_module_skynet
    PRIVATE
      skyuv_skynet_portable
      skyuv_socket_info
      skyuv::allocator
      skyuv::lua
      skyuv::libuv
      skyuv::platform
      skyuv_skynet_runtime_compat
  )


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

install(
  TARGETS skyuv_skynet_portable
  RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}"
  COMPONENT Runtime
)

if(SKYUV_USE_SHARED_LIBUV)
  install(
    TARGETS uv
    LIBRARY DESTINATION "${CMAKE_INSTALL_BINDIR}" COMPONENT Runtime
    RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}"
    COMPONENT Runtime
  )
endif()

install(
  TARGETS
    skyuv_portable_service_snlua
    skyuv_portable_service_logger
    skyuv_portable_service_harbor
    skyuv_portable_service_gate
  LIBRARY DESTINATION "${SKYUV_INSTALL_CSERVICEDIR}" COMPONENT Runtime
  RUNTIME DESTINATION "${SKYUV_INSTALL_CSERVICEDIR}"
  COMPONENT Runtime
)

install(
  TARGETS
    skyuv_portable_lua_module_bson
    skyuv_portable_lua_module_lpeg
    skyuv_portable_lua_module_skynet
  LIBRARY DESTINATION "${SKYUV_INSTALL_LUACLIBDIR}" COMPONENT Runtime
  RUNTIME DESTINATION "${SKYUV_INSTALL_LUACLIBDIR}"
  COMPONENT Runtime
)

install(
  TARGETS skyuv_portable_lua_module_client
  LIBRARY DESTINATION "${SKYUV_INSTALL_LUACLIBDIR}/client" COMPONENT Runtime
  RUNTIME DESTINATION "${SKYUV_INSTALL_LUACLIBDIR}/client"
  COMPONENT Runtime
)

install(
  TARGETS skyuv_portable_lua_module_md5
  LIBRARY DESTINATION "${SKYUV_INSTALL_LUACLIBDIR}/md5" COMPONENT Runtime
  RUNTIME DESTINATION "${SKYUV_INSTALL_LUACLIBDIR}/md5"
  COMPONENT Runtime
)

install(
  TARGETS skyuv_portable_lua_module_control
  LIBRARY DESTINATION "${SKYUV_INSTALL_LUACLIBDIR}/skyuv" COMPONENT Runtime
  RUNTIME DESTINATION "${SKYUV_INSTALL_LUACLIBDIR}/skyuv"
  COMPONENT Runtime
)

install(
  TARGETS skyuv_portable_lua_module_sproto
  LIBRARY DESTINATION "${SKYUV_INSTALL_LUACLIBDIR}/sproto" COMPONENT Runtime
  RUNTIME DESTINATION "${SKYUV_INSTALL_LUACLIBDIR}/sproto"
  COMPONENT Runtime
)
