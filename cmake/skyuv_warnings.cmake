# skyuv 自有目标的编译警告策略，不得应用到第三方依赖。
option(SKYUV_ENABLE_WARNINGS "启用 skyuv 目标编译警告" ON)
option(SKYUV_ENABLE_PEDANTIC_WARNINGS "启用 skyuv 目标严格标准警告" ON)
option(SKYUV_WARNINGS_AS_ERRORS "将 skyuv 目标编译警告视为错误" ON)

function(skyuv_target_compile_warnings target)
  if(NOT TARGET ${target})
    message(FATAL_ERROR "目标不存在：${target}")
  endif()

  if(NOT SKYUV_ENABLE_WARNINGS)
    return()
  endif()

  target_compile_options(
    ${target}
    PRIVATE
      $<$<COMPILE_LANG_AND_ID:C,MSVC>:/utf-8>
      $<$<COMPILE_LANG_AND_ID:C,MSVC>:/W4>
      $<$<COMPILE_LANG_AND_ID:C,GNU,Clang,AppleClang>:-Wall>
      $<$<COMPILE_LANG_AND_ID:C,GNU,Clang,AppleClang>:-Wextra>
  )

  if(SKYUV_ENABLE_PEDANTIC_WARNINGS)
    target_compile_options(
      ${target}
      PRIVATE
        $<$<COMPILE_LANG_AND_ID:C,GNU,Clang,AppleClang>:-Wpedantic>
    )
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
      $<$<COMPILE_LANG_AND_ID:C,MSVC>:/WX>
      $<$<COMPILE_LANG_AND_ID:C,GNU,Clang,AppleClang>:-Werror>
  )
endfunction()
