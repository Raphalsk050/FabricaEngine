#include "VulkanBackendTypes.h"

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

VkPipelineShaderStageCreateInfo BuildStage(const VulkanShaderResource& shader) {
  VkPipelineShaderStageCreateInfo stage{VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO};
  stage.stage = static_cast<VkShaderStageFlagBits>(ToVkShaderStage(shader.desc.stage));
  stage.module = shader.module;
  stage.pName = shader.desc.entry_point;
  return stage;
}

std::size_t HashGraphicsPipeline(const RHIGraphicsPipelineDesc& desc) {
  return HashBytes(&desc, sizeof(desc));
}

std::size_t HashComputePipeline(const RHIComputePipelineDesc& desc) {
  return HashBytes(&desc, sizeof(desc));
}

}  // namespace

RHISamplerCreateResult CreateVulkanSampler(VulkanContextState& state,
                                           const RHISamplerDesc& desc) {
  const RHISamplerHandle handle = state.samplers.Allocate([&](VulkanSamplerResource& resource) {
    resource.desc = desc;

    VkSamplerCreateInfo create_info{VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO};
    create_info.magFilter = ToVkFilter(desc.mag_filter);
    create_info.minFilter = ToVkFilter(desc.min_filter);
    create_info.mipmapMode = ToVkMipFilter(desc.mip_filter);
    create_info.addressModeU = ToVkAddressMode(desc.address_u);
    create_info.addressModeV = ToVkAddressMode(desc.address_v);
    create_info.addressModeW = ToVkAddressMode(desc.address_w);
    create_info.mipLodBias = desc.mip_lod_bias;
    create_info.anisotropyEnable = desc.anisotropy_enable ? VK_TRUE : VK_FALSE;
    create_info.maxAnisotropy = desc.max_anisotropy;
    create_info.compareEnable = desc.compare_enable ? VK_TRUE : VK_FALSE;
    create_info.compareOp = ToVkCompareOp(desc.compare_op);
    create_info.minLod = desc.min_lod;
    create_info.maxLod = desc.max_lod;
    create_info.borderColor = ToVkBorderColor(desc.border_color);
    vkCreateSampler(state.device, &create_info, nullptr, &resource.sampler);
  });

  if (!handle.IsValid()) {
    return {.result = RHIResult::ErrorOutOfMemory};
  }

  const VulkanSamplerResource* resource = state.samplers.Resolve(handle);
  if (resource == nullptr || resource->sampler == VK_NULL_HANDLE) {
    state.samplers.Destroy(handle, [&](VulkanSamplerResource& failed_resource) {
      DestroyVulkanSampler(state, failed_resource);
    });
    return {.result = RHIResult::ErrorOutOfMemory};
  }

  return {.handle = handle, .result = RHIResult::Success};
}

RHIShaderCreateResult CreateVulkanShader(VulkanContextState& state,
                                         const RHIShaderDesc& desc) {
  const RHIShaderHandle handle = state.shaders.Allocate([&](VulkanShaderResource& resource) {
    resource.desc = desc;

    VkShaderModuleCreateInfo create_info{VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO};
    create_info.codeSize = static_cast<std::size_t>(desc.bytecode_size);
    create_info.pCode = desc.bytecode;
    vkCreateShaderModule(state.device, &create_info, nullptr, &resource.module);
  });

  if (!handle.IsValid()) {
    return {.result = RHIResult::ErrorOutOfMemory};
  }

  const VulkanShaderResource* resource = state.shaders.Resolve(handle);
  if (resource == nullptr || resource->module == VK_NULL_HANDLE) {
    state.shaders.Destroy(handle, [&](VulkanShaderResource& failed_resource) {
      DestroyVulkanShader(state, failed_resource);
    });
    return {.result = RHIResult::ErrorValidation};
  }

  return {.handle = handle, .result = RHIResult::Success};
}

RHIFenceCreateResult CreateVulkanFence(VulkanContextState& state, bool signaled) {
  const RHIFenceHandle handle = state.fences.Allocate([&](VulkanFenceResource& resource) {
    resource.owner = &state;
    VkFenceCreateInfo create_info{VK_STRUCTURE_TYPE_FENCE_CREATE_INFO};
    create_info.flags = signaled ? VK_FENCE_CREATE_SIGNALED_BIT : 0;
    vkCreateFence(state.device, &create_info, nullptr, &resource.fence);
  });

  if (!handle.IsValid()) {
    return {.result = RHIResult::ErrorOutOfMemory};
  }

  const VulkanFenceResource* resource = state.fences.Resolve(handle);
  if (resource == nullptr || resource->fence == VK_NULL_HANDLE) {
    state.fences.Destroy(handle, [&](VulkanFenceResource& failed_resource) {
      DestroyVulkanFence(state, failed_resource);
    });
    return {.result = RHIResult::ErrorOutOfMemory};
  }

  return {.handle = handle, .result = RHIResult::Success};
}

RHIPipelineCreateResult CreateVulkanGraphicsPipeline(VulkanContextState& state,
                                                     const RHIGraphicsPipelineDesc& desc) {
  const RHIPipelineHandle handle = state.pipelines.Allocate([&](VulkanPipelineResource& resource) {
    resource.type = ERHIPipelineType::Graphics;
    resource.graphics_desc = desc;
    resource.pipeline_layout = GetOrCreatePipelineLayout(state, desc.layout);

    const std::size_t hash = HashGraphicsPipeline(desc);
    const auto it = state.graphics_pipeline_cache.find(hash);
    if (it != state.graphics_pipeline_cache.end()) {
      resource.pipeline = it->second;
      return;
    }

    const VulkanShaderResource* vertex_shader = state.shaders.Resolve(desc.vertex_shader);
    const VulkanShaderResource* fragment_shader =
        state.shaders.Resolve(desc.fragment_shader);
    const VulkanRenderPassResource* render_pass =
        state.render_passes.Resolve(desc.render_pass);
    if (vertex_shader == nullptr || fragment_shader == nullptr || render_pass == nullptr ||
        resource.pipeline_layout == VK_NULL_HANDLE) {
      return;
    }

    std::array<VkPipelineShaderStageCreateInfo, 2> stages{
        BuildStage(*vertex_shader), BuildStage(*fragment_shader)};

    std::vector<VkVertexInputBindingDescription> bindings(desc.vertex_input.binding_count);
    for (std::uint32_t index = 0; index < desc.vertex_input.binding_count; ++index) {
      bindings[index].binding = desc.vertex_input.bindings[index].binding;
      bindings[index].stride = desc.vertex_input.bindings[index].stride;
      bindings[index].inputRate =
          desc.vertex_input.bindings[index].input_rate == ERHIVertexInputRate::PerInstance
              ? VK_VERTEX_INPUT_RATE_INSTANCE
              : VK_VERTEX_INPUT_RATE_VERTEX;
    }

    std::vector<VkVertexInputAttributeDescription> attributes(
        desc.vertex_input.attribute_count);
    for (std::uint32_t index = 0; index < desc.vertex_input.attribute_count; ++index) {
      attributes[index].location = desc.vertex_input.attributes[index].location;
      attributes[index].binding = desc.vertex_input.attributes[index].binding;
      attributes[index].format = ToVkFormat(desc.vertex_input.attributes[index].format);
      attributes[index].offset = desc.vertex_input.attributes[index].offset;
    }

    VkPipelineVertexInputStateCreateInfo vertex_input{
        VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO};
    vertex_input.vertexBindingDescriptionCount = static_cast<std::uint32_t>(bindings.size());
    vertex_input.pVertexBindingDescriptions = bindings.empty() ? nullptr : bindings.data();
    vertex_input.vertexAttributeDescriptionCount =
        static_cast<std::uint32_t>(attributes.size());
    vertex_input.pVertexAttributeDescriptions =
        attributes.empty() ? nullptr : attributes.data();

    VkPipelineInputAssemblyStateCreateInfo input_assembly{
        VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO};
    input_assembly.topology = ToVkPrimitiveTopology(desc.topology);

    VkPipelineViewportStateCreateInfo viewport_state{
        VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO};
    viewport_state.viewportCount = 1;
    viewport_state.scissorCount = 1;

    std::array<VkDynamicState, 2> dynamic_states{VK_DYNAMIC_STATE_VIEWPORT,
                                                 VK_DYNAMIC_STATE_SCISSOR};
    VkPipelineDynamicStateCreateInfo dynamic_state{
        VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO};
    dynamic_state.dynamicStateCount = static_cast<std::uint32_t>(dynamic_states.size());
    dynamic_state.pDynamicStates = dynamic_states.data();

    VkPipelineRasterizationStateCreateInfo rasterization{
        VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO};
    rasterization.depthClampEnable = desc.rasterizer.depth_clamp_enable ? VK_TRUE : VK_FALSE;
    rasterization.depthBiasEnable = desc.rasterizer.depth_bias_enable ? VK_TRUE : VK_FALSE;
    rasterization.polygonMode = ToVkPolygonMode(desc.rasterizer.polygon_mode);
    rasterization.cullMode = ToVkCullMode(desc.rasterizer.cull_mode);
    rasterization.frontFace = ToVkFrontFace(desc.rasterizer.front_face);
    rasterization.depthBiasConstantFactor = desc.rasterizer.depth_bias_constant;
    rasterization.depthBiasClamp = desc.rasterizer.depth_bias_clamp;
    rasterization.depthBiasSlopeFactor = desc.rasterizer.depth_bias_slope;
    rasterization.lineWidth = desc.rasterizer.line_width;

    VkPipelineMultisampleStateCreateInfo multisample{
        VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO};
    multisample.rasterizationSamples = ToVkSampleCount(desc.multisample.sample_count);
    multisample.sampleShadingEnable =
        desc.multisample.sample_shading_enable ? VK_TRUE : VK_FALSE;
    multisample.minSampleShading = desc.multisample.min_sample_shading;

    std::vector<VkPipelineColorBlendAttachmentState> blend_attachments(
        desc.blend.attachment_count);
    for (std::uint32_t index = 0; index < desc.blend.attachment_count; ++index) {
      const auto& attachment_desc = desc.blend.attachments[index];
      blend_attachments[index].blendEnable = attachment_desc.blend_enable ? VK_TRUE : VK_FALSE;
      blend_attachments[index].srcColorBlendFactor =
          ToVkBlendFactor(attachment_desc.src_color_factor);
      blend_attachments[index].dstColorBlendFactor =
          ToVkBlendFactor(attachment_desc.dst_color_factor);
      blend_attachments[index].colorBlendOp = ToVkBlendOp(attachment_desc.color_op);
      blend_attachments[index].srcAlphaBlendFactor =
          ToVkBlendFactor(attachment_desc.src_alpha_factor);
      blend_attachments[index].dstAlphaBlendFactor =
          ToVkBlendFactor(attachment_desc.dst_alpha_factor);
      blend_attachments[index].alphaBlendOp = ToVkBlendOp(attachment_desc.alpha_op);
      blend_attachments[index].colorWriteMask =
          ToVkColorComponentMask(attachment_desc.write_mask);
    }

    VkPipelineColorBlendStateCreateInfo color_blend{
        VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO};
    color_blend.attachmentCount = static_cast<std::uint32_t>(blend_attachments.size());
    color_blend.pAttachments = blend_attachments.empty() ? nullptr : blend_attachments.data();
    for (std::size_t index = 0; index < 4; ++index) {
      color_blend.blendConstants[index] = desc.blend.blend_constants[index];
    }

    VkStencilOpState front_stencil{};
    front_stencil.failOp = ToVkStencilOp(desc.depth_stencil.front.fail_op);
    front_stencil.passOp = ToVkStencilOp(desc.depth_stencil.front.pass_op);
    front_stencil.depthFailOp = ToVkStencilOp(desc.depth_stencil.front.depth_fail_op);
    front_stencil.compareOp = ToVkCompareOp(desc.depth_stencil.front.compare_op);
    front_stencil.compareMask = desc.depth_stencil.front.compare_mask;
    front_stencil.writeMask = desc.depth_stencil.front.write_mask;
    front_stencil.reference = desc.depth_stencil.front.reference;

    VkStencilOpState back_stencil{};
    back_stencil.failOp = ToVkStencilOp(desc.depth_stencil.back.fail_op);
    back_stencil.passOp = ToVkStencilOp(desc.depth_stencil.back.pass_op);
    back_stencil.depthFailOp = ToVkStencilOp(desc.depth_stencil.back.depth_fail_op);
    back_stencil.compareOp = ToVkCompareOp(desc.depth_stencil.back.compare_op);
    back_stencil.compareMask = desc.depth_stencil.back.compare_mask;
    back_stencil.writeMask = desc.depth_stencil.back.write_mask;
    back_stencil.reference = desc.depth_stencil.back.reference;

    VkPipelineDepthStencilStateCreateInfo depth_stencil{
        VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO};
    depth_stencil.depthTestEnable = desc.depth_stencil.depth_test_enable ? VK_TRUE : VK_FALSE;
    depth_stencil.depthWriteEnable = desc.depth_stencil.depth_write_enable ? VK_TRUE : VK_FALSE;
    depth_stencil.depthCompareOp = ToVkCompareOp(desc.depth_stencil.depth_compare_op);
    depth_stencil.depthBoundsTestEnable =
        desc.depth_stencil.depth_bounds_test_enable ? VK_TRUE : VK_FALSE;
    depth_stencil.stencilTestEnable = desc.depth_stencil.stencil_test_enable ? VK_TRUE : VK_FALSE;
    depth_stencil.front = front_stencil;
    depth_stencil.back = back_stencil;

    std::vector<VkFormat> color_formats(render_pass->desc.color_attachment_count);
    for (std::uint32_t index = 0; index < render_pass->desc.color_attachment_count; ++index) {
      color_formats[index] = ToVkFormat(render_pass->desc.color_attachments[index].format);
    }

    VkPipelineRenderingCreateInfo rendering_info{
        VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO};
    rendering_info.colorAttachmentCount =
        static_cast<std::uint32_t>(color_formats.size());
    rendering_info.pColorAttachmentFormats =
        color_formats.empty() ? nullptr : color_formats.data();
    if (render_pass->desc.has_depth_stencil_attachment) {
      rendering_info.depthAttachmentFormat =
          ToVkFormat(render_pass->desc.depth_stencil_attachment.format);
      rendering_info.stencilAttachmentFormat =
          HasStencilComponent(render_pass->desc.depth_stencil_attachment.format)
              ? ToVkFormat(render_pass->desc.depth_stencil_attachment.format)
              : VK_FORMAT_UNDEFINED;
    }

    VkGraphicsPipelineCreateInfo create_info{
        VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO};
    create_info.pNext = &rendering_info;
    create_info.stageCount = static_cast<std::uint32_t>(stages.size());
    create_info.pStages = stages.data();
    create_info.pVertexInputState = &vertex_input;
    create_info.pInputAssemblyState = &input_assembly;
    create_info.pViewportState = &viewport_state;
    create_info.pRasterizationState = &rasterization;
    create_info.pMultisampleState = &multisample;
    create_info.pDepthStencilState = &depth_stencil;
    create_info.pColorBlendState = &color_blend;
    create_info.pDynamicState = &dynamic_state;
    create_info.layout = resource.pipeline_layout;

    vkCreateGraphicsPipelines(state.device, state.native_pipeline_cache, 1, &create_info,
                              nullptr, &resource.pipeline);
    if (resource.pipeline != VK_NULL_HANDLE) {
      state.graphics_pipeline_cache.emplace(hash, resource.pipeline);
    }
  });

  if (!handle.IsValid()) {
    return {.result = RHIResult::ErrorOutOfMemory};
  }

  const VulkanPipelineResource* resource = state.pipelines.Resolve(handle);
  if (resource == nullptr || resource->pipeline == VK_NULL_HANDLE ||
      resource->pipeline_layout == VK_NULL_HANDLE) {
    state.pipelines.Destroy(handle, [&](VulkanPipelineResource& failed_resource) {
      DestroyVulkanPipeline(state, failed_resource);
    });
    return {.result = RHIResult::ErrorValidation};
  }

  return {.handle = handle, .result = RHIResult::Success};
}

RHIPipelineCreateResult CreateVulkanComputePipeline(VulkanContextState& state,
                                                    const RHIComputePipelineDesc& desc) {
  const RHIPipelineHandle handle = state.pipelines.Allocate([&](VulkanPipelineResource& resource) {
    resource.type = ERHIPipelineType::Compute;
    resource.compute_desc = desc;
    resource.pipeline_layout = GetOrCreatePipelineLayout(state, desc.layout);

    const std::size_t hash = HashComputePipeline(desc);
    const auto it = state.compute_pipeline_cache.find(hash);
    if (it != state.compute_pipeline_cache.end()) {
      resource.pipeline = it->second;
      return;
    }

    const VulkanShaderResource* shader = state.shaders.Resolve(desc.compute_shader);
    if (shader == nullptr || resource.pipeline_layout == VK_NULL_HANDLE) {
      return;
    }

    const VkPipelineShaderStageCreateInfo stage = BuildStage(*shader);
    VkComputePipelineCreateInfo create_info{
        VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO};
    create_info.stage = stage;
    create_info.layout = resource.pipeline_layout;
    vkCreateComputePipelines(state.device, state.native_pipeline_cache, 1, &create_info,
                             nullptr, &resource.pipeline);
    if (resource.pipeline != VK_NULL_HANDLE) {
      state.compute_pipeline_cache.emplace(hash, resource.pipeline);
    }
  });

  if (!handle.IsValid()) {
    return {.result = RHIResult::ErrorOutOfMemory};
  }

  const VulkanPipelineResource* resource = state.pipelines.Resolve(handle);
  if (resource == nullptr || resource->pipeline == VK_NULL_HANDLE ||
      resource->pipeline_layout == VK_NULL_HANDLE) {
    state.pipelines.Destroy(handle, [&](VulkanPipelineResource& failed_resource) {
      DestroyVulkanPipeline(state, failed_resource);
    });
    return {.result = RHIResult::ErrorValidation};
  }

  return {.handle = handle, .result = RHIResult::Success};
}

void DestroyVulkanSampler(VulkanContextState& state,
                          VulkanSamplerResource& resource) {
  if (resource.sampler != VK_NULL_HANDLE) {
    vkDestroySampler(state.device, resource.sampler, nullptr);
  }
  resource = VulkanSamplerResource{};
}

void DestroyVulkanShader(VulkanContextState& state, VulkanShaderResource& resource) {
  if (resource.module != VK_NULL_HANDLE) {
    vkDestroyShaderModule(state.device, resource.module, nullptr);
  }
  resource = VulkanShaderResource{};
}

void DestroyVulkanPipeline(VulkanContextState&, VulkanPipelineResource& resource) {
  resource = VulkanPipelineResource{};
}

void DestroyVulkanFence(VulkanContextState& state, VulkanFenceResource& resource) {
  if (resource.fence != VK_NULL_HANDLE) {
    vkDestroyFence(state.device, resource.fence, nullptr);
  }
  resource = VulkanFenceResource{};
}

void SetVulkanObjectDebugName(VulkanContextState& state,
                              std::uint64_t object_handle,
                              VkObjectType object_type,
                              std::string_view name) {
  if (state.set_debug_name == nullptr || object_handle == 0 || name.empty()) {
    return;
  }

  std::string storage(name);
  VkDebugUtilsObjectNameInfoEXT info{VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT};
  info.objectHandle = object_handle;
  info.objectType = object_type;
  info.pObjectName = storage.c_str();
  state.set_debug_name(state.device, &info);
}

}  // namespace Fabrica::RHI::Vulkan