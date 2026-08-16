cmake_minimum_required(VERSION 3.23)

project(skyuv VERSION 0.1.0 LANGUAGES C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

option(SKYUV_BUILD_TESTS "构建 skyuv 测试" ON)
option(SKYUV_BUILD_SHARED "将 skyuv 构建为动态库" OFF)
if(WIN32)
  set(SKYUV_SHARED_LIBUV_DEFAULT OFF)
else()
  set(SKYUV_SHARED_LIBUV_DEFAULT ON)
endif()
option(SKYUV_USE_SHARED_LIBUV "链接动态版本的 libuv" ${SKYUV_SHARED_LIBUV_DEFAULT})

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
include(GNUInstallDirs)
include(skyuv_warnings)

set(
  SKYUV_INSTALL_CSERVICEDIR
  "cservice"
  CACHE PATH
  "Skynet C 服务的安装目录（相对于安装前缀）"
)
set(
  SKYUV_INSTALL_LUACLIBDIR
  "luaclib"
  CACHE PATH
  "Lua C 模块的安装目录（相对于安装前缀）"
)

foreach(directory IN ITEMS SKYUV_INSTALL_CSERVICEDIR SKYUV_INSTALL_LUACLIBDIR)
  if(IS_ABSOLUTE "${${directory}}")
    message(FATAL_ERROR "${directory} 必须是相对于安装前缀的路径")
  endif()
endforeach()

if(PROJECT_IS_TOP_LEVEL AND SKYUV_BUILD_TESTS)
  include(CTest)
endif()

add_subdirectory(3rd)

# 实现代码加入后，由各目录维护自己的构建目标。
foreach(directory IN ITEMS src tests)
  if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/${directory}/CMakeLists.txt")
    add_subdirectory("${directory}")
  endif()
endforeach()
