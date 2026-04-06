if(FABRICA_GLFW_BACKEND_AVAILABLE)
  add_executable(fabrica_runtime_sample
    runtime_sample/main.cc
  )

  target_link_libraries(fabrica_runtime_sample
    PRIVATE
      fabrica_core_common
      fabrica_core_jobs
      fabrica_core_logging
      fabrica_core_runtime
      fabrica_core_window
  )
else()
  message(STATUS "GLFW not found: skipping fabrica_runtime_sample target")
endif()
