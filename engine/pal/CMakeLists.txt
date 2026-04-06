add_library(fabrica_pal
  file_system.cc
  threading.cc
)

target_include_directories(fabrica_pal
  PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/..
)

target_link_libraries(fabrica_pal
  PUBLIC
    fabrica_core_async
    fabrica_core_common
    fabrica_core_jobs
    fabrica_core_logging
)

if(FABRICA_BUILD_TESTS)
  add_executable(fabrica_pal_tests
    file_system_test.cc
    test_main.cc
    threading_test.cc
  )

  target_link_libraries(fabrica_pal_tests
    PRIVATE
      fabrica_pal
  )

  add_test(NAME fabrica_pal_tests COMMAND fabrica_pal_tests)
endif()

