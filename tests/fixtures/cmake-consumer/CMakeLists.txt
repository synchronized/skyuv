cmake_minimum_required(VERSION 3.23)

project(skyuv_cmake_consumer LANGUAGES C)

if(NOT DEFINED SKYUV_SOURCE_DIR)
  message(FATAL_ERROR "SKYUV_SOURCE_DIR is required")
endif()

set(SKYUV_BUILD_TESTS OFF CACHE BOOL "" FORCE)
add_subdirectory("${SKYUV_SOURCE_DIR}" skyuv)

if(NOT TARGET skyuv::platform)
  message(FATAL_ERROR "skyuv::platform was not created")
endif()

if(NOT TARGET skyuv::lua)
  message(FATAL_ERROR "skyuv::lua was not created")
endif()
