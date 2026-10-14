cmake_minimum_required(VERSION 3.23)

foreach(variable IN ITEMS SKYUV_TEST_BUILD_DIR SKYUV_TEST_SOURCE_DIR SKYUV_TEST_CONFIG
                          SKYUV_TEST_EXECUTABLE_NAME)
	if(NOT DEFINED ${variable} OR "${${variable}}" STREQUAL "")
		message(FATAL_ERROR "缺少安装测试参数：${variable}")
	endif()
endforeach()

if(DEFINED ENV{RUNNER_TEMP} AND NOT "$ENV{RUNNER_TEMP}" STREQUAL "")
	set(temp_root "$ENV{RUNNER_TEMP}")
elseif(WIN32 AND DEFINED ENV{TEMP} AND NOT "$ENV{TEMP}" STREQUAL "")
	set(temp_root "$ENV{TEMP}")
elseif(DEFINED ENV{TMPDIR} AND NOT "$ENV{TMPDIR}" STREQUAL "")
	set(temp_root "$ENV{TMPDIR}")
else()
	set(temp_root "/tmp")
endif()

string(MD5 test_id "${SKYUV_TEST_BUILD_DIR}-${SKYUV_TEST_CONFIG}")
set(install_root "${temp_root}/skyuv install smoke ${test_id}")
file(REMOVE_RECURSE "${install_root}")

execute_process(
	COMMAND
		"${CMAKE_COMMAND}" --install "${SKYUV_TEST_BUILD_DIR}"
		--config "${SKYUV_TEST_CONFIG}" --component Runtime --prefix "${install_root}"
	RESULT_VARIABLE install_result
	OUTPUT_VARIABLE install_output
	ERROR_VARIABLE install_error
)
if(NOT install_result EQUAL 0)
	message(FATAL_ERROR "安装 Runtime 组件失败：\n${install_output}\n${install_error}")
endif()

set(executable "${install_root}/bin/${SKYUV_TEST_EXECUTABLE_NAME}")
set(config "${install_root}/examples/skyuv.conf")
set(required_files
	"${executable}"
	"${config}"
	"${install_root}/examples/skyuv_runtime_smoke.lua"
	"${install_root}/lualib/loader.lua"
	"${install_root}/service/bootstrap.lua"
)
foreach(path IN LISTS required_files)
	if(NOT EXISTS "${path}")
		message(FATAL_ERROR "安装树缺少必须文件：${path}")
	endif()
endforeach()

file(GLOB_RECURSE forbidden_files
	"${install_root}/*.exp"
	"${install_root}/*.lib"
	"${install_root}/*.obj"
	"${install_root}/*.pdb"
)
if(forbidden_files)
	message(FATAL_ERROR "安装树包含禁止的构建产物：${forbidden_files}")
endif()

file(GLOB_RECURSE text_files "${install_root}/*.conf" "${install_root}/*.lua")
file(TO_CMAKE_PATH "${SKYUV_TEST_SOURCE_DIR}" source_path)
file(TO_CMAKE_PATH "${SKYUV_TEST_BUILD_DIR}" build_path)
foreach(path IN LISTS text_files)
	file(READ "${path}" content)
	string(FIND "${content}" "${source_path}" source_position)
	string(FIND "${content}" "${build_path}" build_position)
	if(NOT source_position EQUAL -1 OR NOT build_position EQUAL -1)
		message(FATAL_ERROR "安装文件泄漏源码树或构建树路径：${path}")
	endif()
endforeach()

execute_process(
	COMMAND "${executable}" "examples/skyuv.conf"
	WORKING_DIRECTORY "${temp_root}"
	TIMEOUT 15
	RESULT_VARIABLE run_result
	OUTPUT_VARIABLE run_output
	ERROR_VARIABLE run_error
)
set(run_log "${run_output}\n${run_error}")
if(NOT run_result EQUAL 0)
	message(FATAL_ERROR "从安装树外启动示例失败（${run_result}）：\n${run_log}")
endif()
if(NOT run_log MATCHES "SKYUV_RUNTIME_SMOKE_OK")
	message(FATAL_ERROR "安装树示例缺少成功标记：\n${run_log}")
endif()

set(deployment_root "${temp_root}/skyuv external config ${test_id}")
file(REMOVE_RECURSE "${deployment_root}")
file(MAKE_DIRECTORY "${deployment_root}")
file(READ "${config}" external_config)
string(REPLACE "logger = nil" "logger = \"node.log\"" external_config "${external_config}")
file(WRITE "${deployment_root}/node.conf" "${external_config}")
if(NOT WIN32)
	# 模拟只读程序树；标准输出和外置日志都不能要求安装树可写。
	file(CHMOD_RECURSE "${install_root}"
		PERMISSIONS OWNER_READ OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE)
endif()
execute_process(
	COMMAND "${executable}" --runtime-paths "examples/skyuv.conf"
	WORKING_DIRECTORY "${temp_root}"
	TIMEOUT 15
	RESULT_VARIABLE readonly_result
	OUTPUT_VARIABLE readonly_output
	ERROR_VARIABLE readonly_error
)
execute_process(
	COMMAND "${executable}" --runtime-paths "${deployment_root}/node.conf"
	WORKING_DIRECTORY "${temp_root}"
	TIMEOUT 15
	RESULT_VARIABLE external_result
	OUTPUT_VARIABLE external_output
	ERROR_VARIABLE external_error
)
if(NOT WIN32)
	# 恢复仅用于清理临时安装树，即使启动失败也先恢复权限再报告。
	file(CHMOD_RECURSE "${install_root}" PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE)
endif()
set(readonly_log "${readonly_output}\n${readonly_error}")
if(NOT readonly_result EQUAL 0 OR NOT readonly_log MATCHES "SKYUV_RUNTIME_SMOKE_OK")
	message(FATAL_ERROR "只读安装树标准输出模式失败：\n${readonly_log}")
endif()
if(NOT external_result EQUAL 0 OR NOT EXISTS "${deployment_root}/log/node.log")
	message(FATAL_ERROR "新模式安装树外配置启动失败：\n${external_output}\n${external_error}")
endif()
file(READ "${deployment_root}/log/node.log" external_log)
if(NOT external_log MATCHES "SKYUV_RUNTIME_SMOKE_OK" OR EXISTS "${deployment_root}/run")
	message(FATAL_ERROR "新模式日志或前台运行目录验证失败：\n${external_log}")
endif()
file(REMOVE_RECURSE "${deployment_root}" "${install_root}")
message(STATUS "安装树及新模式外置配置运行验证通过")
