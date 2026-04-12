set(FABRICA_RENDERER_SHADER_OUTPUT_DIR "${CMAKE_CURRENT_BINARY_DIR}/compiled_shaders")
find_program(FABRICA_GLSLC_EXECUTABLE
  NAMES glslc
  HINTS
    ENV VULKAN_SDK
  PATH_SUFFIXES Bin bin
)

if(NOT FABRICA_GLSLC_EXECUTABLE)
  message(FATAL_ERROR "glslc was not found. Install the Vulkan SDK or add glslc to PATH.")
endif()

set(FABRICA_RENDERER_SHADER_COMMON
  shaders/common/brdf.glsl
  shaders/common/lighting.glsl
  shaders/common/material_inputs.glsl
  shaders/common/math.glsl
  shaders/common/tonemapping.glsl
)

set(FABRICA_RENDERER_SHADER_SOURCES
  shaders/fullscreen.vert
  shaders/pbr.frag
  shaders/pbr.vert
  shaders/tonemapping.frag
)

set(FABRICA_RENDERER_SHADER_OUTPUTS "")
foreach(shader_source IN LISTS FABRICA_RENDERER_SHADER_SOURCES)
  get_filename_component(shader_name "${shader_source}" NAME)
  set(shader_output "${FABRICA_RENDERER_SHADER_OUTPUT_DIR}/${shader_name}.spv")
  add_custom_command(
    OUTPUT "${shader_output}"
    COMMAND ${CMAKE_COMMAND} -E make_directory "${FABRICA_RENDERER_SHADER_OUTPUT_DIR}"
    COMMAND ${FABRICA_GLSLC_EXECUTABLE}
            -I "${CMAKE_CURRENT_SOURCE_DIR}/shaders"
            -I "${CMAKE_CURRENT_SOURCE_DIR}/shaders/common"
            "${CMAKE_CURRENT_SOURCE_DIR}/${shader_source}"
            -o "${shader_output}"
    DEPENDS
      "${CMAKE_CURRENT_SOURCE_DIR}/${shader_source}"
      ${FABRICA_RENDERER_SHADER_COMMON}
    VERBATIM
  )
  list(APPEND FABRICA_RENDERER_SHADER_OUTPUTS "${shader_output}")
endforeach()

add_custom_target(fabrica_renderer_shaders DEPENDS ${FABRICA_RENDERER_SHADER_OUTPUTS})

add_library(fabrica_renderer
  src/Renderer.cpp
  src/internal/FrameResources.cpp
  src/internal/PipelineLibrary.cpp
  src/internal/ShaderLibrary.cpp
)

add_dependencies(fabrica_renderer fabrica_renderer_shaders)

target_include_directories(fabrica_renderer
  PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
  PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(fabrica_renderer
  PUBLIC
    fabrica_core_common
    fabrica_core_logging
    fabrica_rhi
)

target_compile_features(fabrica_renderer PUBLIC cxx_std_20)

target_compile_definitions(fabrica_renderer
  PRIVATE
    FABRICA_RENDERER_DEFAULT_SHADER_DIR="${FABRICA_RENDERER_SHADER_OUTPUT_DIR}"
)

if(FABRICA_BUILD_TESTS)
  add_executable(fabrica_renderer_tests
    tests/camera_data_test.cc
  )

  target_link_libraries(fabrica_renderer_tests
    PRIVATE
      fabrica_core_common
      fabrica_renderer
  )

  add_test(NAME fabrica_renderer_tests COMMAND fabrica_renderer_tests)
endif()
