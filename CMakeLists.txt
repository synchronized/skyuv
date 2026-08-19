cmake_minimum_required(VERSION 3.23)

project(skyuv VERSION 0.1.0 LANGUAGES C)

# CMake 的 project(VERSION) 只接受数字版本；预发布标识单独维护，并用于产物和标签。
set(SKYUV_VERSION_PRERELEASE "beta.1")
if(SKYUV_VERSION_PRERELEASE)
  set(SKYUV_RELEASE_VERSION "${PROJECT_VERSION}-${SKYUV_VERSION_PRERELEASE}")
else()
  set(SKYUV_RELEASE_VERSION "${PROJECT_VERSION}")
endif()

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

# Windows 发行包不依赖可再发行的动态 CRT。该设置必须覆盖主程序、动态模块、
# 测试和静态链接进来的第三方目标，避免同一最终二进制混用两种 CRT。
if(MSVC)
  set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif()

option(SKYUV_BUILD_TESTS "构建 skyuv 测试" ON)
option(SKYUV_BUILD_SHARED "将 skyuv 构建为动态库" OFF)
if(WIN32)
  set(SKYUV_SHARED_LIBUV_DEFAULT OFF)
else()
  set(SKYUV_SHARED_LIBUV_DEFAULT ON)
endif()
option(SKYUV_USE_SHARED_LIBUV "链接动态版本的 libuv" ${SKYUV_SHARED_LIBUV_DEFAULT})
if(WIN32)
  set(SKYUV_PACKAGE_EXTENSION ".zip")
else()
  set(SKYUV_PACKAGE_EXTENSION ".tar.gz")
endif()

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
set(
  SKYUV_INSTALL_LUALIBDIR
  "lualib"
  CACHE PATH
  "Lua 库的安装目录（相对于安装前缀）"
)
set(
  SKYUV_INSTALL_SERVICEDIR
  "service"
  CACHE PATH
  "Lua 服务的安装目录（相对于安装前缀）"
)
set(
  SKYUV_INSTALL_EXAMPLESDIR
  "examples"
  CACHE PATH
  "发行示例的安装目录（相对于安装前缀）"
)
set(
  SKYUV_INSTALL_LICENSESDIR
  "licenses"
  CACHE PATH
  "许可证的安装目录（相对于安装前缀）"
)

foreach(
  directory
  IN ITEMS
    SKYUV_INSTALL_CSERVICEDIR
    SKYUV_INSTALL_LUACLIBDIR
    SKYUV_INSTALL_LUALIBDIR
    SKYUV_INSTALL_SERVICEDIR
    SKYUV_INSTALL_EXAMPLESDIR
    SKYUV_INSTALL_LICENSESDIR
)
  if(IS_ABSOLUTE "${${directory}}")
    message(FATAL_ERROR "${directory} 必须是相对于安装前缀的路径")
  endif()
endforeach()

install(
  DIRECTORY "${PROJECT_SOURCE_DIR}/3rd/skynet/lualib/"
  DESTINATION "${SKYUV_INSTALL_LUALIBDIR}"
  COMPONENT Runtime
  FILES_MATCHING PATTERN "*.lua"
)
install(
  DIRECTORY "${PROJECT_SOURCE_DIR}/3rd/skynet/service/"
  DESTINATION "${SKYUV_INSTALL_SERVICEDIR}"
  COMPONENT Runtime
  FILES_MATCHING PATTERN "*.lua"
)

install(
  FILES "${PROJECT_SOURCE_DIR}/LICENSE"
  DESTINATION "${SKYUV_INSTALL_LICENSESDIR}"
  RENAME skyuv-LICENSE
  COMPONENT Runtime
)
install(
  FILES "${PROJECT_SOURCE_DIR}/3rd/skynet/LICENSE"
  DESTINATION "${SKYUV_INSTALL_LICENSESDIR}"
  RENAME skynet-LICENSE
  COMPONENT Runtime
)
install(
  FILES "${PROJECT_SOURCE_DIR}/3rd/libuv/LICENSE"
  DESTINATION "${SKYUV_INSTALL_LICENSESDIR}"
  RENAME libuv-LICENSE
  COMPONENT Runtime
)
install(
  FILES "${PROJECT_SOURCE_DIR}/3rd/libuv/LICENSE-extra"
  DESTINATION "${SKYUV_INSTALL_LICENSESDIR}"
  RENAME libuv-LICENSE-extra
  COMPONENT Runtime
)
install(
  FILES "${PROJECT_SOURCE_DIR}/3rd/libuv/LICENSE-docs"
  DESTINATION "${SKYUV_INSTALL_LICENSESDIR}"
  RENAME libuv-LICENSE-docs
  COMPONENT Runtime
)
install(
  FILES
    "${PROJECT_SOURCE_DIR}/licenses/Lua-LICENSE"
    "${PROJECT_SOURCE_DIR}/licenses/LPeg-LICENSE"
    "${PROJECT_SOURCE_DIR}/licenses/lua-md5-LICENSE"
  DESTINATION "${SKYUV_INSTALL_LICENSESDIR}"
  COMPONENT Runtime
)

file(MAKE_DIRECTORY "${PROJECT_BINARY_DIR}/generated/examples")
configure_file(
  "${PROJECT_SOURCE_DIR}/examples/skyuv-runtime.conf.in"
  "${PROJECT_BINARY_DIR}/generated/examples/skyuv.conf"
  @ONLY
)
configure_file(
  "${PROJECT_SOURCE_DIR}/examples/skyuv-runtime-echo.conf.in"
  "${PROJECT_BINARY_DIR}/generated/examples/skyuv-echo.conf"
  @ONLY
)
install(
  FILES
    "${PROJECT_BINARY_DIR}/generated/examples/skyuv.conf"
    "${PROJECT_BINARY_DIR}/generated/examples/skyuv-echo.conf"
    "${PROJECT_SOURCE_DIR}/examples/skyuv_runtime_echo.lua"
    "${PROJECT_SOURCE_DIR}/examples/skyuv_runtime_smoke.lua"
  DESTINATION "${SKYUV_INSTALL_EXAMPLESDIR}"
  COMPONENT Runtime
)
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

foreach(dependency IN ITEMS skynet libuv)
  execute_process(
    COMMAND "${GIT_EXECUTABLE}" -C "${PROJECT_SOURCE_DIR}/3rd/${dependency}" rev-parse HEAD
    RESULT_VARIABLE commit_result
    OUTPUT_VARIABLE commit
    OUTPUT_STRIP_TRAILING_WHITESPACE
  )
  execute_process(
    COMMAND "${GIT_EXECUTABLE}" -C "${PROJECT_SOURCE_DIR}/3rd/${dependency}" describe --tags --always
    RESULT_VARIABLE describe_result
    OUTPUT_VARIABLE describe
    OUTPUT_STRIP_TRAILING_WHITESPACE
  )
  if(NOT commit_result EQUAL 0 OR NOT describe_result EQUAL 0)
    message(FATAL_ERROR "无法读取第三方版本：${dependency}")
  endif()
  string(TOUPPER "${dependency}" dependency_upper)
  set("SKYUV_${dependency_upper}_COMMIT" "${commit}")
  set("SKYUV_${dependency_upper}_DESCRIBE" "${describe}")
endforeach()
configure_file(
  "${PROJECT_SOURCE_DIR}/cmake/skyuv-versions.txt.in"
  "${PROJECT_BINARY_DIR}/generated/versions.txt"
  @ONLY
)
install(
  FILES "${PROJECT_BINARY_DIR}/generated/versions.txt"
  DESTINATION "${SKYUV_INSTALL_LICENSESDIR}"
  COMPONENT Runtime
)

# 分配器选项由第三方接入层定义，必须在 add_subdirectory(3rd) 之后判断。
if(CMAKE_SYSTEM_NAME STREQUAL "Linux" AND SKYUV_ALLOCATOR STREQUAL "jemalloc")
  install(
    FILES "${PROJECT_SOURCE_DIR}/3rd/skynet/3rd/jemalloc/COPYING"
    DESTINATION "${SKYUV_INSTALL_LICENSESDIR}"
    RENAME jemalloc-COPYING
    COMPONENT Runtime
  )
endif()

string(TOLOWER "${CMAKE_SYSTEM_NAME}" SKYUV_PACKAGE_SYSTEM)
string(TOLOWER "${CMAKE_SYSTEM_PROCESSOR}" SKYUV_PACKAGE_ARCH)
string(REGEX REPLACE "[^a-z0-9]+" "-" SKYUV_PACKAGE_ARCH "${SKYUV_PACKAGE_ARCH}")
if(WIN32)
  set(CPACK_GENERATOR ZIP)
else()
  set(CPACK_GENERATOR TGZ)
endif()
set(CPACK_PACKAGE_NAME skyuv)
set(CPACK_PACKAGE_VENDOR synchronized)
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "基于 libuv 的跨平台 Skynet 运行时")
set(CPACK_PACKAGE_VERSION "${SKYUV_RELEASE_VERSION}")
set(CPACK_PACKAGE_FILE_NAME
    "skyuv-${SKYUV_RELEASE_VERSION}-${SKYUV_PACKAGE_SYSTEM}-${SKYUV_PACKAGE_ARCH}")
set(CPACK_PACKAGE_DIRECTORY "${PROJECT_BINARY_DIR}/packages")
set(CPACK_PACKAGE_CHECKSUM SHA256)
set(CPACK_PACKAGE_INSTALL_DIRECTORY skyuv)
set(CPACK_PACKAGE_LICENSE_FILE "${PROJECT_SOURCE_DIR}/LICENSE")
set(CPACK_INCLUDE_TOPLEVEL_DIRECTORY ON)
set(CPACK_INSTALL_CMAKE_PROJECTS "${PROJECT_BINARY_DIR};${PROJECT_NAME};Runtime;/")
set(CPACK_VERBATIM_VARIABLES YES)
include(CPack)
