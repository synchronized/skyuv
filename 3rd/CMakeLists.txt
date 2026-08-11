# 第三方目标不得继承 skyuv 自有代码的严格警告策略。
set(LIBUV_BUILD_TESTS OFF CACHE BOOL "不构建 libuv 测试" FORCE)
set(LIBUV_BUILD_BENCH OFF CACHE BOOL "不构建 libuv benchmark" FORCE)
set(LIBUV_BUILD_SHARED ${SKYUV_USE_SHARED_LIBUV} CACHE BOOL "构建动态版本的 libuv" FORCE)
set(ENABLE_CLANG_TIDY OFF CACHE BOOL "不对第三方 libuv 运行 clang-tidy" FORCE)

add_subdirectory(libuv EXCLUDE_FROM_ALL)

if(SKYUV_USE_SHARED_LIBUV)
  add_library(skyuv::libuv ALIAS uv)
else()
  add_library(skyuv::libuv ALIAS uv_a)
endif()

# Skynet 当前没有 CMake 构建入口。待适配层边界确定后，在 skyuv 自有目标中
# 选择性编译其核心源码，避免直接修改上游构建系统。
