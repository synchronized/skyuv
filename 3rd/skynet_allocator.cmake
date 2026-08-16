if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
  set(SKYUV_DEFAULT_ALLOCATOR jemalloc)
else()
  set(SKYUV_DEFAULT_ALLOCATOR system)
endif()

set(
  SKYUV_ALLOCATOR
  ${SKYUV_DEFAULT_ALLOCATOR}
  CACHE STRING
  "skyuv 使用的内存分配器：jemalloc 或 system"
)
set_property(CACHE SKYUV_ALLOCATOR PROPERTY STRINGS jemalloc system)

if(NOT SKYUV_ALLOCATOR STREQUAL "jemalloc" AND NOT SKYUV_ALLOCATOR STREQUAL "system")
  message(FATAL_ERROR "不支持的 SKYUV_ALLOCATOR：${SKYUV_ALLOCATOR}")
endif()

add_library(skyuv_allocator INTERFACE)
add_library(skyuv::allocator ALIAS skyuv_allocator)

if(SKYUV_ALLOCATOR STREQUAL "system")
  target_compile_definitions(skyuv_allocator INTERFACE NOUSE_JEMALLOC=1)
  return()
endif()

if(NOT CMAKE_SYSTEM_NAME STREQUAL "Linux")
  message(FATAL_ERROR "jemalloc 的 skyuv CMake 接入目前仅支持 Linux")
endif()

include(ExternalProject)
find_program(SKYUV_MAKE_EXECUTABLE make REQUIRED)

set(SKYUV_JEMALLOC_UPSTREAM_DIR "${CMAKE_CURRENT_SOURCE_DIR}/skynet/3rd/jemalloc")
set(SKYUV_JEMALLOC_PREFIX "${CMAKE_CURRENT_BINARY_DIR}/jemalloc")
set(SKYUV_JEMALLOC_SOURCE_DIR "${SKYUV_JEMALLOC_PREFIX}/upstream")
set(SKYUV_JEMALLOC_INSTALL_DIR "${SKYUV_JEMALLOC_PREFIX}/install")
set(SKYUV_JEMALLOC_LIBRARY "${SKYUV_JEMALLOC_INSTALL_DIR}/lib/libjemalloc_pic.a")
set(SKYUV_JEMALLOC_INCLUDE_DIR "${SKYUV_JEMALLOC_INSTALL_DIR}/include/jemalloc")

# IMPORTED 目标要求 include 目录在生成阶段已经存在。
file(MAKE_DIRECTORY "${SKYUV_JEMALLOC_INCLUDE_DIR}")

ExternalProject_Add(
  skyuv_jemalloc_external
  PREFIX "${SKYUV_JEMALLOC_PREFIX}"
  SOURCE_DIR "${SKYUV_JEMALLOC_SOURCE_DIR}"
  DOWNLOAD_COMMAND
    ${CMAKE_COMMAND} -E rm -rf "<SOURCE_DIR>"
    COMMAND ${CMAKE_COMMAND} -E copy_directory "${SKYUV_JEMALLOC_UPSTREAM_DIR}" "<SOURCE_DIR>"
  CONFIGURE_COMMAND
    ${CMAKE_COMMAND} -E env "CC=${CMAKE_C_COMPILER}"
    sh ./autogen.sh
      "--prefix=${SKYUV_JEMALLOC_INSTALL_DIR}"
      --disable-cxx
      --disable-shared
      --enable-static
      --with-jemalloc-prefix=je_
      --enable-prof
  BUILD_COMMAND ${SKYUV_MAKE_EXECUTABLE} -j2
  INSTALL_COMMAND ${SKYUV_MAKE_EXECUTABLE} install_include install_lib_static
  BUILD_IN_SOURCE TRUE
  BUILD_BYPRODUCTS "${SKYUV_JEMALLOC_LIBRARY}"
  UPDATE_DISCONNECTED TRUE
)

add_library(skyuv_jemalloc STATIC IMPORTED GLOBAL)
set_target_properties(
  skyuv_jemalloc
  PROPERTIES
    IMPORTED_LOCATION "${SKYUV_JEMALLOC_LIBRARY}"
    INTERFACE_INCLUDE_DIRECTORIES "${SKYUV_JEMALLOC_INCLUDE_DIR}"
)
add_dependencies(skyuv_jemalloc skyuv_jemalloc_external)

# jemalloc 的性能分析实现会调用 log、exp 和 round，静态链接时需显式传递 libm。
target_link_libraries(skyuv_allocator INTERFACE skyuv_jemalloc m)
# CMake 可能因传递依赖去重而把 libm 排到静态 jemalloc 之前，必须防止 GNU ld 提前丢弃它。
target_link_options(skyuv_allocator INTERFACE "LINKER:--no-as-needed")
target_compile_definitions(skyuv_allocator INTERFACE SKYUV_USE_JEMALLOC=1)
