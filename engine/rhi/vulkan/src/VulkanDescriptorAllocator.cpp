#include "VulkanBackendTypes.h"

#include <algorithm>

namespace Fabrica::RHI::Vulkan {
namespace {

std::size_t HashBytes(const void* data, std::size_t size) {
  constexpr std::size_t kFnvOffset = 1469598103934665603ull;
  constexpr std::size_t kFnvPrime = 1099511628211ull;

  const auto* bytes = static_cast<const std::uint8_t*>(data);
  std::size_t hash = kFnvOffset;
  for (std::size_t index = 0; index < size; ++index) {
    hash ^= static_cast<std::size_t>(bytes[index]);
    hash *= kFnvPrime;
  }
  return hash;
}

std::size_t HashDescriptorSetLayout(const RHIDescriptorSetLayoutDesc& desc) {
  return HashBytes(&desc, sizeof(desc));
}

std::size_t HashPipelineLayout(const RHIPipelineLayoutDesc& desc) {
  return HashBytes(&desc, sizeof(desc));
}

std::array<VkDescriptorPoolSize, 6> BuildPoolSizes() {
  return {{{VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 512},
           {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 512},
           {VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 512},
           {VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 256},
           {VK_DESCRIPTOR_TYPE_SAMPLER, 512},
           {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 512}}};
}

}  // namespace

VulkanDescriptorAllocator::VulkanDescriptorAllocator(VulkanContextState& state)
    : state_(&state) {}

VulkanDescriptorAllocator::~VulkanDescriptorAllocator() { Shutdown(); }

bool VulkanDescriptorAllocator::Initialize() { return state_ != nullptr; }

void VulkanDescriptorAllocator::Shutdown() {
  if (state_ == nullptr || state_->device == VK_NULL_HANDLE) {
    return;
  }

  auto destroy_bucket = [&](PoolBucket& bucket) {
    for (VkDescriptorPool pool : bucket.pools) {
      if (pool != VK_NULL_HANDLE) {
        vkDestroyDescriptorPool(state_->device, pool, nullptr);
      }
    }
    bucket.pools.clear();
    bucket.active_pool_index = 0;
  };

  destroy_bucket(per_frame_);
  destroy_bucket(per_draw_);
  destroy_bucket(persistent_);
}

void VulkanDescriptorAllocator::ResetFramePools(std::uint32_t) {
  auto reset_bucket = [&](PoolBucket& bucket) {
    for (VkDescriptorPool pool : bucket.pools) {
      vkResetDescriptorPool(state_->device, pool, 0);
    }
    bucket.active_pool_index = 0;
  };

  reset_bucket(per_frame_);
  reset_bucket(per_draw_);
}

VkDescriptorPool VulkanDescriptorAllocator::AcquirePool(PoolBucket& bucket,
                                                        bool free_individual_sets) {
  const auto pool_sizes = BuildPoolSizes();
  if (bucket.active_pool_index < bucket.pools.size()) {
    return bucket.pools[bucket.active_pool_index];
  }

  VkDescriptorPoolCreateInfo create_info{VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO};
  create_info.poolSizeCount = static_cast<std::uint32_t>(pool_sizes.size());
  create_info.pPoolSizes = pool_sizes.data();
  create_info.maxSets = 512;
  if (free_individual_sets) {
    create_info.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
  }

  VkDescriptorPool pool = VK_NULL_HANDLE;
  if (vkCreateDescriptorPool(state_->device, &create_info, nullptr, &pool) != VK_SUCCESS) {
    return VK_NULL_HANDLE;
  }

  bucket.pools.push_back(pool);
  bucket.active_pool_index = bucket.pools.size() - 1u;
  return pool;
}

VkDescriptorSet VulkanDescriptorAllocator::AllocateSet(
    VkDescriptorSetLayout layout,
    ERHIDescriptorFrequency frequency) {
  PoolBucket* bucket = &persistent_;
  bool free_individual_sets = true;

  if (frequency == ERHIDescriptorFrequency::PerFrame) {
    bucket = &per_frame_;
    free_individual_sets = false;
  } else if (frequency == ERHIDescriptorFrequency::PerDraw) {
    bucket = &per_draw_;
    free_individual_sets = false;
  }

  VkDescriptorSetLayout layouts[] = {layout};
  VkDescriptorSetAllocateInfo alloc_info{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO};
  alloc_info.descriptorSetCount = 1;
  alloc_info.pSetLayouts = layouts;

  while (true) {
    VkDescriptorPool pool = AcquirePool(*bucket, free_individual_sets);
    if (pool == VK_NULL_HANDLE) {
      return VK_NULL_HANDLE;
    }

    alloc_info.descriptorPool = pool;
    VkDescriptorSet descriptor_set = VK_NULL_HANDLE;
    const VkResult result = vkAllocateDescriptorSets(state_->device, &alloc_info,
                                                     &descriptor_set);
    if (result == VK_SUCCESS) {
      return descriptor_set;
    }

    if (result != VK_ERROR_FRAGMENTED_POOL && result != VK_ERROR_OUT_OF_POOL_MEMORY) {
      return VK_NULL_HANDLE;
    }

    ++bucket->active_pool_index;
  }
}

VkDescriptorSetLayout GetOrCreateDescriptorSetLayout(
    VulkanContextState& state,
    const RHIDescriptorSetLayoutDesc& desc) {
  const std::size_t hash = HashDescriptorSetLayout(desc);
  const auto it = state.descriptor_layout_cache.find(hash);
  if (it != state.descriptor_layout_cache.end()) {
    return it->second;
  }

  std::vector<VkDescriptorSetLayoutBinding> bindings(desc.binding_count);
  for (std::uint32_t index = 0; index < desc.binding_count; ++index) {
    bindings[index].binding = desc.bindings[index].binding;
    bindings[index].descriptorType = ToVkDescriptorType(desc.bindings[index].type);
    bindings[index].descriptorCount = desc.bindings[index].count;
    bindings[index].stageFlags = ToVkShaderStage(desc.bindings[index].stage_mask);
  }

  VkDescriptorSetLayoutCreateInfo create_info{
      VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};
  create_info.bindingCount = static_cast<std::uint32_t>(bindings.size());
  create_info.pBindings = bindings.empty() ? nullptr : bindings.data();

  VkDescriptorSetLayout layout = VK_NULL_HANDLE;
  if (vkCreateDescriptorSetLayout(state.device, &create_info, nullptr, &layout) !=
      VK_SUCCESS) {
    return VK_NULL_HANDLE;
  }

  state.descriptor_layout_cache.emplace(hash, layout);
  return layout;
}

VkPipelineLayout GetOrCreatePipelineLayout(VulkanContextState& state,
                                           const RHIPipelineLayoutDesc& desc) {
  const std::size_t hash = HashPipelineLayout(desc);
  const auto it = state.pipeline_layout_cache.find(hash);
  if (it != state.pipeline_layout_cache.end()) {
    return it->second;
  }

  std::vector<VkDescriptorSetLayout> set_layouts(desc.set_layout_count);
  for (std::uint32_t index = 0; index < desc.set_layout_count; ++index) {
    set_layouts[index] = GetOrCreateDescriptorSetLayout(state, desc.set_layouts[index]);
  }

  std::vector<VkPushConstantRange> push_constants(desc.push_constant_range_count);
  for (std::uint32_t index = 0; index < desc.push_constant_range_count; ++index) {
    push_constants[index].offset = desc.push_constant_ranges[index].offset;
    push_constants[index].size = desc.push_constant_ranges[index].size;
    push_constants[index].stageFlags =
        ToVkShaderStage(desc.push_constant_ranges[index].stage_mask);
  }

  VkPipelineLayoutCreateInfo create_info{VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
  create_info.setLayoutCount = static_cast<std::uint32_t>(set_layouts.size());
  create_info.pSetLayouts = set_layouts.empty() ? nullptr : set_layouts.data();
  create_info.pushConstantRangeCount =
      static_cast<std::uint32_t>(push_constants.size());
  create_info.pPushConstantRanges =
      push_constants.empty() ? nullptr : push_constants.data();

  VkPipelineLayout layout = VK_NULL_HANDLE;
  if (vkCreatePipelineLayout(state.device, &create_info, nullptr, &layout) !=
      VK_SUCCESS) {
    return VK_NULL_HANDLE;
  }

  state.pipeline_layout_cache.emplace(hash, layout);
  return layout;
}

RHIDescriptorSetCreateResult CreateVulkanDescriptorSet(VulkanContextState& state,
                                                       const RHIDescriptorSetDesc& desc) {
  const RHIDescriptorSetHandle handle =
      state.descriptor_sets.Allocate([&](VulkanDescriptorSetResource& resource) {
        resource.owner = &state;
        resource.desc = desc;
        resource.layout = GetOrCreateDescriptorSetLayout(state, desc.layout);
        resource.descriptor_set =
            state.descriptor_allocator->AllocateSet(resource.layout, desc.frequency);
      });

  if (!handle.IsValid()) {
    return {.result = RHIResult::ErrorOutOfMemory};
  }

  const VulkanDescriptorSetResource* resource = state.descriptor_sets.Resolve(handle);
  if (resource == nullptr || resource->layout == VK_NULL_HANDLE ||
      resource->descriptor_set == VK_NULL_HANDLE) {
    state.descriptor_sets.Destroy(handle, [&](VulkanDescriptorSetResource& failed_resource) {
      DestroyVulkanDescriptorSet(state, failed_resource);
    });
    return {.result = RHIResult::ErrorOutOfMemory};
  }

  return {.handle = handle, .result = RHIResult::Success};
}

void DestroyVulkanDescriptorSet(VulkanContextState&, VulkanDescriptorSetResource& resource) {
  resource = VulkanDescriptorSetResource{};
}

void VulkanDescriptorSetResource::BindBuffer(
    std::uint32_t binding,
    const RHIDescriptorBufferWriteDesc& write) {
  if (binding >= has_buffer_write.size()) {
    return;
  }
  buffer_writes[binding] = write;
  has_buffer_write[binding] = true;
}

void VulkanDescriptorSetResource::BindTexture(
    std::uint32_t binding,
    const RHIDescriptorTextureWriteDesc& write) {
  if (binding >= has_texture_write.size()) {
    return;
  }
  texture_writes[binding] = write;
  has_texture_write[binding] = true;
}

void VulkanDescriptorSetResource::BindSampler(
    std::uint32_t binding,
    const RHIDescriptorSamplerWriteDesc& write) {
  if (binding >= has_sampler_write.size()) {
    return;
  }
  sampler_writes[binding] = write;
  has_sampler_write[binding] = true;
}

void VulkanDescriptorSetResource::FlushWrites() {
  if (owner == nullptr || descriptor_set == VK_NULL_HANDLE) {
    return;
  }

  std::array<VkWriteDescriptorSet, kMaxDescriptorBindings> writes{};
  std::array<VkDescriptorBufferInfo, kMaxDescriptorBindings> buffer_infos{};
  std::array<VkDescriptorImageInfo, kMaxDescriptorBindings> image_infos{};
  std::uint32_t write_count = 0;
  std::uint32_t image_info_count = 0;
  std::uint32_t buffer_info_count = 0;

  for (std::uint32_t index = 0; index < desc.layout.binding_count; ++index) {
    const RHIDescriptorBindingDesc& binding_desc = desc.layout.bindings[index];
    VkWriteDescriptorSet& write = writes[write_count];
    write = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
    write.dstSet = descriptor_set;
    write.dstBinding = binding_desc.binding;
    write.descriptorCount = binding_desc.count;
    write.descriptorType = ToVkDescriptorType(binding_desc.type);

    if ((binding_desc.type == ERHIDescriptorType::UniformBuffer ||
         binding_desc.type == ERHIDescriptorType::StorageBuffer) &&
        has_buffer_write[binding_desc.binding]) {
      const VulkanBufferResource* buffer =
          owner->buffers.Resolve(buffer_writes[binding_desc.binding].buffer);
      if (buffer == nullptr) {
        continue;
      }
      buffer_infos[buffer_info_count].buffer = buffer->buffer;
      buffer_infos[buffer_info_count].offset = buffer_writes[binding_desc.binding].offset;
      buffer_infos[buffer_info_count].range = buffer_writes[binding_desc.binding].range;
      write.pBufferInfo = &buffer_infos[buffer_info_count++];
      ++write_count;
      continue;
    }

    if ((binding_desc.type == ERHIDescriptorType::SampledTexture ||
         binding_desc.type == ERHIDescriptorType::StorageTexture ||
         binding_desc.type == ERHIDescriptorType::CombinedImageSampler ||
         binding_desc.type == ERHIDescriptorType::Sampler) &&
        (has_texture_write[binding_desc.binding] || has_sampler_write[binding_desc.binding])) {
      if (has_texture_write[binding_desc.binding]) {
        const VulkanTextureResource* texture =
            owner->textures.Resolve(texture_writes[binding_desc.binding].texture);
        if (texture != nullptr) {
          image_infos[image_info_count].imageView = texture->image_view;
          image_infos[image_info_count].imageLayout =
              ToVkImageLayout(texture_writes[binding_desc.binding].layout);
        }
      }
      if (has_sampler_write[binding_desc.binding]) {
        const VulkanSamplerResource* sampler =
            owner->samplers.Resolve(sampler_writes[binding_desc.binding].sampler);
        if (sampler != nullptr) {
          image_infos[image_info_count].sampler = sampler->sampler;
        }
      }
      write.pImageInfo = &image_infos[image_info_count++];
      ++write_count;
    }
  }

  if (write_count > 0) {
    vkUpdateDescriptorSets(owner->device, write_count, writes.data(), 0, nullptr);
  }
}

bool VulkanFenceResource::IsSignaled() const {
  return owner != nullptr && fence != VK_NULL_HANDLE &&
         vkGetFenceStatus(owner->device, fence) == VK_SUCCESS;
}

}  // namespace Fabrica::RHI::Vulkan
