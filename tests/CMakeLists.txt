if(NOT SKYUV_BUILD_TESTS OR NOT BUILD_TESTING)
  return()
endif()

include("${CMAKE_CURRENT_SOURCE_DIR}/portable.cmake")

find_package(Python3 REQUIRED COMPONENTS Interpreter)
add_test(
  NAME benchmark.protocol
  COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/scripts/benchmark_protocol_test.py"
)
set_tests_properties(benchmark.protocol PROPERTIES WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}")
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
  NAME benchmark.tcp_short_connection
  COMMAND
    "${Python3_EXECUTABLE}"
    "${CMAKE_CURRENT_SOURCE_DIR}/scripts/tcp_short_connection_benchmark_test.py"
)
set_tests_properties(
  benchmark.tcp_short_connection PROPERTIES WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}" TIMEOUT 20
)
add_test(
  NAME benchmark.tcp_backpressure
  COMMAND
    "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/scripts/tcp_backpressure_benchmark_test.py"
)
set_tests_properties(
  benchmark.tcp_backpressure PROPERTIES WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}" TIMEOUT 20
)
add_test(
  NAME benchmark.udp_request_reply
  COMMAND
    "${Python3_EXECUTABLE}"
    "${CMAKE_CURRENT_SOURCE_DIR}/scripts/udp_request_reply_benchmark_test.py"
)
set_tests_properties(
  benchmark.udp_request_reply PROPERTIES WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}" TIMEOUT 20
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
  )
endif()

add_executable(skyuv_test_allocator test_allocator.c)
target_link_libraries(skyuv_test_allocator PRIVATE skyuv::allocator cmocka::cmocka)
skyuv_target_compile_warnings(skyuv_test_allocator)
add_test(NAME allocator.basic COMMAND skyuv_test_allocator)

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
skyuv_target_compile_warnings(skyuv_test_dynamic_module)

add_executable(skyuv_test_dynamic test_dynamic.c)
target_compile_definitions(
  skyuv_test_dynamic
  PRIVATE SKYUV_TEST_MODULE_PATH="$<TARGET_FILE:skyuv_test_dynamic_module>"
)
target_link_libraries(skyuv_test_dynamic PRIVATE skyuv::platform cmocka::cmocka)
add_dependencies(skyuv_test_dynamic skyuv_test_dynamic_module)
skyuv_target_compile_warnings(skyuv_test_dynamic)
add_test(NAME platform.dynamic COMMAND skyuv_test_dynamic)

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
  skyuv_test_socket_internal PRIVATE skyuv::socket_core skyuv::libuv cmocka::cmocka
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
  PRIVATE skyuv_socket_stress_core skyuv::platform skyuv::libuv cmocka::cmocka
)
skyuv_target_compile_warnings(skyuv_test_socket_stress)
add_test(NAME socket.stress COMMAND skyuv_test_socket_stress)
set_tests_properties(socket.stress PROPERTIES TIMEOUT 20)

if(TARGET skyuv_lua)
  add_executable(skyuv_test_lua test_lua.c)
  target_link_libraries(skyuv_test_lua PRIVATE skyuv::lua cmocka::cmocka)
  skyuv_target_compile_warnings(skyuv_test_lua)
  add_test(NAME lua.basic COMMAND skyuv_test_lua)
endif()
