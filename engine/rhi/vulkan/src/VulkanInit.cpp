#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#define VK_USE_PLATFORM_WIN32_KHR
#include <windows.h>
#endif

#define VMA_IMPLEMENTATION
#include "VulkanBackendTypes.h"

#include <fstream>

#if defined(FABRICA_USE_GLFW)
#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>
#endif

namespace Fabrica::RHI::Vulkan {
namespace {

VKAPI_ATTR VkBool32 VKAPI_CALL DebugCallback(
    VkDebugUtilsMessageSeverityFlagBitsEXT severity,
    VkDebugUtilsMessageTypeFlagsEXT,
    const VkDebugUtilsMessengerCallbackDataEXT *callback_data, void *) {
  if (callback_data == nullptr || callback_data->pMessage == nullptr) {
    return VK_FALSE;
  }

  if (severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT) {
    FABRICA_LOG(Render, Error) << "[RHI] " << callback_data->pMessage;
  } else if (severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT) {
    FABRICA_LOG(Render, Warning) << "[RHI] " << callback_data->pMessage;
  } else {
    FABRICA_LOG(Render, Debug) << "[RHI] " << callback_data->pMessage;
  }
  return VK_FALSE;
}

std::vector<const char *> BuildInstanceExtensions(const RHIContextDesc &desc) {
  std::vector<const char *> extensions;

  if (desc.window_handle_type == ERHIWindowHandleType::GLFW) {
#if defined(FABRICA_USE_GLFW)
    std::uint32_t count = 0;
    const char **glfw_extensions = glfwGetRequiredInstanceExtensions(&count);
    for (std::uint32_t index = 0; index < count; ++index) {
      extensions.push_back(glfw_extensions[index]);
    }
#endif
#if defined(_WIN32)
  } else if (desc.window_handle_type == ERHIWindowHandleType::Win32) {
    extensions.push_back(VK_KHR_SURFACE_EXTENSION_NAME);
    extensions.push_back(VK_KHR_WIN32_SURFACE_EXTENSION_NAME);
  }
#endif

  if (desc.enable_validation) {
    extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
  }

  return extensions;
}

bool CreateSurface(VulkanContextState &state) {
  if (state.desc.window_handle == nullptr ||
      state.desc.window_handle_type == ERHIWindowHandleType::None) {
    return true;
  }

  if (state.desc.window_handle_type == ERHIWindowHandleType::GLFW) {
#if defined(FABRICA_USE_GLFW)
    const VkResult result = glfwCreateWindowSurface(
        state.instance, static_cast<GLFWwindow *>(state.desc.window_handle),
        nullptr, &state.surface);
    return result == VK_SUCCESS;
#else
      return false;
#endif
  }

#if defined(_WIN32)
  VkWin32SurfaceCreateInfoKHR create_info{
      VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR};
  create_info.hinstance = static_cast<HINSTANCE>(state.desc.window_instance);
  create_info.hwnd = static_cast<HWND>(state.desc.window_handle);
  return vkCreateWin32SurfaceKHR(state.instance, &create_info, nullptr,
                                 &state.surface) == VK_SUCCESS;
#else
    return false;
#endif
}

struct QueueSelection {
  std::uint32_t graphics = kInvalidQueueFamily;
  std::uint32_t compute = kInvalidQueueFamily;
  std::uint32_t transfer = kInvalidQueueFamily;
};

QueueSelection SelectQueues(VulkanContextState &state,
                            VkPhysicalDevice device) {
  QueueSelection selection{};

  std::uint32_t family_count = 0;
  vkGetPhysicalDeviceQueueFamilyProperties(device, &family_count, nullptr);
  std::vector<VkQueueFamilyProperties> families(family_count);
  vkGetPhysicalDeviceQueueFamilyProperties(device, &family_count,
                                           families.data());

  for (std::uint32_t index = 0; index < family_count; ++index) {
    const auto &family = families[index];
    VkBool32 present_support = VK_TRUE;
    if (state.surface != VK_NULL_HANDLE) {
      vkGetPhysicalDeviceSurfaceSupportKHR(device, index, state.surface,
                                           &present_support);
    }

    if (selection.graphics == kInvalidQueueFamily &&
        (family.queueFlags & VK_QUEUE_GRAPHICS_BIT) != 0 &&
        present_support == VK_TRUE) {
      selection.graphics = index;
    }
  }

  for (std::uint32_t index = 0; index < family_count; ++index) {
    const auto &family = families[index];
    if (selection.compute == kInvalidQueueFamily &&
        (family.queueFlags & VK_QUEUE_COMPUTE_BIT) != 0 &&
        (family.queueFlags & VK_QUEUE_GRAPHICS_BIT) == 0) {
      selection.compute = index;
    }
    if (selection.transfer == kInvalidQueueFamily &&
        (family.queueFlags & VK_QUEUE_TRANSFER_BIT) != 0 &&
        (family.queueFlags & VK_QUEUE_GRAPHICS_BIT) == 0 &&
        (family.queueFlags & VK_QUEUE_COMPUTE_BIT) == 0) {
      selection.transfer = index;
    }
  }

  if (selection.compute == kInvalidQueueFamily) {
    selection.compute = selection.graphics;
  }
  if (selection.transfer == kInvalidQueueFamily) {
    selection.transfer = selection.compute != kInvalidQueueFamily
                             ? selection.compute
                             : selection.graphics;
  }

  return selection;
}

int ScoreDevice(VulkanContextState &state, VkPhysicalDevice device) {
  VkPhysicalDeviceProperties properties{};
  VkPhysicalDeviceMemoryProperties memory_properties{};
  vkGetPhysicalDeviceProperties(device, &properties);
  vkGetPhysicalDeviceMemoryProperties(device, &memory_properties);

  const QueueSelection queues = SelectQueues(state, device);
  if (queues.graphics == kInvalidQueueFamily) {
    return -1;
  }

  int score = 0;
  if (properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU) {
    score += 1000;
  }
  score += static_cast<int>(properties.limits.maxImageDimension2D);
  if (queues.compute != queues.graphics) {
    score += 128;
  }
  if (queues.transfer != queues.graphics && queues.transfer != queues.compute) {
    score += 64;
  }
  return score;
}

void FillDeviceInfo(VulkanContextState &state, VkPhysicalDevice device,
                    const QueueSelection &queues, int score) {
  VkPhysicalDeviceProperties properties{};
  VkPhysicalDeviceMemoryProperties memory_properties{};
  vkGetPhysicalDeviceProperties(device, &properties);
  vkGetPhysicalDeviceMemoryProperties(device, &memory_properties);

  std::memcpy(state.device_info.device_name, properties.deviceName,
              sizeof(state.device_info.device_name));
  state.device_info.vendor_id = properties.vendorID;
  state.device_info.device_id = properties.deviceID;
  state.device_info.api_version = properties.apiVersion;
  state.device_info.driver_version = properties.driverVersion;
  state.device_info.graphics_queue_family = queues.graphics;
  state.device_info.compute_queue_family = queues.compute;
  state.device_info.transfer_queue_family = queues.transfer;
  state.device_info.has_async_compute = queues.compute != queues.graphics;
  state.device_info.has_async_transfer =
      queues.transfer != queues.graphics && queues.transfer != queues.compute;
  state.device_info.is_discrete_gpu =
      properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU;
  state.device_info.score = score;

  for (std::uint32_t index = 0; index < memory_properties.memoryHeapCount;
       ++index) {
    const bool device_local = (memory_properties.memoryHeaps[index].flags &
                               VK_MEMORY_HEAP_DEVICE_LOCAL_BIT) != 0;
    if (device_local) {
      state.device_info.device_local_memory_bytes +=
          memory_properties.memoryHeaps[index].size;
    } else {
      state.device_info.host_visible_memory_bytes +=
          memory_properties.memoryHeaps[index].size;
    }
  }
}

VkPipelineCache CreatePipelineCache(VulkanContextState &state) {
  std::ifstream file(state.desc.pipeline_cache_path,
                     std::ios::binary | std::ios::ate);
  std::vector<char> payload;
  if (file.is_open()) {
    const auto size = file.tellg();
    payload.resize(static_cast<std::size_t>(size));
    file.seekg(0);
    file.read(payload.data(), size);
  }

  VkPipelineCacheCreateInfo create_info{
      VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO};
  create_info.initialDataSize = payload.size();
  create_info.pInitialData = payload.empty() ? nullptr : payload.data();

  VkPipelineCache cache = VK_NULL_HANDLE;
  if (vkCreatePipelineCache(state.device, &create_info, nullptr, &cache) !=
      VK_SUCCESS) {
    return VK_NULL_HANDLE;
  }
  return cache;
}

void SavePipelineCache(const VulkanContextState &state) {
  if (state.native_pipeline_cache == VK_NULL_HANDLE) {
    return;
  }

  std::size_t data_size = 0;
  if (vkGetPipelineCacheData(state.device, state.native_pipeline_cache,
                             &data_size, nullptr) != VK_SUCCESS ||
      data_size == 0) {
    return;
  }

  std::vector<std::byte> bytes(data_size);
  if (vkGetPipelineCacheData(state.device, state.native_pipeline_cache,
                             &data_size, bytes.data()) != VK_SUCCESS) {
    return;
  }

  std::ofstream file(state.desc.pipeline_cache_path,
                     std::ios::binary | std::ios::trunc);
  if (!file.is_open()) {
    return;
  }
  file.write(reinterpret_cast<const char *>(bytes.data()),
             static_cast<std::streamsize>(bytes.size()));
}

} // namespace

bool InitializeVulkanDevice(VulkanContextState &state,
                            const RHIContextDesc &desc) {
  state.desc = desc;
  state.validation_enabled = desc.enable_validation;

  if (volkInitialize() != VK_SUCCESS) {
    return false;
  }

  std::vector<const char *> extensions = BuildInstanceExtensions(desc);
  std::vector<const char *> validation_layers;
  if (desc.enable_validation) {
    validation_layers.push_back("VK_LAYER_KHRONOS_validation");
  }

  VkApplicationInfo application_info{VK_STRUCTURE_TYPE_APPLICATION_INFO};
  application_info.pApplicationName = desc.application_name;
  application_info.pEngineName = desc.engine_name;
  application_info.apiVersion = VK_API_VERSION_1_3;

  VkInstanceCreateInfo instance_create_info{
      VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO};
  instance_create_info.pApplicationInfo = &application_info;
  instance_create_info.enabledExtensionCount =
      static_cast<std::uint32_t>(extensions.size());
  instance_create_info.ppEnabledExtensionNames = extensions.data();
  instance_create_info.enabledLayerCount =
      static_cast<std::uint32_t>(validation_layers.size());
  instance_create_info.ppEnabledLayerNames = validation_layers.data();

  if (vkCreateInstance(&instance_create_info, nullptr, &state.instance) !=
      VK_SUCCESS) {
    return false;
  }

  volkLoadInstance(state.instance);
  state.set_debug_name = vkSetDebugUtilsObjectNameEXT;
  state.cmd_begin_debug_label = vkCmdBeginDebugUtilsLabelEXT;
  state.cmd_end_debug_label = vkCmdEndDebugUtilsLabelEXT;
  state.cmd_insert_debug_label = vkCmdInsertDebugUtilsLabelEXT;

  if (desc.enable_validation) {
    VkDebugUtilsMessengerCreateInfoEXT messenger_info{
        VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT};
    messenger_info.messageSeverity =
        VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
        VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
    messenger_info.messageType =
        VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
        VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
        VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
    messenger_info.pfnUserCallback = &DebugCallback;
    vkCreateDebugUtilsMessengerEXT(state.instance, &messenger_info, nullptr,
                                   &state.debug_messenger);
  }

  if (!CreateSurface(state)) {
    ShutdownVulkanDevice(state);
    return false;
  }

  std::uint32_t device_count = 0;
  vkEnumeratePhysicalDevices(state.instance, &device_count, nullptr);
  if (device_count == 0) {
    ShutdownVulkanDevice(state);
    return false;
  }

  std::vector<VkPhysicalDevice> devices(device_count);
  vkEnumeratePhysicalDevices(state.instance, &device_count, devices.data());

  int best_score = -1;
  QueueSelection best_queues{};
  for (VkPhysicalDevice device : devices) {
    const int score = ScoreDevice(state, device);
    if (score > best_score) {
      best_score = score;
      best_queues = SelectQueues(state, device);
      state.physical_device = device;
    }
  }

  if (state.physical_device == VK_NULL_HANDLE) {
    ShutdownVulkanDevice(state);
    return false;
  }

  FillDeviceInfo(state, state.physical_device, best_queues, best_score);
  state.graphics_queue_family = best_queues.graphics;
  state.compute_queue_family = best_queues.compute;
  state.transfer_queue_family = best_queues.transfer;

  const float queue_priority = 1.0f;
  std::vector<VkDeviceQueueCreateInfo> queue_infos;
  std::array<std::uint32_t, 3> unique_queues{state.graphics_queue_family,
                                             state.compute_queue_family,
                                             state.transfer_queue_family};
  for (std::size_t index = 0; index < unique_queues.size(); ++index) {
    bool duplicate = false;
    for (std::size_t previous = 0; previous < index; ++previous) {
      duplicate = duplicate || unique_queues[index] == unique_queues[previous];
    }
    if (duplicate || unique_queues[index] == kInvalidQueueFamily) {
      continue;
    }

    VkDeviceQueueCreateInfo queue_info{
        VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO};
    queue_info.queueFamilyIndex = unique_queues[index];
    queue_info.queueCount = 1;
    queue_info.pQueuePriorities = &queue_priority;
    queue_infos.push_back(queue_info);
  }

  VkPhysicalDeviceDynamicRenderingFeatures dynamic_rendering{
      VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DYNAMIC_RENDERING_FEATURES};
  dynamic_rendering.dynamicRendering = VK_TRUE;

  VkPhysicalDeviceSynchronization2Features synchronization2{
      VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SYNCHRONIZATION_2_FEATURES};
  synchronization2.synchronization2 = VK_TRUE;
  synchronization2.pNext = &dynamic_rendering;

  VkPhysicalDeviceFeatures2 features{
      VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2};
  features.pNext = &synchronization2;
  vkGetPhysicalDeviceFeatures2(state.physical_device, &features);
  features.features.samplerAnisotropy = VK_TRUE;

  const char *device_extensions[] = {VK_KHR_SWAPCHAIN_EXTENSION_NAME};

  VkDeviceCreateInfo device_create_info{VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO};
  device_create_info.queueCreateInfoCount =
      static_cast<std::uint32_t>(queue_infos.size());
  device_create_info.pQueueCreateInfos = queue_infos.data();
  device_create_info.enabledExtensionCount = 1;
  device_create_info.ppEnabledExtensionNames = device_extensions;
  device_create_info.pNext = &features;

  if (vkCreateDevice(state.physical_device, &device_create_info, nullptr,
                     &state.device) != VK_SUCCESS) {
    ShutdownVulkanDevice(state);
    return false;
  }

  volkLoadDevice(state.device);
  state.set_debug_name = vkSetDebugUtilsObjectNameEXT;
  state.cmd_begin_debug_label = vkCmdBeginDebugUtilsLabelEXT;
  state.cmd_end_debug_label = vkCmdEndDebugUtilsLabelEXT;
  state.cmd_insert_debug_label = vkCmdInsertDebugUtilsLabelEXT;

  vkGetDeviceQueue(state.device, state.graphics_queue_family, 0,
                   &state.graphics_queue);
  vkGetDeviceQueue(state.device, state.compute_queue_family, 0,
                   &state.compute_queue);
  vkGetDeviceQueue(state.device, state.transfer_queue_family, 0,
                   &state.transfer_queue);

  VmaVulkanFunctions functions{};
  functions.vkGetInstanceProcAddr = vkGetInstanceProcAddr;
  functions.vkGetDeviceProcAddr = vkGetDeviceProcAddr;
  VmaAllocatorCreateInfo allocator_info{};
  allocator_info.flags = VMA_ALLOCATOR_CREATE_EXT_MEMORY_BUDGET_BIT;
  allocator_info.vulkanApiVersion = VK_API_VERSION_1_3;
  allocator_info.physicalDevice = state.physical_device;
  allocator_info.device = state.device;
  allocator_info.instance = state.instance;
  allocator_info.pVulkanFunctions = &functions;

  if (vmaCreateAllocator(&allocator_info, &state.allocator) != VK_SUCCESS) {
    ShutdownVulkanDevice(state);
    return false;
  }

  state.native_pipeline_cache = CreatePipelineCache(state);

  for (std::uint32_t frame_index = 0; frame_index < kFramesInFlight;
       ++frame_index) {
    FrameResources &frame = state.frames[frame_index];
    frame.deferred_buffers.reserve(256);
    frame.deferred_textures.reserve(128);
    frame.deferred_samplers.reserve(64);
    frame.deferred_shaders.reserve(64);
    frame.deferred_render_passes.reserve(64);
    frame.deferred_pipelines.reserve(64);
    frame.deferred_descriptor_sets.reserve(128);

    const std::array<std::uint32_t, 3> family_indices{
        state.graphics_queue_family, state.compute_queue_family,
        state.transfer_queue_family};
    for (std::size_t queue_index = 0; queue_index < family_indices.size();
         ++queue_index) {
      VkCommandPoolCreateInfo pool_info{
          VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO};
      pool_info.queueFamilyIndex = family_indices[queue_index];
      pool_info.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
      if (vkCreateCommandPool(state.device, &pool_info, nullptr,
                              &frame.command_pools[queue_index]) !=
          VK_SUCCESS) {
        ShutdownVulkanDevice(state);
        return false;
      }

      VkCommandBufferAllocateInfo alloc_info{
          VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
      alloc_info.commandPool = frame.command_pools[queue_index];
      alloc_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
      alloc_info.commandBufferCount = 1;
      if (vkAllocateCommandBuffers(state.device, &alloc_info,
                                   &frame.command_buffers[queue_index]) !=
          VK_SUCCESS) {
        ShutdownVulkanDevice(state);
        return false;
      }
    }

    for (std::size_t semaphore_index = 0;
         semaphore_index < frame.submission_semaphores.size();
         ++semaphore_index) {
      VkSemaphoreCreateInfo semaphore_info{
          VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};
      if (vkCreateSemaphore(state.device, &semaphore_info, nullptr,
                            &frame.submission_semaphores[semaphore_index]) !=
          VK_SUCCESS) {
        ShutdownVulkanDevice(state);
        return false;
      }
    }

    VkSemaphoreCreateInfo semaphore_info{
        VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};
    if (vkCreateSemaphore(state.device, &semaphore_info, nullptr,
                          &frame.image_available) != VK_SUCCESS ||
        vkCreateSemaphore(state.device, &semaphore_info, nullptr,
                          &frame.user_fence_semaphore) != VK_SUCCESS) {
      ShutdownVulkanDevice(state);
      return false;
    }

    VkFenceCreateInfo fence_info{VK_STRUCTURE_TYPE_FENCE_CREATE_INFO};
    fence_info.flags = VK_FENCE_CREATE_SIGNALED_BIT;
    if (vkCreateFence(state.device, &fence_info, nullptr, &frame.frame_fence) !=
        VK_SUCCESS) {
      ShutdownVulkanDevice(state);
      return false;
    }

    frame.command_lists[0] =
        std::make_unique<VulkanCommandList>(state, ERHIQueueType::Graphics);
    frame.command_lists[1] =
        std::make_unique<VulkanCommandList>(state, ERHIQueueType::Compute);
    frame.command_lists[2] =
        std::make_unique<VulkanCommandList>(state, ERHIQueueType::Transfer);
    for (std::size_t queue_index = 0; queue_index < frame.command_lists.size();
         ++queue_index) {
      frame.command_lists[queue_index]->Attach(
          frame.command_buffers[queue_index], frame_index);
    }
  }

  VkCommandPoolCreateInfo immediate_pool{
      VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO};
  immediate_pool.queueFamilyIndex = state.graphics_queue_family;
  immediate_pool.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
  if (vkCreateCommandPool(state.device, &immediate_pool, nullptr,
                          &state.immediate_command_pool) != VK_SUCCESS) {
    ShutdownVulkanDevice(state);
    return false;
  }

  state.initialized = true;
  return true;
}

void ShutdownVulkanDevice(VulkanContextState &state) {
  if (state.device != VK_NULL_HANDLE) {
    vkDeviceWaitIdle(state.device);
    SavePipelineCache(state);
  }

  for (auto &[_, layout] : state.pipeline_layout_cache) {
    if (layout != VK_NULL_HANDLE && state.device != VK_NULL_HANDLE) {
      vkDestroyPipelineLayout(state.device, layout, nullptr);
    }
  }
  state.pipeline_layout_cache.clear();

  for (auto &[_, layout] : state.descriptor_layout_cache) {
    if (layout != VK_NULL_HANDLE && state.device != VK_NULL_HANDLE) {
      vkDestroyDescriptorSetLayout(state.device, layout, nullptr);
    }
  }
  state.descriptor_layout_cache.clear();

  if (state.native_pipeline_cache != VK_NULL_HANDLE &&
      state.device != VK_NULL_HANDLE) {
    vkDestroyPipelineCache(state.device, state.native_pipeline_cache, nullptr);
    state.native_pipeline_cache = VK_NULL_HANDLE;
  }

  for (FrameResources &frame : state.frames) {
    for (VkSemaphore semaphore : frame.submission_semaphores) {
      if (semaphore != VK_NULL_HANDLE && state.device != VK_NULL_HANDLE) {
        vkDestroySemaphore(state.device, semaphore, nullptr);
      }
    }
    if (frame.image_available != VK_NULL_HANDLE &&
        state.device != VK_NULL_HANDLE) {
      vkDestroySemaphore(state.device, frame.image_available, nullptr);
    }
    if (frame.user_fence_semaphore != VK_NULL_HANDLE &&
        state.device != VK_NULL_HANDLE) {
      vkDestroySemaphore(state.device, frame.user_fence_semaphore, nullptr);
    }
    if (frame.frame_fence != VK_NULL_HANDLE && state.device != VK_NULL_HANDLE) {
      vkDestroyFence(state.device, frame.frame_fence, nullptr);
    }
    for (VkCommandPool pool : frame.command_pools) {
      if (pool != VK_NULL_HANDLE && state.device != VK_NULL_HANDLE) {
        vkDestroyCommandPool(state.device, pool, nullptr);
      }
    }
  }

  if (state.immediate_command_pool != VK_NULL_HANDLE &&
      state.device != VK_NULL_HANDLE) {
    vkDestroyCommandPool(state.device, state.immediate_command_pool, nullptr);
    state.immediate_command_pool = VK_NULL_HANDLE;
  }

  if (state.allocator != VK_NULL_HANDLE) {
    vmaDestroyAllocator(state.allocator);
    state.allocator = VK_NULL_HANDLE;
  }

  if (state.device != VK_NULL_HANDLE) {
    vkDestroyDevice(state.device, nullptr);
    state.device = VK_NULL_HANDLE;
  }

  if (state.surface != VK_NULL_HANDLE && state.instance != VK_NULL_HANDLE) {
    vkDestroySurfaceKHR(state.instance, state.surface, nullptr);
    state.surface = VK_NULL_HANDLE;
  }

  if (state.debug_messenger != VK_NULL_HANDLE &&
      state.instance != VK_NULL_HANDLE) {
    vkDestroyDebugUtilsMessengerEXT(state.instance, state.debug_messenger,
                                    nullptr);
    state.debug_messenger = VK_NULL_HANDLE;
  }

  if (state.instance != VK_NULL_HANDLE) {
    vkDestroyInstance(state.instance, nullptr);
    state.instance = VK_NULL_HANDLE;
  }

  state.initialized = false;
}

} // namespace Fabrica::RHI::Vulkan
