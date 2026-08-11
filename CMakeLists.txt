cmake_minimum_required(VERSION 3.23)

project(skyuv VERSION 0.1.0 LANGUAGES C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

option(SKYUV_BUILD_TESTS "构建 skyuv 测试" ON)
option(SKYUV_BUILD_SHARED "将 skyuv 构建为动态库" OFF)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
include(skyuv_warnings)

if(PROJECT_IS_TOP_LEVEL AND SKYUV_BUILD_TESTS)
  include(CTest)
endif()

# 实现代码加入后，由各目录维护自己的构建目标。
foreach(directory IN ITEMS src tests)
  if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/${directory}/CMakeLists.txt")
    add_subdirectory("${directory}")
  endif()
endforeach()
