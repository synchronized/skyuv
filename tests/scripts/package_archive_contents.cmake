cmake_minimum_required(VERSION 3.23)

foreach(
  variable
  IN ITEMS
    SKYUV_TEST_BUILD_DIR
    SKYUV_TEST_CPACK_COMMAND
    SKYUV_TEST_CONFIG
    SKYUV_TEST_EXECUTABLE_NAME
    SKYUV_TEST_MODULE_SUFFIX
    SKYUV_TEST_PACKAGE_EXTENSION
)
  if(NOT DEFINED ${variable} OR "${${variable}}" STREQUAL "")
    message(FATAL_ERROR "缺少归档测试参数：${variable}")
  endif()
endforeach()

set(output_dir "${SKYUV_TEST_BUILD_DIR}/package-test-output")
file(REMOVE_RECURSE "${output_dir}")
file(MAKE_DIRECTORY "${output_dir}")

execute_process(
  COMMAND
    "${SKYUV_TEST_CPACK_COMMAND}" --config "${SKYUV_TEST_BUILD_DIR}/CPackConfig.cmake"
    -C "${SKYUV_TEST_CONFIG}" -B "${output_dir}"
  RESULT_VARIABLE package_result
  OUTPUT_VARIABLE package_output
  ERROR_VARIABLE package_error
)
if(NOT package_result EQUAL 0)
  message(FATAL_ERROR "生成发行归档失败：\n${package_output}\n${package_error}")
endif()

file(GLOB archives "${output_dir}/*${SKYUV_TEST_PACKAGE_EXTENSION}")
list(LENGTH archives archive_count)
if(NOT archive_count EQUAL 1)
  message(FATAL_ERROR "预期生成一个发行归档，实际为 ${archive_count} 个：${archives}")
endif()
list(GET archives 0 archive)

execute_process(
  COMMAND "${CMAKE_COMMAND}" -E tar tf "${archive}"
  RESULT_VARIABLE list_result
  OUTPUT_VARIABLE archive_contents
  ERROR_VARIABLE list_error
)
if(NOT list_result EQUAL 0)
  message(FATAL_ERROR "读取发行归档失败：${list_error}")
endif()
string(REPLACE "\\" "/" archive_contents "${archive_contents}")

set(required_entries
  "/bin/${SKYUV_TEST_EXECUTABLE_NAME}"
  "/cservice/snlua${SKYUV_TEST_MODULE_SUFFIX}"
  "/luaclib/skynet${SKYUV_TEST_MODULE_SUFFIX}"
  "/lualib/loader.lua"
  "/service/bootstrap.lua"
  "/examples/skyuv.conf"
  "/examples/skyuv-echo.conf"
  "/examples/skyuv_runtime_echo.lua"
  "/examples/skyuv_runtime_smoke.lua"
  "/licenses/skyuv-LICENSE"
  "/licenses/Lua-LICENSE"
  "/licenses/LPeg-LICENSE"
  "/licenses/lua-md5-LICENSE"
  "/licenses/versions.txt"
)
foreach(entry IN LISTS required_entries)
  string(FIND "${archive_contents}" "${entry}" entry_position)
  if(entry_position EQUAL -1)
    message(FATAL_ERROR "发行归档缺少必须文件：${entry}")
  endif()
endforeach()

string(REPLACE "\n" ";" archive_entries "${archive_contents}")
foreach(entry IN LISTS archive_entries)
  if(entry MATCHES "\\.(exp|lib|obj|pdb)$")
    message(FATAL_ERROR "发行归档包含禁止的构建产物：${entry}")
  endif()
  if(entry MATCHES "/(tests|CMakeFiles)/" OR entry MATCHES "/CMakeCache\\.txt$")
    message(FATAL_ERROR "发行归档包含测试或构建目录：${entry}")
  endif()
endforeach()

message(STATUS "PACKAGE_ARCHIVE_CONTENTS_OK: ${archive}")
