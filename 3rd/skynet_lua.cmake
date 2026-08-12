set(SKYUV_LUA_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/skynet/3rd/lua")

# 与 Skynet 自带 Lua Makefile 中的 CORE_O 和 LIB_O 保持一致。
set(
  SKYUV_LUA_SOURCES
  ${SKYUV_LUA_SOURCE_DIR}/lapi.c
  ${SKYUV_LUA_SOURCE_DIR}/lcode.c
  ${SKYUV_LUA_SOURCE_DIR}/lctype.c
  ${SKYUV_LUA_SOURCE_DIR}/ldebug.c
  ${SKYUV_LUA_SOURCE_DIR}/ldo.c
  ${SKYUV_LUA_SOURCE_DIR}/ldump.c
  ${SKYUV_LUA_SOURCE_DIR}/lfunc.c
  ${SKYUV_LUA_SOURCE_DIR}/lgc.c
  ${SKYUV_LUA_SOURCE_DIR}/llex.c
  ${SKYUV_LUA_SOURCE_DIR}/lmem.c
  ${SKYUV_LUA_SOURCE_DIR}/lobject.c
  ${SKYUV_LUA_SOURCE_DIR}/lopcodes.c
  ${SKYUV_LUA_SOURCE_DIR}/lparser.c
  ${SKYUV_LUA_SOURCE_DIR}/lstate.c
  ${SKYUV_LUA_SOURCE_DIR}/lstring.c
  ${SKYUV_LUA_SOURCE_DIR}/ltable.c
  ${SKYUV_LUA_SOURCE_DIR}/ltm.c
  ${SKYUV_LUA_SOURCE_DIR}/lundump.c
  ${SKYUV_LUA_SOURCE_DIR}/lvm.c
  ${SKYUV_LUA_SOURCE_DIR}/lzio.c
  ${SKYUV_LUA_SOURCE_DIR}/lauxlib.c
  ${SKYUV_LUA_SOURCE_DIR}/lbaselib.c
  ${SKYUV_LUA_SOURCE_DIR}/lcorolib.c
  ${SKYUV_LUA_SOURCE_DIR}/ldblib.c
  ${SKYUV_LUA_SOURCE_DIR}/liolib.c
  ${SKYUV_LUA_SOURCE_DIR}/lmathlib.c
  ${SKYUV_LUA_SOURCE_DIR}/loadlib.c
  ${SKYUV_LUA_SOURCE_DIR}/loslib.c
  ${SKYUV_LUA_SOURCE_DIR}/lstrlib.c
  ${SKYUV_LUA_SOURCE_DIR}/ltablib.c
  ${SKYUV_LUA_SOURCE_DIR}/lutf8lib.c
  ${SKYUV_LUA_SOURCE_DIR}/linit.c
)

add_library(skyuv_lua STATIC ${SKYUV_LUA_SOURCES})
add_library(skyuv::lua ALIAS skyuv_lua)

target_include_directories(
  skyuv_lua
  PRIVATE
    ${PROJECT_SOURCE_DIR}/src/compat/skynet
    ${CMAKE_CURRENT_SOURCE_DIR}/skynet/skynet-src
  PUBLIC ${SKYUV_LUA_SOURCE_DIR}
)

target_link_libraries(skyuv_lua PRIVATE skyuv::platform)

target_compile_features(skyuv_lua PUBLIC c_std_99)
set_target_properties(
  skyuv_lua
  PROPERTIES
    C_EXTENSIONS ON
    POSITION_INDEPENDENT_CODE ON
)

if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
  target_compile_definitions(skyuv_lua PUBLIC LUA_USE_LINUX)
elseif(APPLE)
  target_compile_definitions(skyuv_lua PUBLIC LUA_USE_MACOSX)
endif()

if(UNIX)
  target_link_libraries(skyuv_lua PRIVATE m ${CMAKE_DL_LIBS})
endif()
