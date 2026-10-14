if(NOT SKYUV_BUILD_TESTS OR NOT BUILD_TESTING)
  return()
endif()

include("${CMAKE_CURRENT_SOURCE_DIR}/portable.cmake")

add_test(
  NAME cmake.embed
  COMMAND
    "${CMAKE_COMMAND}"
    -S "${CMAKE_CURRENT_SOURCE_DIR}/fixtures/cmake-consumer"
    -B "${PROJECT_BINARY_DIR}/tests/cmake-consumer"
    "-DSKYUV_SOURCE_DIR=${PROJECT_SOURCE_DIR}"
    -DSKYUV_BUILD_TESTS=OFF
)

add_test(
  NAME install.runtime_smoke
  COMMAND
    "${CMAKE_COMMAND}"
    "-DSKYUV_TEST_BUILD_DIR=${PROJECT_BINARY_DIR}"
    "-DSKYUV_TEST_SOURCE_DIR=${PROJECT_SOURCE_DIR}"
    "-DSKYUV_TEST_CONFIG=$<CONFIG>"
    "-DSKYUV_TEST_EXECUTABLE_NAME=$<TARGET_FILE_NAME:skyuv>"
    -P "${CMAKE_CURRENT_SOURCE_DIR}/scripts/install_runtime_smoke.cmake"
)
set_tests_properties(install.runtime_smoke PROPERTIES LABELS delivery TIMEOUT 30)

add_test(
  NAME package.archive_contents
  COMMAND
    "${CMAKE_COMMAND}"
    "-DSKYUV_TEST_BUILD_DIR=${PROJECT_BINARY_DIR}"
    "-DSKYUV_TEST_CPACK_COMMAND=${CMAKE_CPACK_COMMAND}"
    "-DSKYUV_TEST_CONFIG=$<CONFIG>"
    "-DSKYUV_TEST_EXECUTABLE_NAME=$<TARGET_FILE_NAME:skyuv>"
    "-DSKYUV_TEST_MODULE_SUFFIX=$<TARGET_FILE_SUFFIX:skyuv_portable_service_snlua>"
    "-DSKYUV_TEST_PACKAGE_EXTENSION=${SKYUV_PACKAGE_EXTENSION}"
    -P "${CMAKE_CURRENT_SOURCE_DIR}/scripts/package_archive_contents.cmake"
)
set_tests_properties(
  package.archive_contents
  PROPERTIES DEPENDS install.runtime_smoke LABELS delivery TIMEOUT 60
)

if(MSVC OR CMAKE_C_SIMULATE_ID STREQUAL "MSVC")
  if(CMAKE_LINKER MATCHES "lld-link")
    find_program(SKYUV_TEST_PE_READER NAMES dumpbin llvm-readobj)
    if(NOT SKYUV_TEST_PE_READER AND CMAKE_OBJDUMP)
      set(SKYUV_TEST_PE_READER "${CMAKE_OBJDUMP}")
      set(SKYUV_TEST_PE_READER_STYLE "objdump")
    endif()
    if(NOT SKYUV_TEST_PE_READER)
      message(FATAL_ERROR "lld-link 下静态 CRT 审计需要 dumpbin、llvm-readobj 或 CMAKE_OBJDUMP")
    endif()
    if(NOT SKYUV_TEST_PE_READER_STYLE)
      if(SKYUV_TEST_PE_READER MATCHES "llvm-readobj")
        set(SKYUV_TEST_PE_READER_STYLE "readobj")
      else()
        set(SKYUV_TEST_PE_READER_STYLE "dumpbin")
      endif()
    endif()
  else()
    set(SKYUV_TEST_PE_READER "${CMAKE_LINKER}")
    set(SKYUV_TEST_PE_READER_STYLE "dumpbin")
  endif()
  add_test(
    NAME package.static_crt
    COMMAND
      "${CMAKE_COMMAND}"
      "-DSKYUV_TEST_ARCHIVE_DIR=${PROJECT_BINARY_DIR}/package-test-output"
      "-DSKYUV_TEST_PE_READER=${SKYUV_TEST_PE_READER}"
      "-DSKYUV_TEST_PE_READER_STYLE=${SKYUV_TEST_PE_READER_STYLE}"
      "-DSKYUV_TEST_PACKAGE_EXTENSION=${SKYUV_PACKAGE_EXTENSION}"
      -P "${CMAKE_CURRENT_SOURCE_DIR}/scripts/package_static_crt.cmake"
  )
  set_tests_properties(
    package.static_crt
    PROPERTIES DEPENDS package.archive_contents LABELS delivery TIMEOUT 30
  )
endif()

find_package(Python3 REQUIRED COMPONENTS Interpreter)
add_test(
  NAME skynet.portable.paths_legacy
  COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/scripts/runtime_paths_legacy.py"
    --executable "$<TARGET_FILE:skyuv>"
    --config-template "${PROJECT_BINARY_DIR}/3rd/skyuv-portable-logger-file.conf"
)
set_tests_properties(skynet.portable.paths_legacy PROPERTIES TIMEOUT 45)
add_test(
  NAME package.extracted_runtime
  COMMAND
    "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/scripts/package_extracted_runtime.py"
    --archive-directory "${PROJECT_BINARY_DIR}/package-test-output"
    --archive-extension "${SKYUV_PACKAGE_EXTENSION}"
    --allocator "${SKYUV_ALLOCATOR}"
    --executable-name "$<TARGET_FILE_NAME:skyuv>"
    --project-version "${SKYUV_RELEASE_VERSION}"
)
set_tests_properties(
  package.extracted_runtime
  PROPERTIES DEPENDS package.archive_contents LABELS delivery TIMEOUT 45
)
add_test(
  NAME benchmark.protocol
  COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/scripts/benchmark_protocol_test.py"
)
set_tests_properties(benchmark.protocol PROPERTIES WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}")
add_test(
  NAME benchmark.soak_protocol
  COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/scripts/soak_protocol_test.py"
)
set_tests_properties(benchmark.soak_protocol PROPERTIES WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}")
add_test(
  NAME benchmark.process_metrics
  COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/scripts/process_metrics_test.py"
)
set_tests_properties(benchmark.process_metrics PROPERTIES WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}")
add_test(
  NAME benchmark.baseline_report
  COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/scripts/baseline_report_test.py"
)
set_tests_properties(benchmark.baseline_report PROPERTIES WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}")
add_test(
  NAME benchmark.baseline_noise
  COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/scripts/baseline_noise_test.py"
)
set_tests_properties(benchmark.baseline_noise PROPERTIES WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}")
add_test(
  NAME benchmark.runner_preflight
  COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/scripts/runner_preflight_test.py"
)
set_tests_properties(benchmark.runner_preflight PROPERTIES WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}")
add_test(
  NAME benchmark.tcp_echo
  COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/scripts/tcp_echo_benchmark_test.py"
)
set_tests_properties(benchmark.tcp_echo PROPERTIES WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}" TIMEOUT 20)
add_test(
  NAME benchmark.tcp_comparison_protocol
  COMMAND
    "${Python3_EXECUTABLE}" "${PROJECT_SOURCE_DIR}/benchmarks/compare_tcp.py"
    --skyuv-executable "${Python3_EXECUTABLE}"
    --skyuv-config "${PROJECT_SOURCE_DIR}/tests/fixtures/benchmark_echo_server.py"
    --upstream-executable "${Python3_EXECUTABLE}"
    --upstream-config "${PROJECT_SOURCE_DIR}/tests/fixtures/benchmark_echo_server.py"
    --build-type "Release" --allocator "system" --compiler "protocol-test"
    --port 25282 --message-size 17 --warmup 0.01 --duration 0.02 --iterations 1
    --output-directory "${PROJECT_BINARY_DIR}/benchmarks/tcp-comparison-test"
)
set_tests_properties(
  benchmark.tcp_comparison_protocol PROPERTIES WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}" TIMEOUT 20
)
add_test(
  NAME benchmark.tcp_short_comparison_protocol
  COMMAND
    "${Python3_EXECUTABLE}" "${PROJECT_SOURCE_DIR}/benchmarks/compare_tcp.py"
    --skyuv-executable "${Python3_EXECUTABLE}"
    --skyuv-config "${PROJECT_SOURCE_DIR}/tests/fixtures/benchmark_echo_server.py"
    --upstream-executable "${Python3_EXECUTABLE}"
    --upstream-config "${PROJECT_SOURCE_DIR}/tests/fixtures/benchmark_echo_server.py"
    --build-type "Release" --allocator "system" --compiler "protocol-test"
    --scenario tcp_short_connection --port 25283 --message-size 23
    --warmup 0.01 --duration 0.02 --iterations 1
    --output-directory "${PROJECT_BINARY_DIR}/benchmarks/tcp-short-comparison-test"
)
set_tests_properties(
  benchmark.tcp_short_comparison_protocol
  PROPERTIES WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}" TIMEOUT 20
)
add_test(
  NAME benchmark.tcp_backpressure_comparison_protocol
  COMMAND
    "${Python3_EXECUTABLE}" "${PROJECT_SOURCE_DIR}/benchmarks/compare_tcp.py"
    --skyuv-executable "${Python3_EXECUTABLE}"
    --skyuv-config "${PROJECT_SOURCE_DIR}/tests/fixtures/benchmark_stream_server.py"
    --upstream-executable "${Python3_EXECUTABLE}"
    --upstream-config "${PROJECT_SOURCE_DIR}/tests/fixtures/benchmark_stream_server.py"
    --build-type "Release" --allocator "system" --compiler "protocol-test"
    --scenario tcp_backpressure --port 25285 --warmup 0.01 --duration 0.03 --iterations 1
    --initial-pause 0.01 --read-delay 0.001 --read-size 1024
    --require-log-marker "SKYUV_BACKPRESSURE_WARNING"
    --output-directory "${PROJECT_BINARY_DIR}/benchmarks/tcp-backpressure-comparison-test"
)
set_tests_properties(
  benchmark.tcp_backpressure_comparison_protocol
  PROPERTIES WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}" TIMEOUT 20
)
add_test(
  NAME benchmark.tcp_short_connection
  COMMAND
    "${Python3_EXECUTABLE}"
    "${CMAKE_CURRENT_SOURCE_DIR}/scripts/tcp_short_connection_benchmark_test.py"
)
set_tests_properties(
  benchmark.tcp_short_connection
  PROPERTIES LABELS stability WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}" TIMEOUT 20
)
add_test(
  NAME benchmark.tcp_backpressure
  COMMAND
    "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/scripts/tcp_backpressure_benchmark_test.py"
)
add_test(
  NAME benchmark.backpressure_queue
  COMMAND "$<TARGET_FILE:skyuv_lua_interpreter>"
    "${CMAKE_CURRENT_SOURCE_DIR}/scripts/backpressure_queue_test.lua"
    "${CMAKE_CURRENT_SOURCE_DIR}/fixtures/skyuv_benchmark_tcp_backpressure.lua"
)
set_tests_properties(benchmark.backpressure_queue PROPERTIES LABELS stability TIMEOUT 10)
set_tests_properties(
  benchmark.tcp_backpressure
  PROPERTIES LABELS stability WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}" TIMEOUT 20
)
add_test(
  NAME benchmark.udp_request_reply
  COMMAND
    "${Python3_EXECUTABLE}"
    "${CMAKE_CURRENT_SOURCE_DIR}/scripts/udp_request_reply_benchmark_test.py"
)
set_tests_properties(
  benchmark.udp_request_reply
  PROPERTIES LABELS stability WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}" TIMEOUT 20
)
add_test(
  NAME benchmark.udp_comparison_protocol
  COMMAND
    "${Python3_EXECUTABLE}" "${PROJECT_SOURCE_DIR}/benchmarks/compare_udp.py"
    --skyuv-executable "${Python3_EXECUTABLE}"
    --skyuv-config "${PROJECT_SOURCE_DIR}/tests/fixtures/benchmark_udp_server.py"
    --upstream-executable "${Python3_EXECUTABLE}"
    --upstream-config "${PROJECT_SOURCE_DIR}/tests/fixtures/benchmark_udp_server.py"
    --build-type "Release" --allocator "system" --compiler "protocol-test"
    --port 25287 --message-size 31 --warmup 0.01 --duration 0.03 --iterations 1
    --output-directory "${PROJECT_BINARY_DIR}/benchmarks/udp-comparison-test"
)
set_tests_properties(
  benchmark.udp_comparison_protocol PROPERTIES WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}" TIMEOUT 20
)
if(TARGET skyuv)
  add_test(
    NAME benchmark.actor_ping_pong
    COMMAND
      "${Python3_EXECUTABLE}" "${PROJECT_SOURCE_DIR}/benchmarks/actor_ping_pong.py"
      --implementation skyuv --build-type "$<CONFIG>" --allocator "${SKYUV_ALLOCATOR}"
      --compiler "${CMAKE_C_COMPILER_ID} ${CMAKE_C_COMPILER_VERSION}"
      --warmup 0.02 --duration 0.05 --iterations 2
      --executable "$<TARGET_FILE:skyuv>"
      --config "${PROJECT_BINARY_DIR}/3rd/skyuv-benchmark-actor.conf"
      --output "${PROJECT_BINARY_DIR}/benchmarks/actor-ping-pong-test.json"
  )
  set_tests_properties(
    benchmark.actor_ping_pong PROPERTIES WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}" TIMEOUT 20
  )
  add_test(
    NAME benchmark.actor_comparison_protocol
    COMMAND
      "${Python3_EXECUTABLE}" "${PROJECT_SOURCE_DIR}/benchmarks/compare_actor.py"
      --skyuv-executable "$<TARGET_FILE:skyuv>"
      --skyuv-config "${PROJECT_BINARY_DIR}/3rd/skyuv-benchmark-actor.conf"
      --upstream-executable "$<TARGET_FILE:skyuv>"
      --upstream-config "${PROJECT_BINARY_DIR}/3rd/skyuv-benchmark-actor.conf"
      --build-type "$<CONFIG>" --allocator "${SKYUV_ALLOCATOR}"
      --compiler "${CMAKE_C_COMPILER_ID} ${CMAKE_C_COMPILER_VERSION}"
      --warmup 0.01 --duration 0.03 --iterations 1
      --output-directory "${PROJECT_BINARY_DIR}/benchmarks/actor-comparison-test"
  )
  set_tests_properties(
    benchmark.actor_comparison_protocol
    PROPERTIES WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}" TIMEOUT 20
  )
  add_test(
    NAME benchmark.actor_multi_producer
    COMMAND
      "${Python3_EXECUTABLE}" "${PROJECT_SOURCE_DIR}/benchmarks/actor_multi_producer.py"
      --implementation skyuv --build-type "$<CONFIG>" --allocator "${SKYUV_ALLOCATOR}"
      --compiler "${CMAKE_C_COMPILER_ID} ${CMAKE_C_COMPILER_VERSION}"
      --warmup 0.02 --duration 0.05 --iterations 2 --producers 4
      --executable "$<TARGET_FILE:skyuv>"
      --config "${PROJECT_BINARY_DIR}/3rd/skyuv-benchmark-actor-multi.conf"
      --output "${PROJECT_BINARY_DIR}/benchmarks/actor-multi-producer-test.json"
  )
  set_tests_properties(
    benchmark.actor_multi_producer PROPERTIES WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}" TIMEOUT 20
    LABELS stability
  )
  add_test(
    NAME benchmark.actor_ring
    COMMAND
      "${Python3_EXECUTABLE}" "${PROJECT_SOURCE_DIR}/benchmarks/actor_ping_pong.py"
      --implementation skyuv --build-type "$<CONFIG>" --allocator "${SKYUV_ALLOCATOR}"
      --compiler "${CMAKE_C_COMPILER_ID} ${CMAKE_C_COMPILER_VERSION}"
      --warmup 0.02 --duration 0.05 --iterations 2 --actors 8
      --benchmark-name actor_ring --scenario token_ring --sample-marker SKYUV_ACTOR_RING_SAMPLE
      --executable "$<TARGET_FILE:skyuv>"
      --config "${PROJECT_BINARY_DIR}/3rd/skyuv-benchmark-actor-ring.conf"
      --output "${PROJECT_BINARY_DIR}/benchmarks/actor-ring-test.json"
  )
  set_tests_properties(
    benchmark.actor_ring PROPERTIES WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}" TIMEOUT 20
    LABELS stability
  )
  add_test(
    NAME benchmark.timer_burst
    COMMAND
      "${Python3_EXECUTABLE}" "${PROJECT_SOURCE_DIR}/benchmarks/actor_ping_pong.py"
      --implementation skyuv --build-type "$<CONFIG>" --allocator "${SKYUV_ALLOCATOR}"
      --compiler "${CMAKE_C_COMPILER_ID} ${CMAKE_C_COMPILER_VERSION}"
      --warmup 0.02 --duration 0.05 --iterations 2 --actors 1 --timer-count 1000
      --benchmark-name timer_burst --scenario concentrated_deadline
      --sample-marker SKYUV_TIMER_BURST_SAMPLE
      --executable "$<TARGET_FILE:skyuv>"
      --config "${PROJECT_BINARY_DIR}/3rd/skyuv-benchmark-timer-burst.conf"
      --output "${PROJECT_BINARY_DIR}/benchmarks/timer-burst-test.json"
  )
  set_tests_properties(
    benchmark.timer_burst PROPERTIES WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}" TIMEOUT 20
    LABELS stability
  )
endif()

add_executable(skyuv_test_allocator test_allocator.c)
target_link_libraries(skyuv_test_allocator PRIVATE skyuv::allocator cmocka::cmocka)
skyuv_target_compile_warnings(skyuv_test_allocator)
add_test(NAME allocator.basic COMMAND skyuv_test_allocator)

add_executable(skyuv_test_memory test_memory.c)
target_link_libraries(skyuv_test_memory PRIVATE skyuv::memory cmocka::cmocka)
skyuv_target_compile_warnings(skyuv_test_memory)
add_test(NAME memory.basic COMMAND skyuv_test_memory)

add_executable(skyuv_test_thread test_thread.c)
target_link_libraries(skyuv_test_thread PRIVATE skyuv::platform cmocka::cmocka)
skyuv_target_compile_warnings(skyuv_test_thread)
add_test(NAME platform.thread COMMAND skyuv_test_thread)

add_executable(skyuv_test_time test_time.c)
target_link_libraries(skyuv_test_time PRIVATE skyuv::platform cmocka::cmocka)
skyuv_target_compile_warnings(skyuv_test_time)
add_test(NAME platform.time COMMAND skyuv_test_time)

add_library(skyuv_test_dynamic_module SHARED dynamic_module.c)
set_target_properties(skyuv_test_dynamic_module PROPERTIES WINDOWS_EXPORT_ALL_SYMBOLS ON)
target_include_directories(skyuv_test_dynamic_module PRIVATE "${PROJECT_SOURCE_DIR}/include")
skyuv_target_compile_warnings(skyuv_test_dynamic_module)

add_executable(skyuv_test_dynamic test_dynamic.c)
set_target_properties(
  skyuv_test_dynamic PROPERTIES ENABLE_EXPORTS TRUE WINDOWS_EXPORT_ALL_SYMBOLS TRUE
)
target_link_libraries(skyuv_test_dynamic PRIVATE skyuv::memory skyuv::platform cmocka::cmocka)
target_link_libraries(skyuv_test_dynamic_module PRIVATE skyuv_test_dynamic)
if(APPLE)
  # Mach-O 模块在装载时从主程序解析统一内存符号，链接阶段保留这些未定义引用。
  target_link_options(skyuv_test_dynamic_module PRIVATE "LINKER:-undefined,dynamic_lookup")
endif()
skyuv_target_compile_warnings(skyuv_test_dynamic)
add_custom_target(skyuv_test_dynamic_bundle ALL DEPENDS skyuv_test_dynamic_module)
add_test(NAME platform.dynamic COMMAND skyuv_test_dynamic "$<TARGET_FILE:skyuv_test_dynamic_module>")

add_executable(skyuv_test_atomic test_atomic.c)
target_link_libraries(skyuv_test_atomic PRIVATE skyuv::platform cmocka::cmocka)
skyuv_target_compile_warnings(skyuv_test_atomic)
add_test(NAME platform.atomic COMMAND skyuv_test_atomic)

add_executable(skyuv_test_socket_internal test_socket_internal.c)
target_include_directories(
  skyuv_test_socket_internal
  PRIVATE "${PROJECT_SOURCE_DIR}/src/socket" "${PROJECT_SOURCE_DIR}/include"
)
target_link_libraries(
  skyuv_test_socket_internal
  PRIVATE skyuv::socket_core skyuv::memory skyuv::libuv cmocka::cmocka
)
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
	target_compile_definitions(skyuv_test_socket_internal PRIVATE _POSIX_C_SOURCE=200112L)
endif()
if(UNIX)
  target_compile_definitions(skyuv_test_socket_internal PRIVATE SKYUV_SOCKET_EXTERNAL_FD=1)
else()
  target_compile_definitions(skyuv_test_socket_internal PRIVATE SKYUV_SOCKET_EXTERNAL_FD=0)
endif()
skyuv_target_compile_warnings(skyuv_test_socket_internal)
add_test(NAME socket.internal COMMAND skyuv_test_socket_internal)

add_library(skyuv_socket_stress_core STATIC "${PROJECT_SOURCE_DIR}/src/socket/socket_internal.c")
target_include_directories(
  skyuv_socket_stress_core PRIVATE "${PROJECT_SOURCE_DIR}/src/socket" "${PROJECT_SOURCE_DIR}/include"
)
target_compile_definitions(skyuv_socket_stress_core PRIVATE SKYUV_SOCKET_SLOT_BITS=8)
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
  target_compile_definitions(skyuv_socket_stress_core PRIVATE _POSIX_C_SOURCE=200112L)
endif()
if(UNIX)
  target_compile_definitions(skyuv_socket_stress_core PRIVATE SKYUV_SOCKET_EXTERNAL_FD=1)
else()
  target_compile_definitions(skyuv_socket_stress_core PRIVATE SKYUV_SOCKET_EXTERNAL_FD=0)
endif()
target_link_libraries(skyuv_socket_stress_core PRIVATE skyuv::platform skyuv::libuv)
skyuv_target_compile_warnings(skyuv_socket_stress_core)

add_executable(skyuv_test_socket_stress test_socket_stress.c)
target_include_directories(
  skyuv_test_socket_stress PRIVATE "${PROJECT_SOURCE_DIR}/src/socket" "${PROJECT_SOURCE_DIR}/include"
)
target_compile_definitions(skyuv_test_socket_stress PRIVATE SKYUV_SOCKET_SLOT_BITS=8)
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
  target_compile_definitions(skyuv_test_socket_stress PRIVATE _POSIX_C_SOURCE=200112L)
endif()
target_link_libraries(
  skyuv_test_socket_stress
  PRIVATE skyuv_socket_stress_core skyuv::memory skyuv::platform skyuv::libuv cmocka::cmocka
)
skyuv_target_compile_warnings(skyuv_test_socket_stress)
add_test(NAME socket.stress COMMAND skyuv_test_socket_stress)
set_tests_properties(socket.stress PROPERTIES LABELS stability TIMEOUT 20)

if(TARGET skyuv_lua)
  if(WIN32)
    add_executable(skyuv_test_libuv_initializer test_libuv_initializer.c)
    target_link_libraries(skyuv_test_libuv_initializer PRIVATE uv_a cmocka::cmocka)
    skyuv_target_compile_warnings(skyuv_test_libuv_initializer)
    add_test(NAME libuv.localhost_initializer COMMAND skyuv_test_libuv_initializer)
    add_executable(skyuv_test_libuv_affinity test_libuv_affinity.c)
    # 提取实际编译输入中的单个函数，使用模拟 Win32 掩码验证 32 位以上 CPU。
    file(READ "${SKYUV_BINARY_DIR}/3rd/libuv-patched/src/win/thread.c" affinity_source)
    string(FIND "${affinity_source}" "int uv_thread_setaffinity(" affinity_start)
    string(FIND "${affinity_source}" "int uv_thread_getaffinity(" affinity_end)
    if(affinity_start LESS 0 OR affinity_end LESS affinity_start)
      message(FATAL_ERROR "无法提取 libuv 线程亲和性测试输入，需审查固定版本")
    endif()
    math(EXPR affinity_length "${affinity_end} - ${affinity_start}")
    string(SUBSTRING "${affinity_source}" ${affinity_start} ${affinity_length} affinity_function)
    file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/libuv_affinity_fixture.h" "${affinity_function}")
    target_include_directories(skyuv_test_libuv_affinity PRIVATE "${CMAKE_CURRENT_BINARY_DIR}")
    target_link_libraries(skyuv_test_libuv_affinity PRIVATE uv_a cmocka::cmocka)
    skyuv_target_compile_warnings(skyuv_test_libuv_affinity)
    add_test(NAME libuv.affinity_mask_width COMMAND skyuv_test_libuv_affinity)
    add_executable(skyuv_test_libuv_stat_path test_libuv_stat_path.c)
    # 测试实际补丁副本的完整回退函数，通过系统调用夹具覆盖罕见失败分支。
    file(READ "${SKYUV_BINARY_DIR}/3rd/libuv-patched/src/win/fs.c" stat_source)
    string(FIND "${stat_source}" "static DWORD fs__stat_directory(" stat_start)
    string(FIND "${stat_source}" "static DWORD fs__stat_impl_from_path(" stat_end)
    if(stat_start LESS 0 OR stat_end LESS stat_start)
      message(FATAL_ERROR "无法提取 libuv stat 回退测试输入，需审查固定版本")
    endif()
    math(EXPR stat_length "${stat_end} - ${stat_start}")
    string(SUBSTRING "${stat_source}" ${stat_start} ${stat_length} stat_function)
    file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/libuv_stat_fixture.h" "${stat_function}")
    target_include_directories(skyuv_test_libuv_stat_path PRIVATE
      "${CMAKE_CURRENT_BINARY_DIR}")
    target_link_libraries(skyuv_test_libuv_stat_path PRIVATE uv_a cmocka::cmocka)
    skyuv_target_compile_warnings(skyuv_test_libuv_stat_path)
    add_test(NAME libuv.stat_path_restoration COMMAND skyuv_test_libuv_stat_path)
    set_tests_properties(libuv.stat_path_restoration PROPERTIES TIMEOUT 15)
  endif()
  add_executable(skyuv_test_client_runtime test_client_runtime.c)
  target_link_libraries(skyuv_test_client_runtime PRIVATE skyuv_client_runtime cmocka::cmocka)
  skyuv_target_compile_warnings(skyuv_test_client_runtime)
  add_test(NAME client.runtime COMMAND skyuv_test_client_runtime)
  set_tests_properties(client.runtime PROPERTIES TIMEOUT 30 LABELS stability)
  if(TARGET skyuv_lua_interpreter)
    add_test(NAME lua.interpreter COMMAND skyuv_lua_interpreter -e
      "assert(6 * 7 == 42); assert(require('math').sqrt(81) == 9)")
    add_test(NAME lua.interpreter_error COMMAND skyuv_lua_interpreter -e "error('expected')")
    set_tests_properties(lua.interpreter_error PROPERTIES WILL_FAIL TRUE)
    if(TARGET skyuv_portable_lua_module_client)
      add_test(NAME lua.client_modules COMMAND skyuv_lua_interpreter
        "${CMAKE_CURRENT_SOURCE_DIR}/fixtures/client_modules.lua"
        "$<TARGET_FILE_DIR:skyuv_portable_lua_module_client>/?$<TARGET_FILE_SUFFIX:skyuv_portable_lua_module_client>")
      add_test(NAME lua.client_libuv COMMAND "${Python3_EXECUTABLE}"
        "${CMAKE_CURRENT_SOURCE_DIR}/scripts/client_libuv.py"
        "$<TARGET_FILE:skyuv_lua_interpreter>" "${CMAKE_CURRENT_SOURCE_DIR}/fixtures/client_libuv.lua"
        "$<TARGET_FILE_DIR:skyuv_portable_lua_module_client>/?$<TARGET_FILE_SUFFIX:skyuv_portable_lua_module_client>")
      set_tests_properties(lua.client_libuv PROPERTIES TIMEOUT 60 LABELS stability)
    endif()
  endif()
  add_executable(skyuv_test_lua test_lua.c)
  target_link_libraries(skyuv_test_lua PRIVATE skyuv::lua cmocka::cmocka)
  skyuv_target_compile_warnings(skyuv_test_lua)
  add_test(NAME lua.basic COMMAND skyuv_test_lua)
endif()

if(WIN32)
  if(TARGET skyuv_lua_interpreter AND TARGET skyuv_portable_lua_module_client)
    add_test(NAME windows.project_client_modules
      COMMAND "${SKYUV_BINARY_DIR}/project/bin/$<TARGET_FILE_NAME:skyuv_lua_interpreter>" -e
      "package.cpath = [[${SKYUV_BINARY_DIR}/project/luaclib/?.dll]]; assert(require('client.socket')); assert(require('client.crypt'))")
  endif()
  get_property(windows_runtime_tests DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}" PROPERTY TESTS)
  add_test(NAME windows.runtime_layout COMMAND "${CMAKE_COMMAND}" -P
    "${SKYUV_BINARY_DIR}/generated/verify-runtime-$<CONFIG>.cmake")
  set_tests_properties(windows.runtime_layout PROPERTIES FIXTURES_SETUP windows_runtime_ready)
  # 缺失 DLL 时只报告部署失败，不继续启动会触发系统模态弹窗的程序。
  foreach(windows_runtime_test IN LISTS windows_runtime_tests)
    set_property(TEST ${windows_runtime_test} APPEND PROPERTY FIXTURES_REQUIRED windows_runtime_ready)
  endforeach()
endif()
