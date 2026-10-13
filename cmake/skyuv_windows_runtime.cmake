# Windows 的加载器从可执行文件目录查找共享依赖；构建树也必须具备完整运行布局。
function(skyuv_collect_runtime_targets directory output)
  get_property(local_targets DIRECTORY "${directory}" PROPERTY BUILDSYSTEM_TARGETS)
  get_property(children DIRECTORY "${directory}" PROPERTY SUBDIRECTORIES)
  foreach(child IN LISTS children)
    skyuv_collect_runtime_targets("${child}" child_targets)
    list(APPEND local_targets ${child_targets})
  endforeach()
  set(${output} "${local_targets}" PARENT_SCOPE)
endfunction()

if(WIN32)
  skyuv_collect_runtime_targets("${SKYUV_SOURCE_DIR}" runtime_targets)
  set(runtime_executables)
  set(runtime_libraries)
  set(runtime_script "file(MAKE_DIRECTORY \"${SKYUV_BINARY_DIR}/bin\")\n")
  set(runtime_verify "")
  foreach(runtime_target IN LISTS runtime_targets)
    get_target_property(runtime_type ${runtime_target} TYPE)
    if(runtime_type STREQUAL "EXECUTABLE" OR runtime_target STREQUAL "uv")
      # 每个 preset 独立构建目录；空生成器表达式避免多配置生成器另加 Debug/Release。
      set_target_properties(${runtime_target} PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${SKYUV_BINARY_DIR}/bin/$<0:>")
      foreach(runtime_config IN ITEMS DEBUG RELEASE RELWITHDEBINFO MINSIZEREL)
        set_target_properties(${runtime_target} PROPERTIES
          RUNTIME_OUTPUT_DIRECTORY_${runtime_config} "${SKYUV_BINARY_DIR}/bin")
      endforeach()
    endif()
    if(runtime_type STREQUAL "EXECUTABLE")
      list(APPEND runtime_executables ${runtime_target})
      string(APPEND runtime_verify
        "if(NOT EXISTS \"$<TARGET_FILE:${runtime_target}>\")\n  message(FATAL_ERROR \"运行程序缺失：${runtime_target}\")\nendif()\n")
    elseif(runtime_type STREQUAL "MODULE_LIBRARY" OR runtime_type STREQUAL "SHARED_LIBRARY")
      list(APPEND runtime_libraries ${runtime_target})
      set(runtime_destination "${SKYUV_BINARY_DIR}/bin")
      if(NOT runtime_target STREQUAL "uv")
        get_target_property(runtime_directory ${runtime_target} RUNTIME_OUTPUT_DIRECTORY)
        set(runtime_destination "${SKYUV_BINARY_DIR}/bin")
        string(FIND "${runtime_directory}" "${SKYUV_BINARY_DIR}/3rd/portable/" runtime_prefix)
        if(runtime_prefix EQUAL 0)
          file(RELATIVE_PATH runtime_relative "${SKYUV_BINARY_DIR}/3rd/portable" "${runtime_directory}")
          string(APPEND runtime_destination "/${runtime_relative}")
        endif()
        string(APPEND runtime_script
          "file(MAKE_DIRECTORY \"${runtime_destination}\")\n"
          "file(COPY_FILE \"$<TARGET_FILE:${runtime_target}>\" \"${runtime_destination}/$<TARGET_FILE_NAME:${runtime_target}>\" ONLY_IF_DIFFERENT)\n")
      endif()
      string(APPEND runtime_verify
        "if(NOT EXISTS \"${runtime_destination}/$<TARGET_FILE_NAME:${runtime_target}>\")\n  message(FATAL_ERROR \"运行 DLL 缺失：${runtime_target}\")\nendif()\n"
        "file(SHA256 \"$<TARGET_FILE:${runtime_target}>\" original_hash)\n"
        "file(SHA256 \"${runtime_destination}/$<TARGET_FILE_NAME:${runtime_target}>\" staged_hash)\n"
        "if(NOT original_hash STREQUAL staged_hash)\n  message(FATAL_ERROR \"运行 DLL 与构建产物不一致：${runtime_target}\")\nendif()\n")
    endif()
  endforeach()
  file(GENERATE OUTPUT "${SKYUV_BINARY_DIR}/generated/stage-runtime-$<CONFIG>.cmake"
    CONTENT "${runtime_script}")
  file(GENERATE OUTPUT "${SKYUV_BINARY_DIR}/generated/verify-runtime-$<CONFIG>.cmake"
    CONTENT "${runtime_verify}")
  add_custom_target(skyuv_stage_windows_runtime ALL
    COMMAND "${CMAKE_COMMAND}" -P "${SKYUV_BINARY_DIR}/generated/stage-runtime-$<CONFIG>.cmake"
    DEPENDS ${runtime_libraries}
    COMMENT "部署 Windows 构建运行目录")
  foreach(runtime_executable IN LISTS runtime_executables)
    # 测试模块可能反向链接测试程序；程序仅依赖共享 I/O 库，避免部署目标形成环。
    if(TARGET uv)
      add_dependencies(${runtime_executable} uv)
    endif()
  endforeach()
endif()
