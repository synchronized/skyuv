# 便携 Skynet 的配置夹具与 Windows 运行级测试集中维护在此处。
file(TO_CMAKE_PATH "${PROJECT_SOURCE_DIR}/3rd/skynet" SKYUV_PORTABLE_SOURCE_DIR)
file(TO_CMAKE_PATH "${PROJECT_BINARY_DIR}/3rd/portable" SKYUV_PORTABLE_BINARY_DIR)
set(SKYUV_PORTABLE_MODULE_SUFFIX "${CMAKE_SHARED_MODULE_SUFFIX}")
file(TO_CMAKE_PATH "${PROJECT_SOURCE_DIR}/tests/fixtures" SKYUV_EXAMPLE_SOURCE_DIR)
set(SKYUV_PORTABLE_START_SERVICE skyuv_smoke)
configure_file(
  "${PROJECT_SOURCE_DIR}/tests/fixtures/skyuv-portable-smoke.conf.in"
  "${PROJECT_BINARY_DIR}/3rd/skyuv-portable-smoke.conf"
  @ONLY
)
set(SKYUV_PORTABLE_START_SERVICE skyuv_echo)
configure_file(
  "${PROJECT_SOURCE_DIR}/tests/fixtures/skyuv-portable-smoke.conf.in"
  "${PROJECT_BINARY_DIR}/3rd/skyuv-portable-echo.conf"
  @ONLY
)

set(SKYUV_PORTABLE_START_SERVICE skyuv_benchmark_tcp_echo)
configure_file(
  "${PROJECT_SOURCE_DIR}/tests/fixtures/skyuv-portable-smoke.conf.in"
  "${PROJECT_BINARY_DIR}/3rd/skyuv-benchmark-tcp-echo.conf"
  @ONLY
)

set(SKYUV_PORTABLE_START_SERVICE skyuv_benchmark_tcp_backpressure)
configure_file(
  "${PROJECT_SOURCE_DIR}/tests/fixtures/skyuv-portable-smoke.conf.in"
  "${PROJECT_BINARY_DIR}/3rd/skyuv-benchmark-tcp-backpressure.conf"
  @ONLY
)

set(SKYUV_PORTABLE_START_SERVICE skyuv_benchmark_udp_echo)
configure_file(
  "${PROJECT_SOURCE_DIR}/tests/fixtures/skyuv-portable-smoke.conf.in"
  "${PROJECT_BINARY_DIR}/3rd/skyuv-benchmark-udp-echo.conf"
  @ONLY
)

configure_file(
  "${PROJECT_SOURCE_DIR}/tests/fixtures/skyuv-benchmark-actor.conf.in"
  "${PROJECT_BINARY_DIR}/3rd/skyuv-benchmark-actor.conf"
  @ONLY
)
configure_file(
  "${PROJECT_SOURCE_DIR}/tests/fixtures/skyuv-benchmark-actor-multi.conf.in"
  "${PROJECT_BINARY_DIR}/3rd/skyuv-benchmark-actor-multi.conf"
  @ONLY
)
configure_file(
  "${PROJECT_SOURCE_DIR}/tests/fixtures/skyuv-benchmark-actor-ring.conf.in"
  "${PROJECT_BINARY_DIR}/3rd/skyuv-benchmark-actor-ring.conf"
  @ONLY
)
configure_file(
  "${PROJECT_SOURCE_DIR}/tests/fixtures/skyuv-benchmark-timer-burst.conf.in"
  "${PROJECT_BINARY_DIR}/3rd/skyuv-benchmark-timer-burst.conf"
  @ONLY
)
set(SKYUV_PORTABLE_START_SERVICE skyuv_tcp_events)
configure_file(
  "${PROJECT_SOURCE_DIR}/tests/fixtures/skyuv-portable-smoke.conf.in"
  "${PROJECT_BINARY_DIR}/3rd/skyuv-portable-tcp-events.conf"
  @ONLY
)
set(SKYUV_PORTABLE_START_SERVICE skyuv_socket_controls)
configure_file(
  "${PROJECT_SOURCE_DIR}/tests/fixtures/skyuv-portable-smoke.conf.in"
  "${PROJECT_BINARY_DIR}/3rd/skyuv-portable-socket-controls.conf"
  @ONLY
)
set(SKYUV_PORTABLE_START_SERVICE skyuv_udp_events)
configure_file(
  "${PROJECT_SOURCE_DIR}/tests/fixtures/skyuv-portable-smoke.conf.in"
  "${PROJECT_BINARY_DIR}/3rd/skyuv-portable-udp-events.conf"
  @ONLY
)
set(SKYUV_PORTABLE_START_SERVICE skyuv_client_module)
configure_file(
  "${PROJECT_SOURCE_DIR}/tests/fixtures/skyuv-portable-smoke.conf.in"
  "${PROJECT_BINARY_DIR}/3rd/skyuv-portable-client-module.conf"
  @ONLY
)
set(SKYUV_PORTABLE_START_SERVICE skyuv_client_socket)
configure_file(
  "${PROJECT_SOURCE_DIR}/tests/fixtures/skyuv-portable-smoke.conf.in"
  "${PROJECT_BINARY_DIR}/3rd/skyuv-portable-client-socket.conf"
  @ONLY
)
set(SKYUV_PORTABLE_START_SERVICE skyuv_client_stdin)
configure_file(
  "${PROJECT_SOURCE_DIR}/tests/fixtures/skyuv-portable-smoke.conf.in"
  "${PROJECT_BINARY_DIR}/3rd/skyuv-portable-client-stdin.conf"
  @ONLY
)
set(SKYUV_PORTABLE_START_SERVICE skyuv_bson)
configure_file(
  "${PROJECT_SOURCE_DIR}/tests/fixtures/skyuv-portable-smoke.conf.in"
  "${PROJECT_BINARY_DIR}/3rd/skyuv-portable-bson.conf"
  @ONLY
)
set(SKYUV_PORTABLE_START_SERVICE skyuv_sproto)
configure_file(
  "${PROJECT_SOURCE_DIR}/tests/fixtures/skyuv-portable-smoke.conf.in"
  "${PROJECT_BINARY_DIR}/3rd/skyuv-portable-sproto.conf"
  @ONLY
)
set(SKYUV_PORTABLE_START_SERVICE skyuv_md5)
configure_file(
  "${PROJECT_SOURCE_DIR}/tests/fixtures/skyuv-portable-smoke.conf.in"
  "${PROJECT_BINARY_DIR}/3rd/skyuv-portable-md5.conf"
  @ONLY
)
set(SKYUV_PORTABLE_START_SERVICE skyuv_crypt)
configure_file(
  "${PROJECT_SOURCE_DIR}/tests/fixtures/skyuv-portable-smoke.conf.in"
  "${PROJECT_BINARY_DIR}/3rd/skyuv-portable-crypt.conf"
  @ONLY
)
set(SKYUV_PORTABLE_START_SERVICE skyuv_stm)
configure_file(
  "${PROJECT_SOURCE_DIR}/tests/fixtures/skyuv-portable-smoke.conf.in"
  "${PROJECT_BINARY_DIR}/3rd/skyuv-portable-stm.conf"
  @ONLY
)
set(SKYUV_PORTABLE_START_SERVICE skyuv_sharetable)
configure_file(
  "${PROJECT_SOURCE_DIR}/tests/fixtures/skyuv-portable-smoke.conf.in"
  "${PROJECT_BINARY_DIR}/3rd/skyuv-portable-sharetable.conf"
  @ONLY
)
set(SKYUV_PORTABLE_START_SERVICE skyuv_sharedata)
configure_file(
  "${PROJECT_SOURCE_DIR}/tests/fixtures/skyuv-portable-smoke.conf.in"
  "${PROJECT_BINARY_DIR}/3rd/skyuv-portable-sharedata.conf"
  @ONLY
)
set(SKYUV_PORTABLE_START_SERVICE skyuv_datasheet)
configure_file(
  "${PROJECT_SOURCE_DIR}/tests/fixtures/skyuv-portable-smoke.conf.in"
  "${PROJECT_BINARY_DIR}/3rd/skyuv-portable-datasheet.conf"
  @ONLY
)
set(SKYUV_PORTABLE_START_SERVICE skyuv_netpack)
configure_file(
  "${PROJECT_SOURCE_DIR}/tests/fixtures/skyuv-portable-smoke.conf.in"
  "${PROJECT_BINARY_DIR}/3rd/skyuv-portable-netpack.conf"
  @ONLY
)
set(SKYUV_PORTABLE_START_SERVICE skyuv_gate)
configure_file(
  "${PROJECT_SOURCE_DIR}/tests/fixtures/skyuv-portable-smoke.conf.in"
  "${PROJECT_BINARY_DIR}/3rd/skyuv-portable-gate.conf"
  @ONLY
)
set(SKYUV_PORTABLE_START_SERVICE skyuv_cgate)
configure_file(
  "${PROJECT_SOURCE_DIR}/tests/fixtures/skyuv-portable-smoke.conf.in"
  "${PROJECT_BINARY_DIR}/3rd/skyuv-portable-cgate.conf"
  @ONLY
)
set(SKYUV_PORTABLE_START_SERVICE skyuv_memory)
configure_file(
  "${PROJECT_SOURCE_DIR}/tests/fixtures/skyuv-portable-smoke.conf.in"
  "${PROJECT_BINARY_DIR}/3rd/skyuv-portable-memory.conf"
  @ONLY
)
set(SKYUV_PORTABLE_START_SERVICE skyuv_multicast)
configure_file(
  "${PROJECT_SOURCE_DIR}/tests/fixtures/skyuv-portable-smoke.conf.in"
  "${PROJECT_BINARY_DIR}/3rd/skyuv-portable-multicast.conf"
  @ONLY
)
set(SKYUV_PORTABLE_START_SERVICE skyuv_lpeg)
configure_file(
  "${PROJECT_SOURCE_DIR}/tests/fixtures/skyuv-portable-smoke.conf.in"
  "${PROJECT_BINARY_DIR}/3rd/skyuv-portable-lpeg.conf"
  @ONLY
)
set(SKYUV_PORTABLE_START_SERVICE skyuv_control)
configure_file(
  "${PROJECT_SOURCE_DIR}/tests/fixtures/skyuv-portable-smoke.conf.in"
  "${PROJECT_BINARY_DIR}/3rd/skyuv-portable-control.conf"
  @ONLY
)
set(SKYUV_PORTABLE_START_SERVICE skyuv_process_shutdown)
configure_file(
  "${PROJECT_SOURCE_DIR}/tests/fixtures/skyuv-portable-smoke.conf.in"
  "${PROJECT_BINARY_DIR}/3rd/skyuv-portable-process-shutdown.conf"
  @ONLY
)
set(SKYUV_PORTABLE_START_SERVICE skyuv_write_shutdown)
configure_file(
  "${PROJECT_SOURCE_DIR}/tests/fixtures/skyuv-portable-smoke.conf.in"
  "${PROJECT_BINARY_DIR}/3rd/skyuv-portable-write-shutdown.conf"
  @ONLY
)
file(TO_CMAKE_PATH "${PROJECT_BINARY_DIR}/skyuv-signal.log" SKYUV_SIGNAL_LOG)
configure_file(
  "${PROJECT_SOURCE_DIR}/tests/fixtures/skyuv-portable-signal.conf.in"
  "${PROJECT_BINARY_DIR}/3rd/skyuv-portable-signal.conf"
  @ONLY
)
file(TO_CMAKE_PATH "${PROJECT_BINARY_DIR}/logger output/skyuv-runtime.log" SKYUV_LOGGER_FILE)
configure_file(
  "${PROJECT_SOURCE_DIR}/tests/fixtures/skyuv-portable-logger-file.conf.in"
  "${PROJECT_BINARY_DIR}/3rd/skyuv-portable-logger-file.conf"
  @ONLY
)
file(TO_CMAKE_PATH
     "${PROJECT_BINARY_DIR}/missing logger parent/child/skyuv-runtime.log"
     SKYUV_LOGGER_ERROR_FILE)
configure_file(
  "${PROJECT_SOURCE_DIR}/tests/fixtures/skyuv-portable-logger-error.conf.in"
  "${PROJECT_BINARY_DIR}/3rd/skyuv-portable-logger-error.conf"
  @ONLY
)
set(SKYUV_PORTABLE_START_SERVICE skyuv_smoke)
configure_file(
  "${PROJECT_SOURCE_DIR}/tests/fixtures/skyuv-portable-daemon.conf.in"
  "${PROJECT_BINARY_DIR}/3rd/skyuv-portable-daemon.conf"
  @ONLY
)
set(SKYUV_PORTABLE_START_SERVICE skyuv_cluster_core)
configure_file(
  "${PROJECT_SOURCE_DIR}/tests/fixtures/skyuv-portable-smoke.conf.in"
  "${PROJECT_BINARY_DIR}/3rd/skyuv-portable-cluster-core.conf"
  @ONLY
)
set(SKYUV_PORTABLE_START_SERVICE skyuv_debugchannel)
configure_file(
  "${PROJECT_SOURCE_DIR}/tests/fixtures/skyuv-portable-smoke.conf.in"
  "${PROJECT_BINARY_DIR}/3rd/skyuv-portable-debugchannel.conf"
  @ONLY
)
set(SKYUV_PORTABLE_START_SERVICE skyuv_mongo_driver)
configure_file(
  "${PROJECT_SOURCE_DIR}/tests/fixtures/skyuv-portable-smoke.conf.in"
  "${PROJECT_BINARY_DIR}/3rd/skyuv-portable-mongo-driver.conf"
  @ONLY
)
set(SKYUV_PORTABLE_START_SERVICE skyuv_mongo_integration)
configure_file(
  "${PROJECT_SOURCE_DIR}/tests/fixtures/skyuv-portable-smoke.conf.in"
  "${PROJECT_BINARY_DIR}/3rd/skyuv-portable-mongo-integration.conf"
  @ONLY
)
set(SKYUV_PORTABLE_START_SERVICE skyuv_cluster_provider)
configure_file(
  "${PROJECT_SOURCE_DIR}/tests/fixtures/skyuv-portable-smoke.conf.in"
  "${PROJECT_BINARY_DIR}/3rd/skyuv-portable-cluster-provider.conf"
  @ONLY
)
set(SKYUV_PORTABLE_START_SERVICE skyuv_cluster_consumer)
configure_file(
  "${PROJECT_SOURCE_DIR}/tests/fixtures/skyuv-portable-smoke.conf.in"
  "${PROJECT_BINARY_DIR}/3rd/skyuv-portable-cluster-consumer.conf"
  @ONLY
)
set(SKYUV_HARBOR_ID 1)
set(SKYUV_HARBOR_PORT 25287)
set(SKYUV_STANDALONE_LINE "standalone = \"127.0.0.1:25286\"")
set(SKYUV_PORTABLE_START_SERVICE skyuv_harbor_provider)
configure_file(
  "${PROJECT_SOURCE_DIR}/tests/fixtures/skyuv-portable-harbor.conf.in"
  "${PROJECT_BINARY_DIR}/3rd/skyuv-portable-harbor-node1.conf"
  @ONLY
)
set(SKYUV_HARBOR_ID 2)
set(SKYUV_HARBOR_PORT 25288)
set(SKYUV_STANDALONE_LINE "")
set(SKYUV_PORTABLE_START_SERVICE skyuv_harbor_consumer)
configure_file(
  "${PROJECT_SOURCE_DIR}/tests/fixtures/skyuv-portable-harbor.conf.in"
  "${PROJECT_BINARY_DIR}/3rd/skyuv-portable-harbor-node2.conf"
  @ONLY
)
set(SKYUV_HARBOR_ID 3)
set(SKYUV_HARBOR_PORT 25289)
configure_file(
  "${PROJECT_SOURCE_DIR}/tests/fixtures/skyuv-portable-harbor.conf.in"
  "${PROJECT_BINARY_DIR}/3rd/skyuv-portable-harbor-node3.conf"
  @ONLY
)
if(WIN32 AND BUILD_TESTING)
  find_package(Python3 REQUIRED COMPONENTS Interpreter)

  add_test(
    NAME skynet.portable.smoke
    COMMAND
      "${Python3_EXECUTABLE}" "${PROJECT_SOURCE_DIR}/tests/scripts/portable_smoke.py"
      "$<TARGET_FILE:skyuv_skynet_portable>" "3rd/skyuv-portable-smoke.conf"
  )
  set_tests_properties(
    skynet.portable.smoke
    PROPERTIES
      TIMEOUT 10
      WORKING_DIRECTORY "${PROJECT_BINARY_DIR}"
  )
  add_test(
    NAME skynet.portable.echo
    COMMAND
      pwsh -NoProfile -File "${PROJECT_SOURCE_DIR}/tests/scripts/portable-echo.ps1"
      -Executable "$<TARGET_FILE:skyuv_skynet_portable>"
      -Config "3rd/skyuv-portable-echo.conf"
  )
  add_test(
    NAME skynet.portable.client_module
    COMMAND
      "${Python3_EXECUTABLE}" "${PROJECT_SOURCE_DIR}/tests/scripts/portable_smoke.py"
      "$<TARGET_FILE:skyuv_skynet_portable>" "3rd/skyuv-portable-client-module.conf"
  )
  set_tests_properties(
    skynet.portable.client_module
    PROPERTIES
      TIMEOUT 10
      WORKING_DIRECTORY "${PROJECT_BINARY_DIR}"
  )
  add_test(
    NAME skynet.portable.client_socket
    COMMAND
      "${Python3_EXECUTABLE}" "${PROJECT_SOURCE_DIR}/tests/scripts/client_socket_harness.py"
      "$<TARGET_FILE:skyuv_skynet_portable>" "3rd/skyuv-portable-client-socket.conf"
      --allow-abort-after-success
  )
  set_tests_properties(
    skynet.portable.client_socket
    PROPERTIES
      RUN_SERIAL TRUE
      TIMEOUT 20
      WORKING_DIRECTORY "${PROJECT_BINARY_DIR}"
  )
  add_test(
    NAME skynet.portable.client_stdin
    COMMAND
      "${Python3_EXECUTABLE}" "${PROJECT_SOURCE_DIR}/tests/scripts/client_stdin_harness.py"
      "$<TARGET_FILE:skyuv_skynet_portable>" "3rd/skyuv-portable-client-stdin.conf"
      --allow-abort-after-success
  )
  set_tests_properties(
    skynet.portable.client_stdin
    PROPERTIES
      TIMEOUT 20
      WORKING_DIRECTORY "${PROJECT_BINARY_DIR}"
  )
  add_test(
    NAME skynet.portable.bson
    COMMAND
      "${Python3_EXECUTABLE}" "${PROJECT_SOURCE_DIR}/tests/scripts/portable_smoke.py"
      "$<TARGET_FILE:skyuv_skynet_portable>" "3rd/skyuv-portable-bson.conf"
  )
  set_tests_properties(
    skynet.portable.bson
    PROPERTIES
      TIMEOUT 10
      WORKING_DIRECTORY "${PROJECT_BINARY_DIR}"
  )
  add_test(
    NAME skynet.portable.sproto
    COMMAND
      "${Python3_EXECUTABLE}" "${PROJECT_SOURCE_DIR}/tests/scripts/portable_smoke.py"
      "$<TARGET_FILE:skyuv_skynet_portable>" "3rd/skyuv-portable-sproto.conf"
  )
  set_tests_properties(
    skynet.portable.sproto
    PROPERTIES
      TIMEOUT 10
      WORKING_DIRECTORY "${PROJECT_BINARY_DIR}"
  )
  add_test(
    NAME skynet.portable.md5
    COMMAND
      "${Python3_EXECUTABLE}" "${PROJECT_SOURCE_DIR}/tests/scripts/portable_smoke.py"
      "$<TARGET_FILE:skyuv_skynet_portable>" "3rd/skyuv-portable-md5.conf"
  )
  set_tests_properties(
    skynet.portable.md5
    PROPERTIES
      TIMEOUT 10
      WORKING_DIRECTORY "${PROJECT_BINARY_DIR}"
  )
  add_test(
    NAME skynet.portable.crypt
    COMMAND
      "${Python3_EXECUTABLE}" "${PROJECT_SOURCE_DIR}/tests/scripts/portable_smoke.py"
      "$<TARGET_FILE:skyuv_skynet_portable>" "3rd/skyuv-portable-crypt.conf"
  )
  set_tests_properties(
    skynet.portable.crypt
    PROPERTIES
      TIMEOUT 10
      WORKING_DIRECTORY "${PROJECT_BINARY_DIR}"
  )
  add_test(
    NAME skynet.portable.stm
    COMMAND
      "${Python3_EXECUTABLE}" "${PROJECT_SOURCE_DIR}/tests/scripts/portable_smoke.py"
      "$<TARGET_FILE:skyuv_skynet_portable>" "3rd/skyuv-portable-stm.conf"
  )
  set_tests_properties(
    skynet.portable.stm
    PROPERTIES
      TIMEOUT 10
      WORKING_DIRECTORY "${PROJECT_BINARY_DIR}"
  )
  add_test(
    NAME skynet.portable.sharetable
    COMMAND
      "${Python3_EXECUTABLE}" "${PROJECT_SOURCE_DIR}/tests/scripts/portable_smoke.py"
      "$<TARGET_FILE:skyuv_skynet_portable>" "3rd/skyuv-portable-sharetable.conf"
  )
  set_tests_properties(
    skynet.portable.sharetable
    PROPERTIES
      TIMEOUT 10
      WORKING_DIRECTORY "${PROJECT_BINARY_DIR}"
  )
  add_test(
    NAME skynet.portable.sharedata
    COMMAND
      "${Python3_EXECUTABLE}" "${PROJECT_SOURCE_DIR}/tests/scripts/portable_smoke.py"
      "$<TARGET_FILE:skyuv_skynet_portable>" "3rd/skyuv-portable-sharedata.conf"
  )
  set_tests_properties(
    skynet.portable.sharedata
    PROPERTIES
      TIMEOUT 10
      WORKING_DIRECTORY "${PROJECT_BINARY_DIR}"
  )
  add_test(
    NAME skynet.portable.datasheet
    COMMAND
      "${Python3_EXECUTABLE}" "${PROJECT_SOURCE_DIR}/tests/scripts/portable_smoke.py"
      "$<TARGET_FILE:skyuv_skynet_portable>" "3rd/skyuv-portable-datasheet.conf"
  )
  set_tests_properties(
    skynet.portable.datasheet
    PROPERTIES
      TIMEOUT 10
      WORKING_DIRECTORY "${PROJECT_BINARY_DIR}"
  )
  add_test(
    NAME skynet.portable.netpack
    COMMAND
      "${Python3_EXECUTABLE}" "${PROJECT_SOURCE_DIR}/tests/scripts/portable_smoke.py"
      "$<TARGET_FILE:skyuv_skynet_portable>" "3rd/skyuv-portable-netpack.conf"
  )
  set_tests_properties(
    skynet.portable.netpack
    PROPERTIES
      TIMEOUT 10
      WORKING_DIRECTORY "${PROJECT_BINARY_DIR}"
  )
  add_test(
    NAME skynet.portable.gate
    COMMAND
      "${Python3_EXECUTABLE}" "${PROJECT_SOURCE_DIR}/tests/scripts/portable_gate.py"
      "$<TARGET_FILE:skyuv_skynet_portable>" "3rd/skyuv-portable-gate.conf"
      --kind gate
  )
  set_tests_properties(
    skynet.portable.gate
    PROPERTIES
      TIMEOUT 15
      WORKING_DIRECTORY "${PROJECT_BINARY_DIR}"
  )
  add_test(
    NAME skynet.portable.cgate
    COMMAND
      "${Python3_EXECUTABLE}" "${PROJECT_SOURCE_DIR}/tests/scripts/portable_gate.py"
      "$<TARGET_FILE:skyuv_skynet_portable>" "3rd/skyuv-portable-cgate.conf"
      --kind cgate
  )
  set_tests_properties(
    skynet.portable.cgate
    PROPERTIES
      TIMEOUT 15
      WORKING_DIRECTORY "${PROJECT_BINARY_DIR}"
  )
  add_test(
    NAME skynet.portable.memory
    COMMAND
      "${Python3_EXECUTABLE}" "${PROJECT_SOURCE_DIR}/tests/scripts/portable_smoke.py"
      "$<TARGET_FILE:skyuv_skynet_portable>" "3rd/skyuv-portable-memory.conf"
  )
  set_tests_properties(
    skynet.portable.memory
    PROPERTIES
      TIMEOUT 10
      WORKING_DIRECTORY "${PROJECT_BINARY_DIR}"
  )
  add_test(
    NAME skynet.portable.multicast
    COMMAND
      "${Python3_EXECUTABLE}" "${PROJECT_SOURCE_DIR}/tests/scripts/portable_smoke.py"
      "$<TARGET_FILE:skyuv_skynet_portable>" "3rd/skyuv-portable-multicast.conf"
  )
  set_tests_properties(
    skynet.portable.multicast
    PROPERTIES
      TIMEOUT 10
      WORKING_DIRECTORY "${PROJECT_BINARY_DIR}"
  )
  add_test(
    NAME skynet.portable.control
    COMMAND
      "${Python3_EXECUTABLE}" "${PROJECT_SOURCE_DIR}/tests/scripts/portable_smoke.py"
      "$<TARGET_FILE:skyuv_skynet_portable>" "3rd/skyuv-portable-control.conf"
  )
  add_test(
    NAME skynet.portable.console_break
    COMMAND
      "${Python3_EXECUTABLE}" "${PROJECT_SOURCE_DIR}/tests/scripts/process_shutdown.py"
      "$<TARGET_FILE:skyuv_skynet_portable>"
      "3rd/skyuv-portable-process-shutdown.conf"
      --windows-console-break
  )
  set_tests_properties(
    skynet.portable.console_break
    PROPERTIES
      TIMEOUT 15
      WORKING_DIRECTORY "${PROJECT_BINARY_DIR}"
  )
  add_test(
    NAME skynet.portable.write_shutdown
    COMMAND
      "${Python3_EXECUTABLE}" "${PROJECT_SOURCE_DIR}/tests/scripts/write_shutdown.py"
      "$<TARGET_FILE:skyuv_skynet_portable>" "3rd/skyuv-portable-write-shutdown.conf"
      --windows-console-break
  )
  set_tests_properties(
    skynet.portable.write_shutdown
    PROPERTIES RUN_SERIAL TRUE TIMEOUT 20 WORKING_DIRECTORY "${PROJECT_BINARY_DIR}"
  )
  set_tests_properties(
    skynet.portable.control
    PROPERTIES
      TIMEOUT 10
      WORKING_DIRECTORY "${PROJECT_BINARY_DIR}"
  )
  add_test(
    NAME skynet.portable.paths
    COMMAND
      pwsh -NoProfile -File "${PROJECT_SOURCE_DIR}/tests/scripts/portable-paths.ps1"
      -Executable "$<TARGET_FILE:skyuv_skynet_portable>"
      -PortableDirectory "${PROJECT_BINARY_DIR}/3rd/portable"
      -SkynetSource "${PROJECT_SOURCE_DIR}/3rd/skynet"
      -FixtureSource "${PROJECT_SOURCE_DIR}/tests/fixtures"
  )
  set_tests_properties(
    skynet.portable.paths
    PROPERTIES
      TIMEOUT 15
      WORKING_DIRECTORY "${PROJECT_BINARY_DIR}"
  )
  add_test(
    NAME skynet.portable.daemon_unsupported
    COMMAND
      pwsh -NoProfile -File "${PROJECT_SOURCE_DIR}/tests/scripts/portable-daemon.ps1"
      -Executable "$<TARGET_FILE:skyuv_skynet_portable>"
      -Config "3rd/skyuv-portable-daemon.conf"
  )
  set_tests_properties(
    skynet.portable.daemon_unsupported
    PROPERTIES
      TIMEOUT 10
      WORKING_DIRECTORY "${PROJECT_BINARY_DIR}"
  )
  add_test(
    NAME skynet.portable.cluster_core
    COMMAND
      "${Python3_EXECUTABLE}" "${PROJECT_SOURCE_DIR}/tests/scripts/portable_smoke.py"
      "$<TARGET_FILE:skyuv_skynet_portable>" "3rd/skyuv-portable-cluster-core.conf"
  )
  set_tests_properties(
    skynet.portable.cluster_core
    PROPERTIES
      TIMEOUT 10
      WORKING_DIRECTORY "${PROJECT_BINARY_DIR}"
  )
  add_test(
    NAME skynet.portable.debugchannel
    COMMAND
      "${Python3_EXECUTABLE}" "${PROJECT_SOURCE_DIR}/tests/scripts/portable_smoke.py"
      "$<TARGET_FILE:skyuv_skynet_portable>" "3rd/skyuv-portable-debugchannel.conf"
  )
  set_tests_properties(
    skynet.portable.debugchannel
    PROPERTIES
      TIMEOUT 10
      WORKING_DIRECTORY "${PROJECT_BINARY_DIR}"
  )
  add_test(
    NAME skynet.portable.mongo_driver
    COMMAND
      "${Python3_EXECUTABLE}" "${PROJECT_SOURCE_DIR}/tests/scripts/portable_smoke.py"
      "$<TARGET_FILE:skyuv_skynet_portable>" "3rd/skyuv-portable-mongo-driver.conf"
  )
  set_tests_properties(
    skynet.portable.mongo_driver
    PROPERTIES
      TIMEOUT 10
      WORKING_DIRECTORY "${PROJECT_BINARY_DIR}"
  )
  add_test(
    NAME skynet.portable.harbor
    COMMAND
      "${Python3_EXECUTABLE}" "${PROJECT_SOURCE_DIR}/tests/scripts/portable_harbor.py"
      "$<TARGET_FILE:skyuv_skynet_portable>"
      "3rd/skyuv-portable-harbor-node1.conf"
      "3rd/skyuv-portable-harbor-node2.conf"
      "3rd/skyuv-portable-harbor-node3.conf"
  )
  set_tests_properties(
    skynet.portable.harbor
    PROPERTIES
      RUN_SERIAL TRUE
      TIMEOUT 35
      WORKING_DIRECTORY "${PROJECT_BINARY_DIR}"
  )
  set_tests_properties(
    skynet.portable.echo
    PROPERTIES
      TIMEOUT 15
      WORKING_DIRECTORY "${PROJECT_BINARY_DIR}"
  )
endif()

if(BUILD_TESTING)
  find_package(Python3 REQUIRED COMPONENTS Interpreter)
  add_test(
    NAME skynet.portable.cluster
    COMMAND
      "${Python3_EXECUTABLE}" "${PROJECT_SOURCE_DIR}/tests/scripts/portable_cluster.py"
      "$<TARGET_FILE:skyuv_skynet_portable>"
      "3rd/skyuv-portable-cluster-provider.conf"
      "3rd/skyuv-portable-cluster-consumer.conf"
  )
  set_tests_properties(
    skynet.portable.cluster
    PROPERTIES
      RUN_SERIAL TRUE
      TIMEOUT 25
      WORKING_DIRECTORY "${PROJECT_BINARY_DIR}"
  )
  add_test(
    NAME skynet.portable.logger_file
    COMMAND
      "${Python3_EXECUTABLE}" "${PROJECT_SOURCE_DIR}/tests/scripts/portable_logger_file.py"
      "$<TARGET_FILE:skyuv_skynet_portable>"
      "3rd/skyuv-portable-logger-file.conf"
      "${SKYUV_LOGGER_FILE}"
  )
  set_tests_properties(
    skynet.portable.logger_file
    PROPERTIES
      TIMEOUT 15
      WORKING_DIRECTORY "${PROJECT_BINARY_DIR}"
  )
  add_test(
    NAME skynet.portable.logger_error
    COMMAND
      "${Python3_EXECUTABLE}" "${PROJECT_SOURCE_DIR}/tests/scripts/portable_logger_error.py"
      "$<TARGET_FILE:skyuv_skynet_portable>"
      "3rd/skyuv-portable-logger-error.conf"
      "${SKYUV_LOGGER_ERROR_FILE}"
  )
  set_tests_properties(
    skynet.portable.logger_error
    PROPERTIES
      TIMEOUT 15
      WORKING_DIRECTORY "${PROJECT_BINARY_DIR}"
  )
  add_test(
    NAME skynet.portable.lpeg
    COMMAND
      "${Python3_EXECUTABLE}" "${PROJECT_SOURCE_DIR}/tests/scripts/portable_smoke.py"
      "$<TARGET_FILE:skyuv_skynet_portable>" "3rd/skyuv-portable-lpeg.conf"
  )
  set_tests_properties(
    skynet.portable.lpeg
    PROPERTIES
      TIMEOUT 15
      WORKING_DIRECTORY "${PROJECT_BINARY_DIR}"
  )
endif()

if(NOT WIN32 AND BUILD_TESTING)
  find_package(Python3 REQUIRED COMPONENTS Interpreter)
  add_test(
    NAME skynet.portable.client_stdin
    COMMAND
      "${Python3_EXECUTABLE}" "${PROJECT_SOURCE_DIR}/tests/scripts/client_stdin_harness.py"
      "$<TARGET_FILE:skyuv_skynet_portable>" "3rd/skyuv-portable-client-stdin.conf"
      --allow-abort-after-success
  )
  set_tests_properties(
    skynet.portable.client_stdin
    PROPERTIES
      TIMEOUT 20
      WORKING_DIRECTORY "${PROJECT_BINARY_DIR}"
  )
  add_test(
    NAME skynet.portable.client_socket
    COMMAND
      "${Python3_EXECUTABLE}" "${PROJECT_SOURCE_DIR}/tests/scripts/client_socket_harness.py"
      "$<TARGET_FILE:skyuv_skynet_portable>" "3rd/skyuv-portable-client-socket.conf"
      --allow-abort-after-success
  )
  set_tests_properties(
    skynet.portable.client_socket
    PROPERTIES
      RUN_SERIAL TRUE
      TIMEOUT 20
      WORKING_DIRECTORY "${PROJECT_BINARY_DIR}"
  )
  foreach(module_name IN ITEMS smoke client_module cluster_core debugchannel mongo_driver)
    string(REPLACE "_" "-" config_name "${module_name}")
    add_test(
      NAME "skynet.portable.${module_name}"
      COMMAND
        "${Python3_EXECUTABLE}" "${PROJECT_SOURCE_DIR}/tests/scripts/portable_smoke.py"
        "$<TARGET_FILE:skyuv_skynet_portable>"
        "3rd/skyuv-portable-${config_name}.conf"
    )
    set_tests_properties(
      "skynet.portable.${module_name}"
      PROPERTIES
        TIMEOUT 15
        WORKING_DIRECTORY "${PROJECT_BINARY_DIR}"
    )
  endforeach()
  foreach(gate_kind IN ITEMS gate cgate)
    add_test(
      NAME "skynet.portable.${gate_kind}"
      COMMAND
        "${Python3_EXECUTABLE}" "${PROJECT_SOURCE_DIR}/tests/scripts/portable_gate.py"
        "$<TARGET_FILE:skyuv_skynet_portable>" "3rd/skyuv-portable-${gate_kind}.conf"
        --kind "${gate_kind}"
    )
    set_tests_properties(
      "skynet.portable.${gate_kind}"
      PROPERTIES
        TIMEOUT 20
        WORKING_DIRECTORY "${PROJECT_BINARY_DIR}"
    )
  endforeach()
  add_test(
    NAME skynet.portable.bson
    COMMAND
      "${Python3_EXECUTABLE}" "${PROJECT_SOURCE_DIR}/tests/scripts/portable_smoke.py"
      "$<TARGET_FILE:skyuv_skynet_portable>" "3rd/skyuv-portable-bson.conf"
  )
  set_tests_properties(
    skynet.portable.bson
    PROPERTIES
      TIMEOUT 15
      WORKING_DIRECTORY "${PROJECT_BINARY_DIR}"
  )
  foreach(module_name IN ITEMS sproto md5 crypt)
    add_test(
      NAME "skynet.portable.${module_name}"
      COMMAND
        "${Python3_EXECUTABLE}" "${PROJECT_SOURCE_DIR}/tests/scripts/portable_smoke.py"
        "$<TARGET_FILE:skyuv_skynet_portable>"
        "3rd/skyuv-portable-${module_name}.conf"
    )
    set_tests_properties(
      "skynet.portable.${module_name}"
      PROPERTIES
        TIMEOUT 15
        WORKING_DIRECTORY "${PROJECT_BINARY_DIR}"
    )
  endforeach()
  foreach(module_name IN ITEMS stm sharetable sharedata datasheet multicast netpack memory control)
    add_test(
      NAME "skynet.portable.${module_name}"
      COMMAND
        "${Python3_EXECUTABLE}" "${PROJECT_SOURCE_DIR}/tests/scripts/portable_smoke.py"
        "$<TARGET_FILE:skyuv_skynet_portable>"
        "3rd/skyuv-portable-${module_name}.conf"
    )
    set_tests_properties(
      "skynet.portable.${module_name}"
      PROPERTIES
        TIMEOUT 15
        WORKING_DIRECTORY "${PROJECT_BINARY_DIR}"
    )
  endforeach()
  add_test(
    NAME skynet.portable.harbor
    COMMAND
      "${Python3_EXECUTABLE}" "${PROJECT_SOURCE_DIR}/tests/scripts/portable_harbor.py"
      "$<TARGET_FILE:skyuv_skynet_portable>"
      "3rd/skyuv-portable-harbor-node1.conf"
      "3rd/skyuv-portable-harbor-node2.conf"
      "3rd/skyuv-portable-harbor-node3.conf"
  )
  set_tests_properties(
    skynet.portable.harbor
    PROPERTIES
      RUN_SERIAL TRUE
      TIMEOUT 45
      WORKING_DIRECTORY "${PROJECT_BINARY_DIR}"
  )
endif()
