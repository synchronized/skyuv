cmake_minimum_required(VERSION 3.23)

foreach(variable IN ITEMS SKYUV_TEST_SOURCE_DIR SKYUV_TEST_BUILD_DIR SKYUV_TEST_CONFIG
                         SKYUV_TEST_COMPILER SKYUV_TEST_GENERATOR SKYUV_TEST_LUA)
  if(NOT DEFINED ${variable} OR "${${variable}}" STREQUAL "")
    message(FATAL_ERROR "缺少消费测试参数：${variable}")
  endif()
endforeach()
set(test_root "${SKYUV_TEST_BUILD_DIR}/tests/dynamic consumer space")
set(prefix "${test_root}/development prefix")
file(REMOVE_RECURSE "${test_root}")

function(run_checked)
  execute_process(COMMAND ${ARGV} RESULT_VARIABLE result OUTPUT_VARIABLE output ERROR_VARIABLE error)
  if(NOT result EQUAL 0)
    message(FATAL_ERROR "外部消费步骤失败：${ARGV}\n${output}\n${error}")
  endif()
endfunction()

run_checked("${CMAKE_COMMAND}" --install "${SKYUV_TEST_BUILD_DIR}"
  --config "${SKYUV_TEST_CONFIG}" --component Development --prefix "${prefix}")
# 移动安装前缀后再消费，防止导出文件暗中依赖原安装位置。
set(relocated_prefix "${test_root}/relocated development prefix")
file(RENAME "${prefix}" "${relocated_prefix}")
set(prefix "${relocated_prefix}")
set(generator_arguments -G "${SKYUV_TEST_GENERATOR}")
if(DEFINED SKYUV_TEST_GENERATOR_PLATFORM AND NOT "${SKYUV_TEST_GENERATOR_PLATFORM}" STREQUAL "")
  list(APPEND generator_arguments -A "${SKYUV_TEST_GENERATOR_PLATFORM}")
endif()
if(DEFINED SKYUV_TEST_GENERATOR_TOOLSET AND NOT "${SKYUV_TEST_GENERATOR_TOOLSET}" STREQUAL "")
  list(APPEND generator_arguments -T "${SKYUV_TEST_GENERATOR_TOOLSET}")
endif()
foreach(mode IN ITEMS package embedded)
  set(child "${test_root}/${mode}")
  if(mode STREQUAL "package")
    set(mode_arguments "-DCMAKE_PREFIX_PATH=${prefix}")
  else()
    set(mode_arguments "-DSKYUV_SOURCE_DIR=${SKYUV_TEST_SOURCE_DIR}"
      -DSKYUV_ALLOCATOR=system -DSKYUV_BUILD_TESTS=OFF)
  endif()
  run_checked("${CMAKE_COMMAND}" -S "${SKYUV_TEST_SOURCE_DIR}/examples/cmake-consumer"
    -B "${child}" ${generator_arguments} "-DCMAKE_C_COMPILER=${SKYUV_TEST_COMPILER}"
    "-DCMAKE_BUILD_TYPE=${SKYUV_TEST_CONFIG}" "-DSKYUV_LUA_EXECUTABLE=${SKYUV_TEST_LUA}"
    -DBUILD_TESTING=ON ${mode_arguments})
  run_checked("${CMAKE_COMMAND}" --build "${child}" --config "${SKYUV_TEST_CONFIG}"
    --target skyuv_consumer skyuv_consumer_smoke --parallel 2)
  run_checked("${CMAKE_CTEST_COMMAND}" --test-dir "${child}" -C "${SKYUV_TEST_CONFIG}"
    --output-on-failure)
endforeach()
message(STATUS "源码嵌入和安装 package 的独立 Lua 动态 require 验证通过")
