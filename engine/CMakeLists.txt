cmake_minimum_required(VERSION 3.20)
project(FabricaEngine LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(FABRICA_BUILD_TESTS "Build FabricaEngine unit tests" ON)
option(FABRICA_BUILD_SAMPLES "Build FabricaEngine sample applications" ON)
option(FABRICA_ENABLE_GLFW_WINDOW_BACKEND "Enable GLFW-based window backend" ON)
option(FABRICA_FETCH_GLFW "Fetch GLFW with CMake when no system package is available" OFF)
set(FABRICA_GLFW_FETCH_TAG "3.4" CACHE STRING "GLFW git tag used when FABRICA_FETCH_GLFW is ON")

if(FABRICA_BUILD_TESTS)
  include(CTest)
endif()

if(MSVC)
  add_compile_options(/W4 /permissive- /FS)
else()
  add_compile_options(-Wall -Wextra -Wpedantic)
endif()

set(FABRICA_GLFW_BACKEND_AVAILABLE OFF)
set(FABRICA_GLFW_BACKEND_SOURCE "none")

add_subdirectory(core)
add_subdirectory(pal)

if(FABRICA_BUILD_SAMPLES)
  add_subdirectory(apps)
endif()
