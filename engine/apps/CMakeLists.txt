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

  message(STATUS "fabrica_runtime_sample enabled (GLFW provider: ${FABRICA_GLFW_BACKEND_SOURCE})")
else()
  message(STATUS "GLFW backend unavailable: skipping fabrica_runtime_sample target")
  message(STATUS "To enable sample build: install glfw3 package or configure with -DFABRICA_FETCH_GLFW=ON")
endif()
