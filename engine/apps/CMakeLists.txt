if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR AND NOT FABRICA_APPS_BOOTSTRAP_ACTIVE)
  cmake_minimum_required(VERSION 3.20)
  project(FabricaAppsBootstrap LANGUAGES CXX)

  set(FABRICA_APPS_BOOTSTRAP_ACTIVE ON CACHE INTERNAL "Enable engine root bootstrap when opening engine/apps directly")

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

  message(STATUS "Bootstrapping from engine/apps and delegating to engine/CMakeLists.txt")
  add_subdirectory(.. ${CMAKE_BINARY_DIR}/engine_root)
  return()
endif()

if(FABRICA_GLFW_BACKEND_AVAILABLE)
  add_executable(fabrica_runtime_sample
    runtime_sample/main.cc
  )

  target_link_libraries(fabrica_runtime_sample
    PRIVATE
      fabrica_core_app
  )

  message(STATUS "fabrica_runtime_sample enabled (GLFW provider: ${FABRICA_GLFW_BACKEND_SOURCE})")
else()
  message(STATUS "GLFW backend unavailable: skipping fabrica_runtime_sample target")
  message(STATUS "To enable sample build: install glfw3 package or configure with -DFABRICA_FETCH_GLFW=ON")
endif()
