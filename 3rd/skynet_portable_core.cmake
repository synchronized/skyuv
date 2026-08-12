set(SKYUV_SKYNET_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/skynet/skynet-src")
set(SKYUV_SKYNET_COMPAT_DIR "${PROJECT_SOURCE_DIR}/src/compat/skynet")

set(
  SKYUV_SKYNET_PORTABLE_CORE_SOURCES
  "${SKYUV_SKYNET_SOURCE_DIR}/skynet_handle.c"
  "${SKYUV_SKYNET_SOURCE_DIR}/skynet_mq.c"
  "${SKYUV_SKYNET_SOURCE_DIR}/skynet_timer.c"
  "${SKYUV_SKYNET_SOURCE_DIR}/skynet_error.c"
  "${SKYUV_SKYNET_SOURCE_DIR}/skynet_harbor.c"
  "${SKYUV_SKYNET_SOURCE_DIR}/skynet_env.c"
  "${SKYUV_SKYNET_SOURCE_DIR}/skynet_monitor.c"
)

# 只验证不含 VLA 且已完成平台迁移的 Actor 基础子集；旧 socket、daemon、Unix
# 入口及 MSVC 尚不支持的上游 VLA 源文件留在 Linux 基线目标。
add_library(skyuv_skynet_portable_core OBJECT ${SKYUV_SKYNET_PORTABLE_CORE_SOURCES})
set_target_properties(skyuv_skynet_portable_core PROPERTIES C_EXTENSIONS TRUE)
target_include_directories(
  skyuv_skynet_portable_core
  PRIVATE
    "${SKYUV_SKYNET_COMPAT_DIR}/dynamic"
    "${SKYUV_SKYNET_COMPAT_DIR}"
    "${SKYUV_SKYNET_SOURCE_DIR}"
    "${CMAKE_CURRENT_SOURCE_DIR}/skynet/3rd/lua"
)
target_compile_options(
  skyuv_skynet_portable_core
  PRIVATE
    "$<$<COMPILE_LANG_AND_ID:C,MSVC>:/FI${SKYUV_SKYNET_COMPAT_DIR}/atomic.h>"
    "$<$<COMPILE_LANG_AND_ID:C,MSVC>:/FI${SKYUV_SKYNET_COMPAT_DIR}/spinlock.h>"
    "$<$<NOT:$<COMPILE_LANG_AND_ID:C,MSVC>>:-include${SKYUV_SKYNET_COMPAT_DIR}/atomic.h>"
    "$<$<NOT:$<COMPILE_LANG_AND_ID:C,MSVC>>:-include${SKYUV_SKYNET_COMPAT_DIR}/spinlock.h>"
)
set_source_files_properties(
  "${SKYUV_SKYNET_SOURCE_DIR}/skynet_timer.c"
  PROPERTIES
    COMPILE_OPTIONS
      "$<$<COMPILE_LANG_AND_ID:C,MSVC>:/FI${SKYUV_SKYNET_COMPAT_DIR}/skyuv_time.h>;$<$<NOT:$<COMPILE_LANG_AND_ID:C,MSVC>>:-include${SKYUV_SKYNET_COMPAT_DIR}/skyuv_time.h>"
)
target_link_libraries(
  skyuv_skynet_portable_core
  PRIVATE skyuv::lua skyuv::platform skyuv::allocator
)
