#pragma once

#include <array>
#include <cstdint>
#include <cstring>
#include <limits>
#include <optional>
#include <string>
#include <string_view>
#include <type_traits>
#include <vector>

#include <Volk/volk.h>

#ifndef VMA_STATIC_VULKAN_FUNCTIONS
#define VMA_STATIC_VULKAN_FUNCTIONS 0
#endif

#ifndef VMA_DYNAMIC_VULKAN_FUNCTIONS
#define VMA_DYNAMIC_VULKAN_FUNCTIONS 1
#endif

#ifndef VMA_VULKAN_VERSION
#define VMA_VULKAN_VERSION 1003000
#endif

#include <vma/vk_mem_alloc.h>

#include "core/common/assert.h"
#include "core/logging/logger.h"
#include "rhi/RHITypes.h"

namespace Fabrica::RHI::Vulkan {

constexpr std::uint32_t kInvalidQueueFamily = std::numeric_limits<std::uint32_t>::max();
constexpr std::uint32_t kMaxSubmittedCommandLists = 64;

#define FABRICA_RHI_VK_CHECK(Expression, Message)                             \
  do {                                                                        \
    const VkResult fabrica_rhi_vk_result = (Expression);                      \
    if (fabrica_rhi_vk_result != VK_SUCCESS) {                                \
      FABRICA_LOG(Render, Error)                                              \
          << "[RHI] Vulkan failure: " << (Message) << " | code="           \
          << static_cast<int>(fabrica_rhi_vk_result);                         \
    }                                                                         \
    FABRICA_ASSERT(fabrica_rhi_vk_result == VK_SUCCESS, Message);             \
  } while (false)

inline RHIResult ToRHIResult(VkResult result) {
  switch (result) {
    case VK_SUCCESS:
      return RHIResult::Success;
    case VK_ERROR_OUT_OF_HOST_MEMORY:
    case VK_ERROR_OUT_OF_DEVICE_MEMORY:
      return RHIResult::ErrorOutOfMemory;
    case VK_ERROR_DEVICE_LOST:
      return RHIResult::ErrorDeviceLost;
    case VK_ERROR_OUT_OF_DATE_KHR:
    case VK_SUBOPTIMAL_KHR:
      return RHIResult::ErrorSwapchainOutOfDate;
    case VK_TIMEOUT:
      return RHIResult::ErrorTimeout;
    default:
      return RHIResult::ErrorValidation;
  }
}

inline bool IsDepthFormat(ERHITextureFormat format) {
  return format == ERHITextureFormat::D16_UNORM ||
         format == ERHITextureFormat::D24_UNORM_S8_UINT ||
         format == ERHITextureFormat::D32_SFLOAT;
}

inline bool HasStencilComponent(ERHITextureFormat format) {
  return format == ERHITextureFormat::D24_UNORM_S8_UINT;
}

inline VkFormat ToVkFormat(ERHITextureFormat format) {
  switch (format) {
    case ERHITextureFormat::R8_UNORM:
      return VK_FORMAT_R8_UNORM;
    case ERHITextureFormat::R8G8_UNORM:
      return VK_FORMAT_R8G8_UNORM;
    case ERHITextureFormat::R8G8B8A8_UNORM:
      return VK_FORMAT_R8G8B8A8_UNORM;
    case ERHITextureFormat::B8G8R8A8_UNORM:
      return VK_FORMAT_B8G8R8A8_UNORM;
    case ERHITextureFormat::R16G16B16A16_FLOAT:
      return VK_FORMAT_R16G16B16A16_SFLOAT;
    case ERHITextureFormat::R32_FLOAT:
      return VK_FORMAT_R32_SFLOAT;
    case ERHITextureFormat::R32G32_FLOAT:
      return VK_FORMAT_R32G32_SFLOAT;
    case ERHITextureFormat::R32G32B32_FLOAT:
      return VK_FORMAT_R32G32B32_SFLOAT;
    case ERHITextureFormat::R32G32B32A32_FLOAT:
      return VK_FORMAT_R32G32B32A32_SFLOAT;
    case ERHITextureFormat::D16_UNORM:
      return VK_FORMAT_D16_UNORM;
    case ERHITextureFormat::D24_UNORM_S8_UINT:
      return VK_FORMAT_D24_UNORM_S8_UINT;
    case ERHITextureFormat::D32_SFLOAT:
      return VK_FORMAT_D32_SFLOAT;
    case ERHITextureFormat::BC1_RGBA_UNORM:
      return VK_FORMAT_BC1_RGBA_UNORM_BLOCK;
    case ERHITextureFormat::BC3_UNORM:
      return VK_FORMAT_BC3_UNORM_BLOCK;
    case ERHITextureFormat::BC5_UNORM:
      return VK_FORMAT_BC5_UNORM_BLOCK;
    case ERHITextureFormat::BC7_UNORM:
      return VK_FORMAT_BC7_UNORM_BLOCK;
    case ERHITextureFormat::Undefined:
    default:
      return VK_FORMAT_UNDEFINED;
  }
}

inline VkBufferUsageFlags ToVkBufferUsage(ERHIBufferUsage usage) {
  VkBufferUsageFlags flags = 0;
  if (HasAnyFlag(usage, ERHIBufferUsage::VertexBuffer)) {
    flags |= VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
  }
  if (HasAnyFlag(usage, ERHIBufferUsage::IndexBuffer)) {
    flags |= VK_BUFFER_USAGE_INDEX_BUFFER_BIT;
  }
  if (HasAnyFlag(usage, ERHIBufferUsage::UniformBuffer)) {
    flags |= VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
  }
  if (HasAnyFlag(usage, ERHIBufferUsage::StorageBuffer)) {
    flags |= VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
  }
  if (HasAnyFlag(usage, ERHIBufferUsage::IndirectBuffer)) {
    flags |= VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT;
  }
  if (HasAnyFlag(usage, ERHIBufferUsage::TransferSrc)) {
    flags |= VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
  }
  if (HasAnyFlag(usage, ERHIBufferUsage::TransferDst)) {
    flags |= VK_BUFFER_USAGE_TRANSFER_DST_BIT;
  }
  return flags;
}

inline VkImageUsageFlags ToVkImageUsage(ERHITextureUsage usage) {
  VkImageUsageFlags flags = 0;
  if (HasAnyFlag(usage, ERHITextureUsage::Sampled)) {
    flags |= VK_IMAGE_USAGE_SAMPLED_BIT;
  }
  if (HasAnyFlag(usage, ERHITextureUsage::ColorAttachment)) {
    flags |= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
  }
  if (HasAnyFlag(usage, ERHITextureUsage::DepthStencilAttachment)) {
    flags |= VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
  }
  if (HasAnyFlag(usage, ERHITextureUsage::Storage)) {
    flags |= VK_IMAGE_USAGE_STORAGE_BIT;
  }
  if (HasAnyFlag(usage, ERHITextureUsage::TransferSrc)) {
    flags |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
  }
  if (HasAnyFlag(usage, ERHITextureUsage::TransferDst)) {
    flags |= VK_IMAGE_USAGE_TRANSFER_DST_BIT;
  }
  return flags;
}

inline VkImageAspectFlags ToVkImageAspect(ERHITextureAspectFlags aspect) {
  VkImageAspectFlags flags = 0;
  if (HasAnyFlag(aspect, ERHITextureAspectFlags::Color)) {
    flags |= VK_IMAGE_ASPECT_COLOR_BIT;
  }
  if (HasAnyFlag(aspect, ERHITextureAspectFlags::Depth)) {
    flags |= VK_IMAGE_ASPECT_DEPTH_BIT;
  }
  if (HasAnyFlag(aspect, ERHITextureAspectFlags::Stencil)) {
    flags |= VK_IMAGE_ASPECT_STENCIL_BIT;
  }
  return flags;
}

inline VkImageLayout ToVkImageLayout(ERHITextureLayout layout) {
  switch (layout) {
    case ERHITextureLayout::General:
      return VK_IMAGE_LAYOUT_GENERAL;
    case ERHITextureLayout::ColorAttachment:
      return VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    case ERHITextureLayout::DepthStencil:
      return VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
    case ERHITextureLayout::ShaderReadOnly:
      return VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    case ERHITextureLayout::TransferSrc:
      return VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
    case ERHITextureLayout::TransferDst:
      return VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    case ERHITextureLayout::Present:
      return VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
    case ERHITextureLayout::Undefined:
    default:
      return VK_IMAGE_LAYOUT_UNDEFINED;
  }
}

inline VkShaderStageFlags ToVkShaderStage(ERHIShaderStage stage) {
  VkShaderStageFlags flags = 0;
  if (HasAnyFlag(stage, ERHIShaderStage::Vertex)) {
    flags |= VK_SHADER_STAGE_VERTEX_BIT;
  }
  if (HasAnyFlag(stage, ERHIShaderStage::Fragment)) {
    flags |= VK_SHADER_STAGE_FRAGMENT_BIT;
  }
  if (HasAnyFlag(stage, ERHIShaderStage::Compute)) {
    flags |= VK_SHADER_STAGE_COMPUTE_BIT;
  }
  if (HasAnyFlag(stage, ERHIShaderStage::Geometry)) {
    flags |= VK_SHADER_STAGE_GEOMETRY_BIT;
  }
  if (HasAnyFlag(stage, ERHIShaderStage::TessCtrl)) {
    flags |= VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT;
  }
  if (HasAnyFlag(stage, ERHIShaderStage::TessEval)) {
    flags |= VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT;
  }
  return flags;
}

inline VkPipelineStageFlags2 ToVkPipelineStage(ERHIPipelineStage stage) {
  if (stage == ERHIPipelineStage::None) {
    return VK_PIPELINE_STAGE_2_NONE;
  }

  VkPipelineStageFlags2 flags = 0;
  if (HasAnyFlag(stage, ERHIPipelineStage::TopOfPipe)) {
    flags |= VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT;
  }
  if (HasAnyFlag(stage, ERHIPipelineStage::DrawIndirect)) {
    flags |= VK_PIPELINE_STAGE_2_DRAW_INDIRECT_BIT;
  }
  if (HasAnyFlag(stage, ERHIPipelineStage::VertexInput)) {
    flags |= VK_PIPELINE_STAGE_2_VERTEX_INPUT_BIT;
  }
  if (HasAnyFlag(stage, ERHIPipelineStage::VertexShader)) {
    flags |= VK_PIPELINE_STAGE_2_VERTEX_SHADER_BIT;
  }
  if (HasAnyFlag(stage, ERHIPipelineStage::FragmentShader)) {
    flags |= VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT;
  }
  if (HasAnyFlag(stage, ERHIPipelineStage::ComputeShader)) {
    flags |= VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT;
  }
  if (HasAnyFlag(stage, ERHIPipelineStage::ColorAttachmentOutput)) {
    flags |= VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;
  }
  if (HasAnyFlag(stage, ERHIPipelineStage::EarlyFragmentTests)) {
    flags |= VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT;
  }
  if (HasAnyFlag(stage, ERHIPipelineStage::LateFragmentTests)) {
    flags |= VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT;
  }
  if (HasAnyFlag(stage, ERHIPipelineStage::Transfer)) {
    flags |= VK_PIPELINE_STAGE_2_TRANSFER_BIT;
  }
  if (HasAnyFlag(stage, ERHIPipelineStage::BottomOfPipe)) {
    flags |= VK_PIPELINE_STAGE_2_BOTTOM_OF_PIPE_BIT;
  }
  if (HasAnyFlag(stage, ERHIPipelineStage::Host)) {
    flags |= VK_PIPELINE_STAGE_2_HOST_BIT;
  }
  if (HasAnyFlag(stage, ERHIPipelineStage::AllGraphics)) {
    flags |= VK_PIPELINE_STAGE_2_ALL_GRAPHICS_BIT;
  }
  if (HasAnyFlag(stage, ERHIPipelineStage::AllCommands)) {
    flags |= VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
  }
  return flags;
}

inline VkAccessFlags2 ToVkAccessFlags(ERHIAccessFlags access) {
  if (access == ERHIAccessFlags::None) {
    return VK_ACCESS_2_NONE;
  }

  VkAccessFlags2 flags = 0;
  if (HasAnyFlag(access, ERHIAccessFlags::IndirectCommandRead)) {
    flags |= VK_ACCESS_2_INDIRECT_COMMAND_READ_BIT;
  }
  if (HasAnyFlag(access, ERHIAccessFlags::IndexRead)) {
    flags |= VK_ACCESS_2_INDEX_READ_BIT;
  }
  if (HasAnyFlag(access, ERHIAccessFlags::VertexAttributeRead)) {
    flags |= VK_ACCESS_2_VERTEX_ATTRIBUTE_READ_BIT;
  }
  if (HasAnyFlag(access, ERHIAccessFlags::UniformRead)) {
    flags |= VK_ACCESS_2_UNIFORM_READ_BIT;
  }
  if (HasAnyFlag(access, ERHIAccessFlags::ShaderRead)) {
    flags |= VK_ACCESS_2_SHADER_READ_BIT;
  }
  if (HasAnyFlag(access, ERHIAccessFlags::ShaderWrite)) {
    flags |= VK_ACCESS_2_SHADER_WRITE_BIT;
  }
  if (HasAnyFlag(access, ERHIAccessFlags::ColorRead)) {
    flags |= VK_ACCESS_2_COLOR_ATTACHMENT_READ_BIT;
  }
  if (HasAnyFlag(access, ERHIAccessFlags::ColorWrite)) {
    flags |= VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT;
  }
  if (HasAnyFlag(access, ERHIAccessFlags::DepthStencilRead)) {
    flags |= VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_READ_BIT;
  }
  if (HasAnyFlag(access, ERHIAccessFlags::DepthWrite)) {
    flags |= VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
  }
  if (HasAnyFlag(access, ERHIAccessFlags::TransferRead)) {
    flags |= VK_ACCESS_2_TRANSFER_READ_BIT;
  }
  if (HasAnyFlag(access, ERHIAccessFlags::TransferWrite)) {
    flags |= VK_ACCESS_2_TRANSFER_WRITE_BIT;
  }
  if (HasAnyFlag(access, ERHIAccessFlags::HostRead)) {
    flags |= VK_ACCESS_2_HOST_READ_BIT;
  }
  if (HasAnyFlag(access, ERHIAccessFlags::HostWrite)) {
    flags |= VK_ACCESS_2_HOST_WRITE_BIT;
  }
  if (HasAnyFlag(access, ERHIAccessFlags::MemoryRead)) {
    flags |= VK_ACCESS_2_MEMORY_READ_BIT;
  }
  if (HasAnyFlag(access, ERHIAccessFlags::MemoryWrite)) {
    flags |= VK_ACCESS_2_MEMORY_WRITE_BIT;
  }
  return flags;
}

inline VkDescriptorType ToVkDescriptorType(ERHIDescriptorType type) {
  switch (type) {
    case ERHIDescriptorType::UniformBuffer:
      return VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    case ERHIDescriptorType::StorageBuffer:
      return VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    case ERHIDescriptorType::SampledTexture:
      return VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
    case ERHIDescriptorType::StorageTexture:
      return VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    case ERHIDescriptorType::Sampler:
      return VK_DESCRIPTOR_TYPE_SAMPLER;
    case ERHIDescriptorType::CombinedImageSampler:
      return VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    default:
      return VK_DESCRIPTOR_TYPE_MAX_ENUM;
  }
}

inline VkSampleCountFlagBits ToVkSampleCount(ERHISampleCount samples) {
  switch (samples) {
    case ERHISampleCount::Sample2:
      return VK_SAMPLE_COUNT_2_BIT;
    case ERHISampleCount::Sample4:
      return VK_SAMPLE_COUNT_4_BIT;
    case ERHISampleCount::Sample8:
      return VK_SAMPLE_COUNT_8_BIT;
    case ERHISampleCount::Sample1:
    default:
      return VK_SAMPLE_COUNT_1_BIT;
  }
}

inline VkPrimitiveTopology ToVkPrimitiveTopology(ERHIPrimitiveTopology topology) {
  switch (topology) {
    case ERHIPrimitiveTopology::TriangleStrip:
      return VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;
    case ERHIPrimitiveTopology::LineList:
      return VK_PRIMITIVE_TOPOLOGY_LINE_LIST;
    case ERHIPrimitiveTopology::LineStrip:
      return VK_PRIMITIVE_TOPOLOGY_LINE_STRIP;
    case ERHIPrimitiveTopology::PointList:
      return VK_PRIMITIVE_TOPOLOGY_POINT_LIST;
    case ERHIPrimitiveTopology::TriangleList:
    default:
      return VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
  }
}

inline VkPolygonMode ToVkPolygonMode(ERHIPolygonMode mode) {
  switch (mode) {
    case ERHIPolygonMode::Line:
      return VK_POLYGON_MODE_LINE;
    case ERHIPolygonMode::Point:
      return VK_POLYGON_MODE_POINT;
    case ERHIPolygonMode::Fill:
    default:
      return VK_POLYGON_MODE_FILL;
  }
}

inline VkCullModeFlags ToVkCullMode(ERHICullMode mode) {
  switch (mode) {
    case ERHICullMode::Front:
      return VK_CULL_MODE_FRONT_BIT;
    case ERHICullMode::Back:
      return VK_CULL_MODE_BACK_BIT;
    case ERHICullMode::FrontAndBack:
      return VK_CULL_MODE_FRONT_AND_BACK;
    case ERHICullMode::None:
    default:
      return VK_CULL_MODE_NONE;
  }
}

inline VkFrontFace ToVkFrontFace(ERHIFrontFace face) {
  return face == ERHIFrontFace::Clockwise ? VK_FRONT_FACE_CLOCKWISE
                                          : VK_FRONT_FACE_COUNTER_CLOCKWISE;
}

inline VkCompareOp ToVkCompareOp(ERHICompareOp op) {
  switch (op) {
    case ERHICompareOp::Never:
      return VK_COMPARE_OP_NEVER;
    case ERHICompareOp::Less:
      return VK_COMPARE_OP_LESS;
    case ERHICompareOp::Equal:
      return VK_COMPARE_OP_EQUAL;
    case ERHICompareOp::LessOrEqual:
      return VK_COMPARE_OP_LESS_OR_EQUAL;
    case ERHICompareOp::Greater:
      return VK_COMPARE_OP_GREATER;
    case ERHICompareOp::NotEqual:
      return VK_COMPARE_OP_NOT_EQUAL;
    case ERHICompareOp::GreaterOrEqual:
      return VK_COMPARE_OP_GREATER_OR_EQUAL;
    case ERHICompareOp::Always:
    default:
      return VK_COMPARE_OP_ALWAYS;
  }
}

inline VkStencilOp ToVkStencilOp(ERHIStencilOp op) {
  switch (op) {
    case ERHIStencilOp::Zero:
      return VK_STENCIL_OP_ZERO;
    case ERHIStencilOp::Replace:
      return VK_STENCIL_OP_REPLACE;
    case ERHIStencilOp::IncrementClamp:
      return VK_STENCIL_OP_INCREMENT_AND_CLAMP;
    case ERHIStencilOp::DecrementClamp:
      return VK_STENCIL_OP_DECREMENT_AND_CLAMP;
    case ERHIStencilOp::Invert:
      return VK_STENCIL_OP_INVERT;
    case ERHIStencilOp::IncrementWrap:
      return VK_STENCIL_OP_INCREMENT_AND_WRAP;
    case ERHIStencilOp::DecrementWrap:
      return VK_STENCIL_OP_DECREMENT_AND_WRAP;
    case ERHIStencilOp::Keep:
    default:
      return VK_STENCIL_OP_KEEP;
  }
}

inline VkBlendFactor ToVkBlendFactor(ERHIBlendFactor factor) {
  switch (factor) {
    case ERHIBlendFactor::Zero:
      return VK_BLEND_FACTOR_ZERO;
    case ERHIBlendFactor::One:
      return VK_BLEND_FACTOR_ONE;
    case ERHIBlendFactor::SrcColor:
      return VK_BLEND_FACTOR_SRC_COLOR;
    case ERHIBlendFactor::OneMinusSrcColor:
      return VK_BLEND_FACTOR_ONE_MINUS_SRC_COLOR;
    case ERHIBlendFactor::DstColor:
      return VK_BLEND_FACTOR_DST_COLOR;
    case ERHIBlendFactor::OneMinusDstColor:
      return VK_BLEND_FACTOR_ONE_MINUS_DST_COLOR;
    case ERHIBlendFactor::SrcAlpha:
      return VK_BLEND_FACTOR_SRC_ALPHA;
    case ERHIBlendFactor::OneMinusSrcAlpha:
      return VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    case ERHIBlendFactor::DstAlpha:
      return VK_BLEND_FACTOR_DST_ALPHA;
    case ERHIBlendFactor::OneMinusDstAlpha:
    default:
      return VK_BLEND_FACTOR_ONE_MINUS_DST_ALPHA;
  }
}

inline VkBlendOp ToVkBlendOp(ERHIBlendOp op) {
  switch (op) {
    case ERHIBlendOp::Subtract:
      return VK_BLEND_OP_SUBTRACT;
    case ERHIBlendOp::ReverseSubtract:
      return VK_BLEND_OP_REVERSE_SUBTRACT;
    case ERHIBlendOp::Min:
      return VK_BLEND_OP_MIN;
    case ERHIBlendOp::Max:
      return VK_BLEND_OP_MAX;
    case ERHIBlendOp::Add:
    default:
      return VK_BLEND_OP_ADD;
  }
}

inline VkColorComponentFlags ToVkColorComponentMask(ERHIColorComponent mask) {
  VkColorComponentFlags flags = 0;
  if (HasAnyFlag(mask, ERHIColorComponent::R)) {
    flags |= VK_COLOR_COMPONENT_R_BIT;
  }
  if (HasAnyFlag(mask, ERHIColorComponent::G)) {
    flags |= VK_COLOR_COMPONENT_G_BIT;
  }
  if (HasAnyFlag(mask, ERHIColorComponent::B)) {
    flags |= VK_COLOR_COMPONENT_B_BIT;
  }
  if (HasAnyFlag(mask, ERHIColorComponent::A)) {
    flags |= VK_COLOR_COMPONENT_A_BIT;
  }
  return flags;
}

inline VkFilter ToVkFilter(ERHIFilter filter) {
  return filter == ERHIFilter::Nearest ? VK_FILTER_NEAREST : VK_FILTER_LINEAR;
}

inline VkSamplerMipmapMode ToVkMipFilter(ERHIFilter filter) {
  return filter == ERHIFilter::Nearest ? VK_SAMPLER_MIPMAP_MODE_NEAREST
                                       : VK_SAMPLER_MIPMAP_MODE_LINEAR;
}

inline VkSamplerAddressMode ToVkAddressMode(ERHISamplerAddressMode mode) {
  switch (mode) {
    case ERHISamplerAddressMode::MirroredRepeat:
      return VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT;
    case ERHISamplerAddressMode::ClampToEdge:
      return VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    case ERHISamplerAddressMode::ClampToBorder:
      return VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
    case ERHISamplerAddressMode::Repeat:
    default:
      return VK_SAMPLER_ADDRESS_MODE_REPEAT;
  }
}

inline VkBorderColor ToVkBorderColor(ERHIBorderColor color) {
  switch (color) {
    case ERHIBorderColor::IntTransparentBlack:
      return VK_BORDER_COLOR_INT_TRANSPARENT_BLACK;
    case ERHIBorderColor::FloatOpaqueBlack:
      return VK_BORDER_COLOR_FLOAT_OPAQUE_BLACK;
    case ERHIBorderColor::IntOpaqueBlack:
      return VK_BORDER_COLOR_INT_OPAQUE_BLACK;
    case ERHIBorderColor::FloatOpaqueWhite:
      return VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;
    case ERHIBorderColor::IntOpaqueWhite:
      return VK_BORDER_COLOR_INT_OPAQUE_WHITE;
    case ERHIBorderColor::FloatTransparentBlack:
    default:
      return VK_BORDER_COLOR_FLOAT_TRANSPARENT_BLACK;
  }
}

inline VmaMemoryUsage ToVmaUsage(ERHIMemoryType memory) {
  switch (memory) {
    case ERHIMemoryType::HostVisible:
    case ERHIMemoryType::HostCoherent:
      return VMA_MEMORY_USAGE_AUTO_PREFER_HOST;
    case ERHIMemoryType::Staging:
      return VMA_MEMORY_USAGE_AUTO_PREFER_HOST;
    case ERHIMemoryType::DeviceLocal:
    default:
      return VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;
  }
}

inline VkIndexType ToVkIndexType(ERHIIndexType type) {
  return type == ERHIIndexType::Uint16 ? VK_INDEX_TYPE_UINT16
                                       : VK_INDEX_TYPE_UINT32;
}

inline VkCommandBufferLevel ToVkCommandBufferLevel() {
  return VK_COMMAND_BUFFER_LEVEL_PRIMARY;
}

template <typename HandleT>
inline std::uint64_t ToDebugHandleValue(HandleT handle) {
  if constexpr (std::is_pointer_v<HandleT>) {
    return static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(handle));
  } else {
    return static_cast<std::uint64_t>(handle);
  }
}

}  // namespace Fabrica::RHI::Vulkan




