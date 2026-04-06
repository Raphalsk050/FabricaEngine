add_library(fabrica_core_common
  common/status.cc
)

target_include_directories(fabrica_core_common
  PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/..
)

target_compile_definitions(fabrica_core_common
  PUBLIC
    $<$<CONFIG:Debug>:FABRICA_DEBUG_BUILD=1>
    $<$<NOT:$<CONFIG:Debug>>:FABRICA_SHIP_BUILD=1>
)

add_library(fabrica_core_memory
  memory/linear_allocator.cc
  memory/stack_allocator.cc
)

target_include_directories(fabrica_core_memory
  PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/..
)

target_link_libraries(fabrica_core_memory
  PUBLIC
    fabrica_core_common
)

add_library(fabrica_core_logging
  logging/logger.cc
)

target_include_directories(fabrica_core_logging
  PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/..
)

target_link_libraries(fabrica_core_logging
  PUBLIC
    fabrica_core_common
)

add_library(fabrica_core_jobs
  jobs/executor.cc
  jobs/simple_foreground_executor.cc
  jobs/task_scheduler.cc
  jobs/thread_pool_executor.cc
)

target_include_directories(fabrica_core_jobs
  PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/..
)

target_link_libraries(fabrica_core_jobs
  PUBLIC
    fabrica_core_common
    fabrica_core_logging
)

add_library(fabrica_core_async
  async/future_group.cc
  async/future_impl.cc
)

target_include_directories(fabrica_core_async
  PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/..
)

target_link_libraries(fabrica_core_async
  PUBLIC
    fabrica_core_common
    fabrica_core_jobs
    fabrica_core_logging
)

add_library(fabrica_core_window
  window/glfw_window_backend.cc
  window/window_system.cc
)

target_include_directories(fabrica_core_window
  PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/..
)

target_link_libraries(fabrica_core_window
  PUBLIC
    fabrica_core_common
    fabrica_core_logging
)

add_library(fabrica_core_assets
  assets/asset_path.cc
  assets/resource_manager.cc
)

target_include_directories(fabrica_core_assets
  PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/..
)

target_link_libraries(fabrica_core_assets
  PUBLIC
    fabrica_core_async
    fabrica_core_common
    fabrica_core_jobs
    fabrica_core_logging
)

if(FABRICA_BUILD_TESTS)
  add_executable(fabrica_core_tests
    async/future_test.cc
    common/holdable_test.cc
    common/invocable_test.cc
    common/status_test.cc
    jobs/executor_test.cc
    jobs/task_scheduler_test.cc
    logging/logger_test.cc
    memory/linear_allocator_test.cc
    memory/pool_allocator_test.cc
    memory/stack_allocator_test.cc
    assets/resource_manager_test.cc
    window/event_queue_test.cc
    window/glfw_window_backend_test.cc
    window/window_system_test.cc
  )

  target_link_libraries(fabrica_core_tests
    PRIVATE
      fabrica_core_common
      fabrica_core_async
      fabrica_core_jobs
      fabrica_core_logging
      fabrica_core_memory
      fabrica_core_assets
      fabrica_core_window
  )

  add_test(NAME fabrica_core_tests COMMAND fabrica_core_tests)
endif()
