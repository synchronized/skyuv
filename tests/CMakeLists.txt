if(NOT SKYUV_BUILD_TESTS OR NOT BUILD_TESTING)
  return()
endif()

include("${CMAKE_CURRENT_SOURCE_DIR}/portable.cmake")

add_test(
  NAME install.runtime_smoke
  COMMAND
    "${CMAKE_COMMAND}"
    "-DSKYUV_TEST_BUILD_DIR=${PROJECT_BINARY_DIR}"
    "-DSKYUV_TEST_SOURCE_DIR=${PROJECT_SOURCE_DIR}"
    "-DSKYUV_TEST_CONFIG=$<CONFIG>"
    "-DSKYUV_TEST_EXECUTABLE_NAME=$<TARGET_FILE_NAME:skyuv_skynet_portable>"
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
    "-DSKYUV_TEST_EXECUTABLE_NAME=$<TARGET_FILE_NAME:skyuv_skynet_portable>"
    "-DSKYUV_TEST_MODULE_SUFFIX=$<TARGET_FILE_SUFFIX:skyuv_portable_service_snlua>"
    "-DSKYUV_TEST_PACKAGE_EXTENSION=${SKYUV_PACKAGE_EXTENSION}"
    -P "${CMAKE_CURRENT_SOURCE_DIR}/scripts/package_archive_contents.cmake"
)
set_tests_properties(
  package.archive_contents
  PROPERTIES DEPENDS install.runtime_smoke LABELS delivery TIMEOUT 60
)

if(MSVC)
  add_test(
    NAME package.static_crt
    COMMAND
      "${CMAKE_COMMAND}"
      "-DSKYUV_TEST_ARCHIVE_DIR=${PROJECT_BINARY_DIR}/package-test-output"
      "-DSKYUV_TEST_LINKER=${CMAKE_LINKER}"
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
  NAME package.extracted_runtime
  COMMAND
    "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/scripts/package_extracted_runtime.py"
    --archive-directory "${PROJECT_BINARY_DIR}/package-test-output"
    --archive-extension "${SKYUV_PACKAGE_EXTENSION}"
    --allocator "${SKYUV_ALLOCATOR}"
    --executable-name "$<TARGET_FILE_NAME:skyuv_skynet_portable>"
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
if(TARGET skyuv_skynet_portable)
  add_test(
    NAME benchmark.actor_ping_pong
    COMMAND
      "${Python3_EXECUTABLE}" "${PROJECT_SOURCE_DIR}/benchmarks/actor_ping_pong.py"
      --implementation skyuv --build-type "$<CONFIG>" --allocator "${SKYUV_ALLOCATOR}"
      --compiler "${CMAKE_C_COMPILER_ID} ${CMAKE_C_COMPILER_VERSION}"
      --warmup 0.02 --duration 0.05 --iterations 2
      --executable "$<TARGET_FILE:skyuv_skynet_portable>"
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
      --skyuv-executable "$<TARGET_FILE:skyuv_skynet_portable>"
      --skyuv-config "${PROJECT_BINARY_DIR}/3rd/skyuv-benchmark-actor.conf"
      --upstream-executable "$<TARGET_FILE:skyuv_skynet_portable>"
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
      --executable "$<TARGET_FILE:skyuv_skynet_portable>"
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
      --executable "$<TARGET_FILE:skyuv_skynet_portable>"
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
      --executable "$<TARGET_FILE:skyuv_skynet_portable>"
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
  add_executable(skyuv_test_lua test_lua.c)
  target_link_libraries(skyuv_test_lua PRIVATE skyuv::lua cmocka::cmocka)
  skyuv_target_compile_warnings(skyuv_test_lua)
  add_test(NAME lua.basic COMMAND skyuv_test_lua)
endif()
