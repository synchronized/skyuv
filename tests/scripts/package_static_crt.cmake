cmake_minimum_required(VERSION 3.23)

foreach(
  variable
  IN ITEMS
    SKYUV_TEST_ARCHIVE_DIR
    SKYUV_TEST_DUMPBIN
    SKYUV_TEST_PACKAGE_EXTENSION
)
  if(NOT DEFINED ${variable} OR "${${variable}}" STREQUAL "")
    message(FATAL_ERROR "缺少静态 CRT 审计参数：${variable}")
  endif()
endforeach()

file(GLOB archives "${SKYUV_TEST_ARCHIVE_DIR}/*${SKYUV_TEST_PACKAGE_EXTENSION}")
list(LENGTH archives archive_count)
if(NOT archive_count EQUAL 1)
  message(FATAL_ERROR "预期审计一个发行归档，实际为 ${archive_count} 个：${archives}")
endif()
list(GET archives 0 archive)

set(extract_dir "${SKYUV_TEST_ARCHIVE_DIR}/static-crt-audit")
file(REMOVE_RECURSE "${extract_dir}")
file(MAKE_DIRECTORY "${extract_dir}")
execute_process(
  COMMAND "${CMAKE_COMMAND}" -E tar xf "${archive}"
  WORKING_DIRECTORY "${extract_dir}"
  RESULT_VARIABLE extract_result
  ERROR_VARIABLE extract_error
)
if(NOT extract_result EQUAL 0)
  message(FATAL_ERROR "解压发行归档失败：${extract_error}")
endif()

file(GLOB_RECURSE pe_files "${extract_dir}/*.exe" "${extract_dir}/*.dll")
list(LENGTH pe_files pe_count)
if(pe_count EQUAL 0)
  message(FATAL_ERROR "发行归档中没有可审计的 PE 文件")
endif()

foreach(pe_file IN LISTS pe_files)
  execute_process(
    COMMAND "${SKYUV_TEST_DUMPBIN}" /DEPENDENTS "${pe_file}"
    RESULT_VARIABLE dumpbin_result
    OUTPUT_VARIABLE dependencies
    ERROR_VARIABLE dumpbin_error
  )
  if(NOT dumpbin_result EQUAL 0)
    message(FATAL_ERROR "读取 PE 导入表失败：${pe_file}\n${dumpbin_error}")
  endif()
  if(dependencies MATCHES
     "(VCRUNTIME|MSVCP|UCRTBASE|api-ms-win-crt)[^\r\n]*\\.dll")
    message(FATAL_ERROR "发行文件仍依赖动态 CRT：${pe_file}\n${dependencies}")
  endif()
endforeach()

message(STATUS "PACKAGE_STATIC_CRT_OK: 已审计 ${pe_count} 个 PE 文件")
