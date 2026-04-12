if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR AND NOT FABRICA_SAMPLES_BOOTSTRAP_ACTIVE)
  cmake_minimum_required(VERSION 3.20)
  project(FabricaSamplesBootstrap LANGUAGES CXX)

  set(FABRICA_SAMPLES_BOOTSTRAP_ACTIVE ON CACHE INTERNAL "Enable engine root bootstrap when opening engine/samples directly")

  if(NOT DEFINED FABRICA_BUILD_SAMPLES)
    set(FABRICA_BUILD_SAMPLES ON CACHE BOOL "Build FabricaEngine sample applications" FORCE)
  endif()

  if(NOT DEFINED FABRICA_ENABLE_GLFW_WINDOW_BACKEND)
    set(FABRICA_ENABLE_GLFW_WINDOW_BACKEND ON CACHE BOOL "Enable GLFW-based window backend" FORCE)
  endif()

  if(NOT DEFINED FABRICA_FETCH_GLFW)
    set(FABRICA_FETCH_GLFW ON CACHE BOOL "Fetch GLFW with CMake when no system package is available" FORCE)
  endif()

  if(NOT DEFINED FABRICA_BUILD_TESTS)
    set(FABRICA_BUILD_TESTS OFF CACHE BOOL "Build FabricaEngine unit tests" FORCE)
  endif()

  message(STATUS "Bootstrapping from engine/samples and delegating to engine/CMakeLists.txt")
  add_subdirectory(.. ${CMAKE_BINARY_DIR}/engine_root)
  return()
endif()

function(fabrica_add_sample target source)
  add_executable(${target} ${source})
  target_link_libraries(${target} PRIVATE ${ARGN})
endfunction()

fabrica_add_sample(fabrica_common_sample
  common_sample/main.cc
  fabrica_core_common
)

fabrica_add_sample(fabrica_memory_sample
  memory_sample/main.cc
  fabrica_core_memory
)

fabrica_add_sample(fabrica_logging_sample
  logging_sample/main.cc
  fabrica_core_logging
)

fabrica_add_sample(fabrica_jobs_sample
  jobs_sample/main.cc
  fabrica_core_jobs
)

fabrica_add_sample(fabrica_async_sample
  async_sample/main.cc
  fabrica_core_async
)

fabrica_add_sample(fabrica_assets_sample
  assets_sample/main.cc
  fabrica_core_assets
)

fabrica_add_sample(fabrica_ecs_sample
  ecs_sample/main.cc
  fabrica_core_ecs
)

fabrica_add_sample(fabrica_input_sample
  input_sample/main.cc
  fabrica_core_input
)

fabrica_add_sample(fabrica_window_sample
  window_sample/main.cc
  fabrica_core_window
)

fabrica_add_sample(fabrica_runtime_headless_sample
  runtime_headless_sample/main.cc
  fabrica_core_runtime
)

fabrica_add_sample(fabrica_view_application_sample
  view_application_sample/main.cc
  fabrica_core_app
)

fabrica_add_sample(fabrica_pal_sample
  pal_sample/main.cc
  fabrica_pal
)

if(FABRICA_GLFW_BACKEND_AVAILABLE)
  find_program(FABRICA_GLSLC_EXECUTABLE
    NAMES glslc
    HINTS
      ENV VULKAN_SDK
    PATH_SUFFIXES Bin bin
  )

  if(NOT FABRICA_GLSLC_EXECUTABLE)
    message(FATAL_ERROR "glslc was not found. Install the Vulkan SDK or add glslc to PATH.")
  endif()

  set(FABRICA_RHI_TRIANGLE_SHADER_OUTPUT_DIR "${CMAKE_CURRENT_BINARY_DIR}/compiled_shaders/rhi_triangle_sample")
  set(FABRICA_RHI_TRIANGLE_SHADER_SOURCES
    rhi_triangle_sample/shaders/triangle.vert
    rhi_triangle_sample/shaders/triangle.frag
  )

  set(FABRICA_RHI_TRIANGLE_SHADER_OUTPUTS "")
  foreach(shader_source IN LISTS FABRICA_RHI_TRIANGLE_SHADER_SOURCES)
    get_filename_component(shader_name "${shader_source}" NAME)
    set(shader_output "${FABRICA_RHI_TRIANGLE_SHADER_OUTPUT_DIR}/${shader_name}.spv")
    add_custom_command(
      OUTPUT "${shader_output}"
      COMMAND ${CMAKE_COMMAND} -E make_directory "${FABRICA_RHI_TRIANGLE_SHADER_OUTPUT_DIR}"
      COMMAND ${FABRICA_GLSLC_EXECUTABLE}
              "${CMAKE_CURRENT_SOURCE_DIR}/${shader_source}"
              -o "${shader_output}"
      DEPENDS
        "${CMAKE_CURRENT_SOURCE_DIR}/${shader_source}"
      VERBATIM
    )
    list(APPEND FABRICA_RHI_TRIANGLE_SHADER_OUTPUTS "${shader_output}")
  endforeach()

  add_custom_target(fabrica_rhi_triangle_sample_shaders
    DEPENDS ${FABRICA_RHI_TRIANGLE_SHADER_OUTPUTS}
  )

  fabrica_add_sample(fabrica_runtime_sample
    runtime_glfw_sample/main.cc
    fabrica_core_app
  )

  fabrica_add_sample(fabrica_rhi_triangle_sample
    rhi_triangle_sample/main.cc
    fabrica_core_window
    fabrica_rhi
    fabrica_rhi_vulkan
  )

  fabrica_add_sample(fabrica_renderer_sample
    renderer_sample/main.cc
    fabrica_core_window
    fabrica_renderer
    fabrica_rhi
    fabrica_rhi_vulkan
  )

  add_dependencies(fabrica_rhi_triangle_sample fabrica_rhi_triangle_sample_shaders)
  target_compile_definitions(fabrica_rhi_triangle_sample
    PRIVATE
      FABRICA_RHI_TRIANGLE_SHADER_DIR="${FABRICA_RHI_TRIANGLE_SHADER_OUTPUT_DIR}"
  )

  message(STATUS "fabrica_runtime_sample enabled (GLFW provider: ${FABRICA_GLFW_BACKEND_SOURCE})")
  message(STATUS "fabrica_rhi_triangle_sample enabled (GLFW provider: ${FABRICA_GLFW_BACKEND_SOURCE})")
  message(STATUS "fabrica_renderer_sample enabled (GLFW provider: ${FABRICA_GLFW_BACKEND_SOURCE})")
else()
  message(STATUS "GLFW backend unavailable: skipping fabrica_runtime_sample target")
  message(STATUS "GLFW backend unavailable: skipping fabrica_rhi_triangle_sample target")
  message(STATUS "GLFW backend unavailable: skipping fabrica_renderer_sample target")
  message(STATUS "To enable sample build: install glfw3 package or configure with -DFABRICA_FETCH_GLFW=ON")
endif()