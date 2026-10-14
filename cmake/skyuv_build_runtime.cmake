# Windows 构建加载器仍需将 DLL 与程序放在相邻位置；最终运行树通过统一安装规则组装。
include("${CMAKE_CURRENT_LIST_DIR}/skyuv_windows_runtime.cmake")
if(NOT SKYUV_STAGE_PROJECT)
  return()
endif()

skyuv_collect_runtime_targets("${SKYUV_SOURCE_DIR}" project_targets)
set(project_dependencies)
foreach(project_target IN LISTS project_targets)
  if(project_target STREQUAL "skyuv" OR project_target STREQUAL "skyuv_lua_interpreter" OR
     project_target MATCHES "^skyuv_portable_(service_|lua_module_)")
    list(APPEND project_dependencies "${project_target}")
  endif()
endforeach()
if(TARGET skyuv_stage_windows_runtime)
  list(APPEND project_dependencies skyuv_stage_windows_runtime)
endif()
set(project_script
"set(CMAKE_INSTALL_PREFIX [==[${SKYUV_BINARY_DIR}/project]==])
set(CMAKE_INSTALL_CONFIG_NAME [==[$<CONFIG>]==])
set(CMAKE_INSTALL_COMPONENT Runtime)
set(CMAKE_INSTALL_LOCAL_ONLY FALSE)
include([==[${SKYUV_BINARY_DIR}/cmake_install.cmake]==])
")
if(TARGET skyuv_lua_interpreter)
  # 独立解释器是构建工具，放入开发运行目录，但不改变 Runtime 发行组件的边界。
  string(APPEND project_script
"file(MAKE_DIRECTORY [==[${SKYUV_BINARY_DIR}/project/${CMAKE_INSTALL_BINDIR}]==])
")
  if(NOT WIN32)
    string(APPEND project_script
"file(COPY_FILE [==[$<TARGET_FILE:skyuv_lua_interpreter>]==]
  [==[${SKYUV_BINARY_DIR}/project/${CMAKE_INSTALL_BINDIR}/$<TARGET_FILE_NAME:skyuv_lua_interpreter>]==]
  ONLY_IF_DIFFERENT)
")
  endif()
endif()
file(GENERATE OUTPUT "${SKYUV_BINARY_DIR}/generated/project-runtime-$<CONFIG>.cmake"
  CONTENT "${project_script}")
add_custom_target(skyuv_stage_runtime ALL
  COMMAND "${CMAKE_COMMAND}" -P "${SKYUV_BINARY_DIR}/generated/project-runtime-$<CONFIG>.cmake"
  DEPENDS ${project_dependencies}
  COMMENT "组装三平台 project 运行目录")
