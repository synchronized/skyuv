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
set(SKYUV_PORTABLE_START_SERVICE skyuv_cluster_core)
configure_file(
  "${PROJECT_SOURCE_DIR}/tests/fixtures/skyuv-portable-smoke.conf.in"
  "${PROJECT_BINARY_DIR}/3rd/skyuv-portable-cluster-core.conf"
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
  add_test(
    NAME skynet.portable.smoke
    COMMAND
      pwsh -NoProfile -File "${PROJECT_SOURCE_DIR}/tests/scripts/portable-smoke.ps1"
      -Executable "$<TARGET_FILE:skyuv_skynet_portable>"
      -Config "3rd/skyuv-portable-smoke.conf"
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
      pwsh -NoProfile -File "${PROJECT_SOURCE_DIR}/tests/scripts/portable-smoke.ps1"
      -Executable "$<TARGET_FILE:skyuv_skynet_portable>"
      -Config "3rd/skyuv-portable-client-module.conf"
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
      pwsh -NoProfile -File "${PROJECT_SOURCE_DIR}/tests/scripts/portable-client-socket.ps1"
      -Executable "$<TARGET_FILE:skyuv_skynet_portable>"
      -Config "3rd/skyuv-portable-client-socket.conf"
  )
  set_tests_properties(
    skynet.portable.client_socket
    PROPERTIES
      TIMEOUT 15
      WORKING_DIRECTORY "${PROJECT_BINARY_DIR}"
  )
  add_test(
    NAME skynet.portable.client_stdin
    COMMAND
      pwsh -NoProfile -File "${PROJECT_SOURCE_DIR}/tests/scripts/portable-client-stdin.ps1"
      -Executable "$<TARGET_FILE:skyuv_skynet_portable>"
      -Config "3rd/skyuv-portable-client-stdin.conf"
  )
  set_tests_properties(
    skynet.portable.client_stdin
    PROPERTIES
      TIMEOUT 15
      WORKING_DIRECTORY "${PROJECT_BINARY_DIR}"
  )
  add_test(
    NAME skynet.portable.bson
    COMMAND
      pwsh -NoProfile -File "${PROJECT_SOURCE_DIR}/tests/scripts/portable-smoke.ps1"
      -Executable "$<TARGET_FILE:skyuv_skynet_portable>"
      -Config "3rd/skyuv-portable-bson.conf"
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
      pwsh -NoProfile -File "${PROJECT_SOURCE_DIR}/tests/scripts/portable-smoke.ps1"
      -Executable "$<TARGET_FILE:skyuv_skynet_portable>"
      -Config "3rd/skyuv-portable-sproto.conf"
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
      pwsh -NoProfile -File "${PROJECT_SOURCE_DIR}/tests/scripts/portable-smoke.ps1"
      -Executable "$<TARGET_FILE:skyuv_skynet_portable>"
      -Config "3rd/skyuv-portable-md5.conf"
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
      pwsh -NoProfile -File "${PROJECT_SOURCE_DIR}/tests/scripts/portable-smoke.ps1"
      -Executable "$<TARGET_FILE:skyuv_skynet_portable>"
      -Config "3rd/skyuv-portable-crypt.conf"
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
      pwsh -NoProfile -File "${PROJECT_SOURCE_DIR}/tests/scripts/portable-smoke.ps1"
      -Executable "$<TARGET_FILE:skyuv_skynet_portable>"
      -Config "3rd/skyuv-portable-stm.conf"
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
      pwsh -NoProfile -File "${PROJECT_SOURCE_DIR}/tests/scripts/portable-smoke.ps1"
      -Executable "$<TARGET_FILE:skyuv_skynet_portable>"
      -Config "3rd/skyuv-portable-sharetable.conf"
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
      pwsh -NoProfile -File "${PROJECT_SOURCE_DIR}/tests/scripts/portable-smoke.ps1"
      -Executable "$<TARGET_FILE:skyuv_skynet_portable>"
      -Config "3rd/skyuv-portable-sharedata.conf"
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
      pwsh -NoProfile -File "${PROJECT_SOURCE_DIR}/tests/scripts/portable-smoke.ps1"
      -Executable "$<TARGET_FILE:skyuv_skynet_portable>"
      -Config "3rd/skyuv-portable-datasheet.conf"
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
      pwsh -NoProfile -File "${PROJECT_SOURCE_DIR}/tests/scripts/portable-smoke.ps1"
      -Executable "$<TARGET_FILE:skyuv_skynet_portable>"
      -Config "3rd/skyuv-portable-netpack.conf"
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
      pwsh -NoProfile -File "${PROJECT_SOURCE_DIR}/tests/scripts/portable-gate.ps1"
      -Executable "$<TARGET_FILE:skyuv_skynet_portable>"
      -Config "3rd/skyuv-portable-gate.conf"
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
      pwsh -NoProfile -File "${PROJECT_SOURCE_DIR}/tests/scripts/portable-cgate.ps1"
      -Executable "$<TARGET_FILE:skyuv_skynet_portable>"
      -Config "3rd/skyuv-portable-cgate.conf"
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
      pwsh -NoProfile -File "${PROJECT_SOURCE_DIR}/tests/scripts/portable-smoke.ps1"
      -Executable "$<TARGET_FILE:skyuv_skynet_portable>"
      -Config "3rd/skyuv-portable-memory.conf"
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
      pwsh -NoProfile -File "${PROJECT_SOURCE_DIR}/tests/scripts/portable-smoke.ps1"
      -Executable "$<TARGET_FILE:skyuv_skynet_portable>"
      -Config "3rd/skyuv-portable-multicast.conf"
  )
  set_tests_properties(
    skynet.portable.multicast
    PROPERTIES
      TIMEOUT 10
      WORKING_DIRECTORY "${PROJECT_BINARY_DIR}"
  )
  add_test(
    NAME skynet.portable.cluster_core
    COMMAND
      pwsh -NoProfile -File "${PROJECT_SOURCE_DIR}/tests/scripts/portable-smoke.ps1"
      -Executable "$<TARGET_FILE:skyuv_skynet_portable>"
      -Config "3rd/skyuv-portable-cluster-core.conf"
  )
  set_tests_properties(
    skynet.portable.cluster_core
    PROPERTIES
      TIMEOUT 10
      WORKING_DIRECTORY "${PROJECT_BINARY_DIR}"
  )
  add_test(
    NAME skynet.portable.cluster
    COMMAND
      pwsh -NoProfile -File "${PROJECT_SOURCE_DIR}/tests/scripts/portable-cluster.ps1"
      -Executable "$<TARGET_FILE:skyuv_skynet_portable>"
      -ProviderConfig "3rd/skyuv-portable-cluster-provider.conf"
      -ConsumerConfig "3rd/skyuv-portable-cluster-consumer.conf"
  )
  set_tests_properties(
    skynet.portable.cluster
    PROPERTIES
      TIMEOUT 20
      WORKING_DIRECTORY "${PROJECT_BINARY_DIR}"
  )
  add_test(
    NAME skynet.portable.harbor
    COMMAND
      pwsh -NoProfile -File "${PROJECT_SOURCE_DIR}/tests/scripts/portable-harbor.ps1"
      -Executable "$<TARGET_FILE:skyuv_skynet_portable>"
      -Node1Config "3rd/skyuv-portable-harbor-node1.conf"
      -Node2Config "3rd/skyuv-portable-harbor-node2.conf"
      -Node3Config "3rd/skyuv-portable-harbor-node3.conf"
  )
  set_tests_properties(
    skynet.portable.harbor
    PROPERTIES
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
