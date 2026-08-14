# 第三方目标不得继承 skyuv 自有代码的严格警告策略。
set(LIBUV_BUILD_TESTS OFF CACHE BOOL "不构建 libuv 测试" FORCE)
set(LIBUV_BUILD_BENCH OFF CACHE BOOL "不构建 libuv benchmark" FORCE)
set(LIBUV_BUILD_SHARED ${SKYUV_USE_SHARED_LIBUV} CACHE BOOL "构建动态版本的 libuv" FORCE)
set(ENABLE_CLANG_TIDY OFF CACHE BOOL "不对第三方 libuv 运行 clang-tidy" FORCE)

add_subdirectory(libuv EXCLUDE_FROM_ALL)

if(SKYUV_USE_SHARED_LIBUV)
  add_library(skyuv::libuv ALIAS uv)
else()
  # skyuv 平台层会被 Skynet 动态模块链接，静态 libuv 必须生成位置无关代码。
  set_target_properties(uv_a PROPERTIES POSITION_INDEPENDENT_CODE ON)
  add_library(skyuv::libuv ALIAS uv_a)
endif()

include(skynet_lua.cmake)
include(skynet_allocator.cmake)
include(skynet_portable_core.cmake)
include(skynet_core.cmake)

if(SKYUV_BUILD_TESTS)
  # CMocka 只用于 skyuv 测试，不构建其示例、自测或动态库。
  set(BUILD_SHARED_LIBS OFF)
  set(WITH_EXAMPLES OFF)
  set(UNIT_TESTING OFF)
  set(PICKY_DEVELOPER OFF)

  # CMocka 会无条件向源码目录创建 compile_commands.json 符号链接。
  # 该行为在 Windows 需要额外权限，也会让多个 preset 争用同一路径。
  # 使用构建目录中的副本，并仅移除这项开发辅助行为，保持第三方源码干净。
  set(SKYUV_CMOCKA_SOURCE_DIR "${CMAKE_CURRENT_BINARY_DIR}/cmocka-source")
  file(REMOVE_RECURSE "${SKYUV_CMOCKA_SOURCE_DIR}")
  file(COPY "${CMAKE_CURRENT_SOURCE_DIR}/cmocka/" DESTINATION "${SKYUV_CMOCKA_SOURCE_DIR}")
  set(SKYUV_CMOCKA_CMAKELISTS "${SKYUV_CMOCKA_SOURCE_DIR}/CMakeLists.txt")
  file(READ "${SKYUV_CMOCKA_CMAKELISTS}" SKYUV_CMOCKA_CMAKE_CONTENT)
  set(SKYUV_CMOCKA_SYMLINK_BLOCK
      "# Link combile database for clangd\nexecute_process(COMMAND cmake -E create_symlink\n                \"\${cmocka_BINARY_DIR}/compile_commands.json\"\n                \"\${cmocka_SOURCE_DIR}/compile_commands.json\")\n")
  string(FIND "${SKYUV_CMOCKA_CMAKE_CONTENT}" "${SKYUV_CMOCKA_SYMLINK_BLOCK}"
         SKYUV_CMOCKA_SYMLINK_POSITION)
  if(SKYUV_CMOCKA_SYMLINK_POSITION EQUAL -1)
    message(FATAL_ERROR "无法定位 CMocka 的 compile_commands.json 符号链接逻辑")
  endif()
  string(REPLACE "${SKYUV_CMOCKA_SYMLINK_BLOCK}" "" SKYUV_CMOCKA_CMAKE_CONTENT
                 "${SKYUV_CMOCKA_CMAKE_CONTENT}")
  file(WRITE "${SKYUV_CMOCKA_CMAKELISTS}" "${SKYUV_CMOCKA_CMAKE_CONTENT}")
  add_subdirectory("${SKYUV_CMOCKA_SOURCE_DIR}" cmocka EXCLUDE_FROM_ALL)
endif()

# Skynet 当前没有 CMake 构建入口。待适配层边界确定后，在 skyuv 自有目标中
# 选择性编译其核心源码，避免直接修改上游构建系统。
