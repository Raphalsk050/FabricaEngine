find_package(Vulkan REQUIRED)

find_path(FABRICA_VOLK_INCLUDE_DIR
  NAMES Volk/volk.h
  HINTS ENV VULKAN_SDK
  PATH_SUFFIXES Include
)

find_path(FABRICA_VMA_INCLUDE_DIR
  NAMES vma/vk_mem_alloc.h
  HINTS ENV VULKAN_SDK
  PATH_SUFFIXES Include
)

add_library(fabrica_rhi_vulkan STATIC
  src/VulkanBackend.cpp
  src/VulkanVolk.cpp
  src/VulkanRHIContext.cpp
  src/VulkanInit.cpp
  src/VulkanBuffer.cpp
  src/VulkanTexture.cpp
  src/VulkanSwapChain.cpp
  src/VulkanCommandList.cpp
  src/VulkanPipeline.cpp
  src/VulkanRenderPass.cpp
  src/VulkanDescriptorAllocator.cpp
)

target_compile_features(fabrica_rhi_vulkan PUBLIC cxx_std_20)

target_include_directories(fabrica_rhi_vulkan
  PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${FABRICA_VOLK_INCLUDE_DIR}
    ${FABRICA_VMA_INCLUDE_DIR}
)

target_link_libraries(fabrica_rhi_vulkan
  PUBLIC
    fabrica_rhi
    fabrica_core_common
    fabrica_core_logging
    Vulkan::Vulkan
)

if(TARGET glfw)
  target_link_libraries(fabrica_rhi_vulkan PUBLIC glfw)
  target_compile_definitions(fabrica_rhi_vulkan PRIVATE FABRICA_USE_GLFW=1)
elseif(TARGET glfw3::glfw)
  target_link_libraries(fabrica_rhi_vulkan PUBLIC glfw3::glfw)
  target_compile_definitions(fabrica_rhi_vulkan PRIVATE FABRICA_USE_GLFW=1)
elseif(TARGET glfw3::glfw3)
  target_link_libraries(fabrica_rhi_vulkan PUBLIC glfw3::glfw3)
  target_compile_definitions(fabrica_rhi_vulkan PRIVATE FABRICA_USE_GLFW=1)
endif()