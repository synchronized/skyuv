# 第三方目标不得继承 skyuv 自有代码的严格警告策略。
set(LIBUV_BUILD_TESTS OFF CACHE BOOL "不构建 libuv 测试" FORCE)
set(LIBUV_BUILD_BENCH OFF CACHE BOOL "不构建 libuv benchmark" FORCE)
set(LIBUV_BUILD_SHARED ${SKYUV_USE_SHARED_LIBUV} CACHE BOOL "构建动态版本的 libuv" FORCE)
set(ENABLE_CLANG_TIDY OFF CACHE BOOL "不对第三方 libuv 运行 clang-tidy" FORCE)

add_subdirectory(libuv EXCLUDE_FROM_ALL)

if(WIN32)
  find_package(Git REQUIRED)
  set(SKYUV_LIBUV_PATCH_DIR "${CMAKE_CURRENT_BINARY_DIR}/libuv-patched")
  file(MAKE_DIRECTORY "${SKYUV_LIBUV_PATCH_DIR}/src/win")
  configure_file("libuv/src/win/winsock.c"
    "${SKYUV_LIBUV_PATCH_DIR}/src/win/winsock.c" COPYONLY)
  file(RELATIVE_PATH SKYUV_LIBUV_PATCH_RELATIVE "${SKYUV_SOURCE_DIR}" "${SKYUV_LIBUV_PATCH_DIR}")
  execute_process(COMMAND "${GIT_EXECUTABLE}" apply --unidiff-zero
    "--directory=${SKYUV_LIBUV_PATCH_RELATIVE}"
    "${SKYUV_SOURCE_DIR}/patches/libuv/0001-Fix-Windows-IPv6-initializer-braces.patch"
    WORKING_DIRECTORY "${SKYUV_SOURCE_DIR}"
    RESULT_VARIABLE SKYUV_LIBUV_PATCH_RESULT ERROR_VARIABLE SKYUV_LIBUV_PATCH_ERROR)
  if(NOT SKYUV_LIBUV_PATCH_RESULT EQUAL 0)
    message(FATAL_ERROR "应用 libuv IPv6 初始化补丁失败：${SKYUV_LIBUV_PATCH_ERROR}")
  endif()
  file(READ "${SKYUV_LIBUV_PATCH_DIR}/src/win/winsock.c" SKYUV_LIBUV_PATCHED_CONTENT)
  string(FIND "${SKYUV_LIBUV_PATCHED_CONTENT}"
    "init_sin6_addr = { { IN6ADDR_LOOPBACK_INIT } };" SKYUV_LIBUV_PATCH_POSITION)
  if(SKYUV_LIBUV_PATCH_POSITION EQUAL -1)
    message(FATAL_ERROR "libuv IPv6 初始化补丁未生效")
  endif()
  foreach(uv_target IN ITEMS uv uv_a)
    if(TARGET ${uv_target})
      get_target_property(uv_original_sources ${uv_target} SOURCES)
      set(uv_patched_sources)
      set(uv_winsock_replaced FALSE)
      foreach(uv_source IN LISTS uv_original_sources)
        if(uv_source STREQUAL "src/win/winsock.c" OR
           uv_source STREQUAL "${CMAKE_CURRENT_SOURCE_DIR}/libuv/src/win/winsock.c")
          list(APPEND uv_patched_sources "${SKYUV_LIBUV_PATCH_DIR}/src/win/winsock.c")
          set(uv_winsock_replaced TRUE)
        elseif(IS_ABSOLUTE "${uv_source}" OR uv_source MATCHES "^\\$<")
          list(APPEND uv_patched_sources "${uv_source}")
        else()
          list(APPEND uv_patched_sources "${CMAKE_CURRENT_SOURCE_DIR}/libuv/${uv_source}")
        endif()
      endforeach()
      if(NOT uv_winsock_replaced)
        message(FATAL_ERROR "libuv ${uv_target} 的 winsock.c 编译输入未找到，需审查补丁接入")
      endif()
      set_property(TARGET ${uv_target} PROPERTY SOURCES "${uv_patched_sources}")
      # 恢复原文件在 src/win 内的引号头文件搜索顺序。
      target_include_directories(${uv_target} BEFORE PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/libuv/src/win")
    endif()
  endforeach()
endif()

if(WIN32 AND CMAKE_C_COMPILER_ID STREQUAL "Clang"
   AND CMAKE_C_COMPILER_FRONTEND_VARIANT STREQUAL "GNU")
  foreach(uv_target IN ITEMS uv uv_a)
    if(TARGET ${uv_target})
      # Clang 的 Windows ABI 不定义 __GNUC__，补齐树宏已经提供的 unused 扩展入口。
      # CRT 提示仅限第三方 libuv；不改变 skyuv 自有目标的警告策略。
      target_compile_definitions(${uv_target} PRIVATE
        _CRT_SECURE_NO_WARNINGS "UV__UNUSED=__attribute__((unused))")
      # heap-inl.h 的共享静态实现不能通过宏覆盖，例外仅限已审查的两个翻译单元。
      set_property(SOURCE "${CMAKE_CURRENT_SOURCE_DIR}/libuv/src/timer.c"
        "${CMAKE_CURRENT_SOURCE_DIR}/libuv/src/win/core.c"
        TARGET_DIRECTORY ${uv_target} APPEND PROPERTY COMPILE_OPTIONS -Wno-unused-function)
    endif()
  endforeach()
endif()

if(SKYUV_USE_SHARED_LIBUV)
  set(SKYUV_LIBUV_TARGET uv)
  add_library(skyuv::libuv ALIAS uv)
else()
  # skyuv 平台层会被 Skynet 动态模块链接，静态 libuv 必须生成位置无关代码。
  set_target_properties(uv_a PROPERTIES POSITION_INDEPENDENT_CODE ON)
  set(SKYUV_LIBUV_TARGET uv_a)
  add_library(skyuv::libuv ALIAS uv_a)
endif()

set_target_properties(
  "${SKYUV_LIBUV_TARGET}"
  PROPERTIES
    EXPORT_NAME libuv
)

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
