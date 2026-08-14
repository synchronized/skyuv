# skyuv 自有目标的编译警告策略，不得应用到第三方依赖。
option(SKYUV_ENABLE_WARNINGS "启用 skyuv 目标编译警告" ON)
option(SKYUV_ENABLE_PEDANTIC_WARNINGS "启用 skyuv 目标严格标准警告" ON)
option(SKYUV_WARNINGS_AS_ERRORS "将 skyuv 目标编译警告视为错误" ON)

set(SKYUV_USES_MSVC_FRONTEND FALSE)
if(CMAKE_C_COMPILER_ID STREQUAL "MSVC" OR
   (CMAKE_C_COMPILER_ID STREQUAL "Clang" AND CMAKE_C_SIMULATE_ID STREQUAL "MSVC"))
  set(SKYUV_USES_MSVC_FRONTEND TRUE)
endif()

function(skyuv_target_force_include target header)
  if(SKYUV_USES_MSVC_FRONTEND)
    target_compile_options(${target} PRIVATE "/FI${header}")
  else()
    target_compile_options(${target} PRIVATE "-include${header}")
  endif()
endfunction()

function(skyuv_target_compile_warnings target)
  if(NOT TARGET ${target})
    message(FATAL_ERROR "目标不存在：${target}")
  endif()

  if(NOT SKYUV_ENABLE_WARNINGS)
    return()
  endif()

  # clang-cl 的编译器 ID 是 Clang，但使用 MSVC 命令行前端。
  # 不得向它传递 Clang GNU 前端的 -Wall；该选项在 clang-cl 中会启用
  # 大量不属于 /W4 的兼容性诊断，并把 Windows SDK 与第三方头文件一并报错。
  if(SKYUV_USES_MSVC_FRONTEND)
    target_compile_options(${target} PRIVATE /utf-8 /W4)
    set(SKYUV_TARGET_USES_MSVC_FRONTEND TRUE)
  else()
    target_compile_options(${target} PRIVATE -Wall -Wextra)
    set(SKYUV_TARGET_USES_MSVC_FRONTEND FALSE)
  endif()

  if(SKYUV_ENABLE_PEDANTIC_WARNINGS AND NOT SKYUV_TARGET_USES_MSVC_FRONTEND)
    target_compile_options(${target} PRIVATE -Wpedantic)
  endif()

  if(NOT SKYUV_WARNINGS_AS_ERRORS)
    return()
  endif()

  # CMake 3.24 起提供统一属性；兼容项目要求的 CMake 3.23。
  if(CMAKE_VERSION VERSION_GREATER_EQUAL "3.24")
    set_property(TARGET ${target} PROPERTY COMPILE_WARNING_AS_ERROR ON)
    return()
  endif()

  target_compile_options(
    ${target}
    PRIVATE
      $<$<BOOL:${SKYUV_TARGET_USES_MSVC_FRONTEND}>:/WX>
      $<$<NOT:$<BOOL:${SKYUV_TARGET_USES_MSVC_FRONTEND}>>:-Werror>
  )
endfunction()
