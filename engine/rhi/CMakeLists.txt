add_library(fabrica_rhi
  src/RHIFactory.cpp
  src/RenderGraph.cpp
)

target_include_directories(fabrica_rhi
  PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(fabrica_rhi
  PUBLIC
    fabrica_core_common
)

target_compile_features(fabrica_rhi PUBLIC cxx_std_20)

add_subdirectory(vulkan)

if(FABRICA_BUILD_TESTS)
  add_executable(fabrica_rhi_tests
    tests/rhi_handle_test.cc
    tests/rhi_factory_test.cc
    tests/render_graph_test.cc
  )

  target_link_libraries(fabrica_rhi_tests
    PRIVATE
      fabrica_core_common
      fabrica_core_logging
      fabrica_rhi
      fabrica_rhi_vulkan
  )

  target_include_directories(fabrica_rhi_tests
    PRIVATE
      ${CMAKE_CURRENT_SOURCE_DIR}/include
      ${CMAKE_CURRENT_SOURCE_DIR}/..
  )

  add_test(NAME fabrica_rhi_tests COMMAND fabrica_rhi_tests)
endif()