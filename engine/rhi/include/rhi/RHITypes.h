#pragma once

#include <cstddef>
#include <cstdint>
#include <type_traits>

namespace Fabrica::RHI {

constexpr std::uint32_t kFramesInFlight = 3;
constexpr std::uint32_t kMaxVertexBindings = 8;
constexpr std::uint32_t kMaxVertexAttributes = 16;
constexpr std::uint32_t kMaxColorAttachments = 8;
constexpr std::uint32_t kMaxDescriptorBindings = 32;
constexpr std::uint32_t kMaxDescriptorSets = 8;
constexpr std::uint32_t kMaxPushConstantRanges = 4;
constexpr std::uint32_t kMaxAttachmentLayers = 8;
constexpr std::uint32_t kMaxShaderEntryPointLength = 64;
constexpr std::uint32_t kMaxApplicationNameLength = 64;
constexpr std::uint32_t kMaxDeviceNameLength = 256;

constexpr std::uint64_t kRHIHandleIndexMask = (1ull << 40ull) - 1ull;
constexpr std::uint32_t kRHIHandleTypeShift = 40u;
constexpr std::uint32_t kRHIHandleGenerationShift = 48u;

#define FABRICA_RHI_DECLARE_HANDLE(Name) \
  struct Name final {                    \
    std::uint64_t id = 0;                \
    constexpr bool IsValid() const { return id != 0; } \
    constexpr explicit operator bool() const { return IsValid(); } \
    constexpr bool operator==(const Name&) const = default; \
  }

FABRICA_RHI_DECLARE_HANDLE(RHIBufferHandle);
FABRICA_RHI_DECLARE_HANDLE(RHITextureHandle);
FABRICA_RHI_DECLARE_HANDLE(RHISamplerHandle);
FABRICA_RHI_DECLARE_HANDLE(RHIShaderHandle);
FABRICA_RHI_DECLARE_HANDLE(RHIRenderPassHandle);
FABRICA_RHI_DECLARE_HANDLE(RHIPipelineHandle);
FABRICA_RHI_DECLARE_HANDLE(RHIDescriptorSetHandle);
FABRICA_RHI_DECLARE_HANDLE(RHIFenceHandle);

#undef FABRICA_RHI_DECLARE_HANDLE

enum class ERHIResourceType : std::uint8_t {
  kUnknown = 0,
  kBuffer = 1,
  kTexture = 2,
  kSampler = 3,
  kShader = 4,
  kRenderPass = 5,
  kPipeline = 6,
  kDescriptorSet = 7,
  kFence = 8,
};

enum class ERHIBackend : std::uint8_t {
  Vulkan = 0,
  D3D12,
  Metal,
  Null,
};

enum class RHIResult : std::uint8_t {
  Success = 0,
  ErrorOutOfMemory,
  ErrorDeviceLost,
  ErrorInvalidHandle,
  ErrorUnsupported,
  ErrorValidation,
  ErrorSwapchainOutOfDate,
  ErrorTimeout,
};

enum class ERHIWindowHandleType : std::uint8_t {
  None = 0,
  GLFW,
  Win32,
};

enum class ERHIQueueType : std::uint8_t {
  Graphics = 0,
  Compute,
  Transfer,
};

enum class ERHIPipelineType : std::uint8_t {
  Graphics = 0,
  Compute,
};

enum class ERHIBufferUsage : std::uint32_t {
  None = 0,
  VertexBuffer = 1u << 0u,
  IndexBuffer = 1u << 1u,
  UniformBuffer = 1u << 2u,
  StorageBuffer = 1u << 3u,
  IndirectBuffer = 1u << 4u,
  TransferSrc = 1u << 5u,
  TransferDst = 1u << 6u,
};

enum class ERHITextureUsage : std::uint32_t {
  None = 0,
  Sampled = 1u << 0u,
  ColorAttachment = 1u << 1u,
  DepthStencilAttachment = 1u << 2u,
  Storage = 1u << 3u,
  TransferSrc = 1u << 4u,
  TransferDst = 1u << 5u,
};

enum class ERHITextureFormat : std::uint16_t {
  Undefined = 0,
  R8_UNORM,
  R8G8_UNORM,
  R8G8B8A8_UNORM,
  B8G8R8A8_UNORM,
  R16G16B16A16_FLOAT,
  R32_FLOAT,
  R32G32_FLOAT,
  R32G32B32_FLOAT,
  R32G32B32A32_FLOAT,
  D16_UNORM,
  D24_UNORM_S8_UINT,
  D32_SFLOAT,
  BC1_RGBA_UNORM,
  BC3_UNORM,
  BC5_UNORM,
  BC7_UNORM,
};

enum class ERHITextureLayout : std::uint8_t {
  Undefined = 0,
  General,
  ColorAttachment,
  DepthStencil,
  ShaderReadOnly,
  TransferSrc,
  TransferDst,
  Present,
};

enum class ERHIMemoryType : std::uint8_t {
  DeviceLocal = 0,
  HostVisible,
  HostCoherent,
  Staging,
};

enum class ERHIPipelineStage : std::uint32_t {
  None = 0,
  TopOfPipe = 1u << 0u,
  DrawIndirect = 1u << 1u,
  VertexInput = 1u << 2u,
  VertexShader = 1u << 3u,
  FragmentShader = 1u << 4u,
  ComputeShader = 1u << 5u,
  ColorAttachmentOutput = 1u << 6u,
  EarlyFragmentTests = 1u << 7u,
  LateFragmentTests = 1u << 8u,
  Transfer = 1u << 9u,
  BottomOfPipe = 1u << 10u,
  Host = 1u << 11u,
  AllGraphics = 1u << 12u,
  AllCommands = 1u << 13u,
};

enum class ERHIAccessFlags : std::uint32_t {
  None = 0,
  IndirectCommandRead = 1u << 0u,
  IndexRead = 1u << 1u,
  VertexAttributeRead = 1u << 2u,
  UniformRead = 1u << 3u,
  ShaderRead = 1u << 4u,
  ShaderWrite = 1u << 5u,
  ColorRead = 1u << 6u,
  ColorWrite = 1u << 7u,
  DepthStencilRead = 1u << 8u,
  DepthWrite = 1u << 9u,
  TransferRead = 1u << 10u,
  TransferWrite = 1u << 11u,
  HostRead = 1u << 12u,
  HostWrite = 1u << 13u,
  MemoryRead = 1u << 14u,
  MemoryWrite = 1u << 15u,
};

enum class ERHIShaderStage : std::uint32_t {
  None = 0,
  Vertex = 1u << 0u,
  Fragment = 1u << 1u,
  Compute = 1u << 2u,
  Geometry = 1u << 3u,
  TessCtrl = 1u << 4u,
  TessEval = 1u << 5u,
  AllGraphics = (1u << 0u) | (1u << 1u) | (1u << 3u) | (1u << 4u) | (1u << 5u),
  All = ((1u << 0u) | (1u << 1u) | (1u << 2u) | (1u << 3u) | (1u << 4u) | (1u << 5u)),
};

enum class ERHIIndexType : std::uint8_t {
  Uint16 = 0,
  Uint32,
};

enum class ERHISampleCount : std::uint8_t {
  Sample1 = 1,
  Sample2 = 2,
  Sample4 = 4,
  Sample8 = 8,
};

enum class ERHIDescriptorType : std::uint8_t {
  UniformBuffer = 0,
  StorageBuffer,
  SampledTexture,
  StorageTexture,
  Sampler,
  CombinedImageSampler,
};

enum class ERHIDescriptorFrequency : std::uint8_t {
  PerFrame = 0,
  PerDraw,
  Persistent,
};

enum class ERHILoadOp : std::uint8_t {
  Load = 0,
  Clear,
  DontCare,
};

enum class ERHIStoreOp : std::uint8_t {
  Store = 0,
  DontCare,
};

enum class ERHIPrimitiveTopology : std::uint8_t {
  TriangleList = 0,
  TriangleStrip,
  LineList,
  LineStrip,
  PointList,
};

enum class ERHIVertexInputRate : std::uint8_t {
  PerVertex = 0,
  PerInstance,
};

enum class ERHIPolygonMode : std::uint8_t {
  Fill = 0,
  Line,
  Point,
};

enum class ERHICullMode : std::uint8_t {
  None = 0,
  Front = 1,
  Back = 2,
  FrontAndBack = 3,
};

enum class ERHIFrontFace : std::uint8_t {
  CounterClockwise = 0,
  Clockwise,
};

enum class ERHICompareOp : std::uint8_t {
  Never = 0,
  Less,
  Equal,
  LessOrEqual,
  Greater,
  NotEqual,
  GreaterOrEqual,
  Always,
};

enum class ERHIStencilOp : std::uint8_t {
  Keep = 0,
  Zero,
  Replace,
  IncrementClamp,
  DecrementClamp,
  Invert,
  IncrementWrap,
  DecrementWrap,
};

enum class ERHIBlendFactor : std::uint8_t {
  Zero = 0,
  One,
  SrcColor,
  OneMinusSrcColor,
  DstColor,
  OneMinusDstColor,
  SrcAlpha,
  OneMinusSrcAlpha,
  DstAlpha,
  OneMinusDstAlpha,
};

enum class ERHIBlendOp : std::uint8_t {
  Add = 0,
  Subtract,
  ReverseSubtract,
  Min,
  Max,
};

enum class ERHIColorComponent : std::uint8_t {
  None = 0,
  R = 1u << 0u,
  G = 1u << 1u,
  B = 1u << 2u,
  A = 1u << 3u,
  All = (1u << 0u) | (1u << 1u) | (1u << 2u) | (1u << 3u),
};

enum class ERHITextureAspectFlags : std::uint8_t {
  None = 0,
  Color = 1u << 0u,
  Depth = 1u << 1u,
  Stencil = 1u << 2u,
  DepthStencil = (1u << 1u) | (1u << 2u),
};

enum class ERHIFilter : std::uint8_t {
  Nearest = 0,
  Linear,
};

enum class ERHISamplerAddressMode : std::uint8_t {
  Repeat = 0,
  MirroredRepeat,
  ClampToEdge,
  ClampToBorder,
};

enum class ERHIBorderColor : std::uint8_t {
  FloatTransparentBlack = 0,
  IntTransparentBlack,
  FloatOpaqueBlack,
  IntOpaqueBlack,
  FloatOpaqueWhite,
  IntOpaqueWhite,
};

template <typename Enum>
struct EnableBitMaskOperators : std::false_type {};

template <typename Enum>
concept BitMaskEnum = EnableBitMaskOperators<Enum>::value;

template <BitMaskEnum Enum>
constexpr Enum operator|(Enum lhs, Enum rhs) {
  using Underlying = std::underlying_type_t<Enum>;
  return static_cast<Enum>(static_cast<Underlying>(lhs) |
                           static_cast<Underlying>(rhs));
}

template <BitMaskEnum Enum>
constexpr Enum operator&(Enum lhs, Enum rhs) {
  using Underlying = std::underlying_type_t<Enum>;
  return static_cast<Enum>(static_cast<Underlying>(lhs) &
                           static_cast<Underlying>(rhs));
}

template <BitMaskEnum Enum>
constexpr Enum& operator|=(Enum& lhs, Enum rhs) {
  lhs = lhs | rhs;
  return lhs;
}

template <BitMaskEnum Enum>
constexpr Enum& operator&=(Enum& lhs, Enum rhs) {
  lhs = lhs & rhs;
  return lhs;
}

template <BitMaskEnum Enum>
constexpr bool HasAnyFlag(Enum value, Enum flags) {
  using Underlying = std::underlying_type_t<Enum>;
  return (static_cast<Underlying>(value) & static_cast<Underlying>(flags)) != 0;
}

template <>
struct EnableBitMaskOperators<ERHIBufferUsage> : std::true_type {};
template <>
struct EnableBitMaskOperators<ERHITextureUsage> : std::true_type {};
template <>
struct EnableBitMaskOperators<ERHIPipelineStage> : std::true_type {};
template <>
struct EnableBitMaskOperators<ERHIAccessFlags> : std::true_type {};
template <>
struct EnableBitMaskOperators<ERHIShaderStage> : std::true_type {};
template <>
struct EnableBitMaskOperators<ERHIColorComponent> : std::true_type {};
template <>
struct EnableBitMaskOperators<ERHITextureAspectFlags> : std::true_type {};

constexpr std::uint64_t EncodeHandleId(std::uint16_t generation,
                                       ERHIResourceType type,
                                       std::uint64_t index) {
  return (static_cast<std::uint64_t>(generation) << kRHIHandleGenerationShift) |
         (static_cast<std::uint64_t>(type) << kRHIHandleTypeShift) |
         (index & kRHIHandleIndexMask);
}

constexpr std::uint16_t DecodeHandleGeneration(std::uint64_t id) {
  return static_cast<std::uint16_t>(id >> kRHIHandleGenerationShift);
}

constexpr ERHIResourceType DecodeHandleType(std::uint64_t id) {
  return static_cast<ERHIResourceType>((id >> kRHIHandleTypeShift) & 0xffull);
}

constexpr std::uint64_t DecodeHandleIndex(std::uint64_t id) {
  return id & kRHIHandleIndexMask;
}

inline constexpr const char* ToString(RHIResult result) {
  switch (result) {
    case RHIResult::Success:
      return "Success";
    case RHIResult::ErrorOutOfMemory:
      return "ErrorOutOfMemory";
    case RHIResult::ErrorDeviceLost:
      return "ErrorDeviceLost";
    case RHIResult::ErrorInvalidHandle:
      return "ErrorInvalidHandle";
    case RHIResult::ErrorUnsupported:
      return "ErrorUnsupported";
    case RHIResult::ErrorValidation:
      return "ErrorValidation";
    case RHIResult::ErrorSwapchainOutOfDate:
      return "ErrorSwapchainOutOfDate";
    case RHIResult::ErrorTimeout:
      return "ErrorTimeout";
    default:
      return "Unknown";
  }
}

#define FABRICA_RHI_DECLARE_CREATE_RESULT(Name, HandleType)                  \
  struct Name {                                                              \
    HandleType handle{};                                                     \
    RHIResult result = RHIResult::ErrorValidation;                           \
    [[nodiscard]] constexpr bool Succeeded() const {                         \
      return result == RHIResult::Success && handle.IsValid();               \
    }                                                                        \
  }

FABRICA_RHI_DECLARE_CREATE_RESULT(RHIBufferCreateResult, RHIBufferHandle);
FABRICA_RHI_DECLARE_CREATE_RESULT(RHITextureCreateResult, RHITextureHandle);
FABRICA_RHI_DECLARE_CREATE_RESULT(RHISamplerCreateResult, RHISamplerHandle);
FABRICA_RHI_DECLARE_CREATE_RESULT(RHIShaderCreateResult, RHIShaderHandle);
FABRICA_RHI_DECLARE_CREATE_RESULT(RHIRenderPassCreateResult, RHIRenderPassHandle);
FABRICA_RHI_DECLARE_CREATE_RESULT(RHIPipelineCreateResult, RHIPipelineHandle);
FABRICA_RHI_DECLARE_CREATE_RESULT(RHIDescriptorSetCreateResult, RHIDescriptorSetHandle);
FABRICA_RHI_DECLARE_CREATE_RESULT(RHIFenceCreateResult, RHIFenceHandle);

#undef FABRICA_RHI_DECLARE_CREATE_RESULT

struct RHIExtent2D {
  std::uint32_t width = 0;
  std::uint32_t height = 0;
};

struct RHIExtent3D {
  std::uint32_t width = 0;
  std::uint32_t height = 0;
  std::uint32_t depth = 1;
};

struct RHIOffset3D {
  std::int32_t x = 0;
  std::int32_t y = 0;
  std::int32_t z = 0;
};

struct RHIRect {
  std::int32_t x = 0;
  std::int32_t y = 0;
  std::uint32_t width = 0;
  std::uint32_t height = 0;
};

struct RHIMappedRange {
  void* data = nullptr;
  std::size_t offset = 0;
  std::size_t size = 0;
};

struct RHIViewport {
  float x = 0.0f;
  float y = 0.0f;
  float width = 0.0f;
  float height = 0.0f;
  float min_depth = 0.0f;
  float max_depth = 1.0f;
};

struct RHIColorClearValue {
  float r = 0.0f;
  float g = 0.0f;
  float b = 0.0f;
  float a = 1.0f;
};

struct RHIDepthStencilClearValue {
  float depth = 1.0f;
  std::uint32_t stencil = 0;
};

struct RHIContextDesc {
  void* window_handle = nullptr;
  void* window_instance = nullptr;
  ERHIWindowHandleType window_handle_type = ERHIWindowHandleType::None;
  RHIExtent2D initial_extent{1280u, 720u};
  bool enable_validation = false;
  bool enable_vsync = true;
  std::uint32_t frames_in_flight = kFramesInFlight;
  char application_name[kMaxApplicationNameLength] = "FabricaEngine";
  char engine_name[kMaxApplicationNameLength] = "FabricaEngine";
  char pipeline_cache_path[260] = "fabrica_vulkan_pipeline_cache.bin";
};

struct RHIDeviceInfo {
  char device_name[kMaxDeviceNameLength] = {};
  std::uint32_t vendor_id = 0;
  std::uint32_t device_id = 0;
  std::uint32_t api_version = 0;
  std::uint32_t driver_version = 0;
  std::uint64_t device_local_memory_bytes = 0;
  std::uint64_t host_visible_memory_bytes = 0;
  std::uint32_t graphics_queue_family = 0;
  std::uint32_t compute_queue_family = 0;
  std::uint32_t transfer_queue_family = 0;
  bool has_async_compute = false;
  bool has_async_transfer = false;
  bool is_discrete_gpu = false;
  std::int32_t score = 0;
};

struct RHIBufferDesc {
  std::uint64_t size = 0;
  std::uint32_t stride = 0;
  ERHIBufferUsage usage = ERHIBufferUsage::None;
  ERHIMemoryType memory = ERHIMemoryType::DeviceLocal;
  bool mapped_at_creation = false;
};

struct RHITextureDesc {
  std::uint32_t width = 1;
  std::uint32_t height = 1;
  std::uint32_t depth = 1;
  std::uint32_t mip_levels = 1;
  std::uint32_t array_layers = 1;
  ERHITextureFormat format = ERHITextureFormat::Undefined;
  ERHITextureUsage usage = ERHITextureUsage::None;
  ERHITextureAspectFlags aspect = ERHITextureAspectFlags::Color;
  ERHIMemoryType memory = ERHIMemoryType::DeviceLocal;
  ERHISampleCount samples = ERHISampleCount::Sample1;
  ERHITextureLayout initial_layout = ERHITextureLayout::Undefined;
};

struct RHISamplerDesc {
  ERHIFilter min_filter = ERHIFilter::Linear;
  ERHIFilter mag_filter = ERHIFilter::Linear;
  ERHIFilter mip_filter = ERHIFilter::Linear;
  ERHISamplerAddressMode address_u = ERHISamplerAddressMode::Repeat;
  ERHISamplerAddressMode address_v = ERHISamplerAddressMode::Repeat;
  ERHISamplerAddressMode address_w = ERHISamplerAddressMode::Repeat;
  float mip_lod_bias = 0.0f;
  bool anisotropy_enable = false;
  float max_anisotropy = 1.0f;
  bool compare_enable = false;
  ERHICompareOp compare_op = ERHICompareOp::Always;
  float min_lod = 0.0f;
  float max_lod = 16.0f;
  ERHIBorderColor border_color = ERHIBorderColor::FloatOpaqueBlack;
};

struct RHIShaderDesc {
  ERHIShaderStage stage = ERHIShaderStage::None;
  const std::uint32_t* bytecode = nullptr;
  std::uint64_t bytecode_size = 0;
  char entry_point[kMaxShaderEntryPointLength] = "main";
};

struct RHIAttachmentDesc {
  ERHITextureFormat format = ERHITextureFormat::Undefined;
  ERHISampleCount samples = ERHISampleCount::Sample1;
  ERHILoadOp load_op = ERHILoadOp::Clear;
  ERHIStoreOp store_op = ERHIStoreOp::Store;
  ERHILoadOp stencil_load_op = ERHILoadOp::DontCare;
  ERHIStoreOp stencil_store_op = ERHIStoreOp::DontCare;
  ERHITextureLayout initial_layout = ERHITextureLayout::Undefined;
  ERHITextureLayout final_layout = ERHITextureLayout::Undefined;
};

struct RHIRenderPassDesc {
  std::uint32_t color_attachment_count = 0;
  RHIAttachmentDesc color_attachments[kMaxColorAttachments] = {};
  bool has_depth_stencil_attachment = false;
  RHIAttachmentDesc depth_stencil_attachment = {};
};

struct RHIDescriptorBindingDesc {
  std::uint32_t binding = 0;
  ERHIDescriptorType type = ERHIDescriptorType::UniformBuffer;
  std::uint32_t count = 1;
  ERHIShaderStage stage_mask = ERHIShaderStage::None;
};

struct RHIDescriptorSetLayoutDesc {
  std::uint32_t binding_count = 0;
  RHIDescriptorBindingDesc bindings[kMaxDescriptorBindings] = {};
};

struct RHIPushConstantRangeDesc {
  std::uint32_t offset = 0;
  std::uint32_t size = 0;
  ERHIShaderStage stage_mask = ERHIShaderStage::None;
};

struct RHIPipelineLayoutDesc {
  std::uint32_t set_layout_count = 0;
  RHIDescriptorSetLayoutDesc set_layouts[kMaxDescriptorSets] = {};
  std::uint32_t push_constant_range_count = 0;
  RHIPushConstantRangeDesc push_constant_ranges[kMaxPushConstantRanges] = {};
};

struct RHIVertexBindingDesc {
  std::uint32_t binding = 0;
  std::uint32_t stride = 0;
  ERHIVertexInputRate input_rate = ERHIVertexInputRate::PerVertex;
};

struct RHIVertexAttributeDesc {
  std::uint32_t location = 0;
  std::uint32_t binding = 0;
  ERHITextureFormat format = ERHITextureFormat::Undefined;
  std::uint32_t offset = 0;
};

struct RHIVertexInputDesc {
  std::uint32_t binding_count = 0;
  RHIVertexBindingDesc bindings[kMaxVertexBindings] = {};
  std::uint32_t attribute_count = 0;
  RHIVertexAttributeDesc attributes[kMaxVertexAttributes] = {};
};

struct RHIRasterizerDesc {
  ERHIPolygonMode polygon_mode = ERHIPolygonMode::Fill;
  ERHICullMode cull_mode = ERHICullMode::Back;
  ERHIFrontFace front_face = ERHIFrontFace::CounterClockwise;
  bool depth_clamp_enable = false;
  bool depth_bias_enable = false;
  float depth_bias_constant = 0.0f;
  float depth_bias_clamp = 0.0f;
  float depth_bias_slope = 0.0f;
  float line_width = 1.0f;
};

struct RHIColorBlendAttachmentDesc {
  bool blend_enable = false;
  ERHIBlendFactor src_color_factor = ERHIBlendFactor::One;
  ERHIBlendFactor dst_color_factor = ERHIBlendFactor::Zero;
  ERHIBlendOp color_op = ERHIBlendOp::Add;
  ERHIBlendFactor src_alpha_factor = ERHIBlendFactor::One;
  ERHIBlendFactor dst_alpha_factor = ERHIBlendFactor::Zero;
  ERHIBlendOp alpha_op = ERHIBlendOp::Add;
  ERHIColorComponent write_mask = ERHIColorComponent::All;
};

struct RHIBlendDesc {
  std::uint32_t attachment_count = 0;
  RHIColorBlendAttachmentDesc attachments[kMaxColorAttachments] = {};
  float blend_constants[4] = {0.0f, 0.0f, 0.0f, 0.0f};
};

struct RHIStencilFaceDesc {
  ERHIStencilOp fail_op = ERHIStencilOp::Keep;
  ERHIStencilOp pass_op = ERHIStencilOp::Keep;
  ERHIStencilOp depth_fail_op = ERHIStencilOp::Keep;
  ERHICompareOp compare_op = ERHICompareOp::Always;
  std::uint32_t compare_mask = 0xffu;
  std::uint32_t write_mask = 0xffu;
  std::uint32_t reference = 0u;
};

struct RHIDepthStencilDesc {
  bool depth_test_enable = false;
  bool depth_write_enable = false;
  ERHICompareOp depth_compare_op = ERHICompareOp::LessOrEqual;
  bool depth_bounds_test_enable = false;
  bool stencil_test_enable = false;
  RHIStencilFaceDesc front = {};
  RHIStencilFaceDesc back = {};
};

struct RHIMultisampleDesc {
  ERHISampleCount sample_count = ERHISampleCount::Sample1;
  bool sample_shading_enable = false;
  float min_sample_shading = 0.0f;
};

struct RHIGraphicsPipelineDesc {
  RHIShaderHandle vertex_shader = {};
  RHIShaderHandle fragment_shader = {};
  RHIShaderHandle geometry_shader = {};
  RHIShaderHandle tess_ctrl_shader = {};
  RHIShaderHandle tess_eval_shader = {};
  RHIRenderPassHandle render_pass = {};
  RHIVertexInputDesc vertex_input = {};
  RHIRasterizerDesc rasterizer = {};
  RHIBlendDesc blend = {};
  RHIDepthStencilDesc depth_stencil = {};
  RHIMultisampleDesc multisample = {};
  RHIPipelineLayoutDesc layout = {};
  ERHIPrimitiveTopology topology = ERHIPrimitiveTopology::TriangleList;
};

struct RHIComputePipelineDesc {
  RHIShaderHandle compute_shader = {};
  RHIPipelineLayoutDesc layout = {};
};

struct RHIDescriptorSetDesc {
  RHIDescriptorSetLayoutDesc layout = {};
  ERHIDescriptorFrequency frequency = ERHIDescriptorFrequency::Persistent;
};

struct RHIDescriptorBufferWriteDesc {
  RHIBufferHandle buffer = {};
  std::uint64_t offset = 0;
  std::uint64_t range = 0;
};

struct RHIDescriptorTextureWriteDesc {
  RHITextureHandle texture = {};
  ERHITextureLayout layout = ERHITextureLayout::ShaderReadOnly;
};

struct RHIDescriptorSamplerWriteDesc {
  RHISamplerHandle sampler = {};
};

struct RHITextureSubresourceRange {
  ERHITextureAspectFlags aspect = ERHITextureAspectFlags::Color;
  std::uint32_t base_mip_level = 0;
  std::uint32_t level_count = 1;
  std::uint32_t base_array_layer = 0;
  std::uint32_t layer_count = 1;
};

struct RHITextureUpdateDesc {
  const void* data = nullptr;
  std::uint64_t data_size = 0;
  std::uint32_t width = 0;
  std::uint32_t height = 0;
  std::uint32_t depth = 1;
  std::uint32_t mip_level = 0;
  std::uint32_t base_array_layer = 0;
  std::uint32_t layer_count = 1;
  std::uint32_t row_pitch = 0;
  std::uint32_t slice_pitch = 0;
  ERHITextureAspectFlags aspect = ERHITextureAspectFlags::Color;
};

struct RHIBufferCopyRegion {
  std::uint64_t src_offset = 0;
  std::uint64_t dst_offset = 0;
  std::uint64_t size = 0;
};

struct RHIBufferTextureCopyRegion {
  std::uint64_t buffer_offset = 0;
  std::uint32_t buffer_row_length = 0;
  std::uint32_t buffer_image_height = 0;
  std::uint32_t texture_mip_level = 0;
  std::uint32_t base_array_layer = 0;
  std::uint32_t layer_count = 1;
  RHIOffset3D texture_offset = {};
  RHIExtent3D texture_extent = {};
  ERHITextureAspectFlags aspect = ERHITextureAspectFlags::Color;
};

struct RHIRenderPassColorAttachmentBeginDesc {
  RHITextureHandle texture = {};
  ERHITextureLayout layout = ERHITextureLayout::ColorAttachment;
  ERHILoadOp load_op = ERHILoadOp::Load;
  ERHIStoreOp store_op = ERHIStoreOp::Store;
  RHIColorClearValue clear_value = {};
};

struct RHIRenderPassDepthStencilAttachmentBeginDesc {
  RHITextureHandle texture = {};
  ERHITextureLayout layout = ERHITextureLayout::DepthStencil;
  ERHILoadOp depth_load_op = ERHILoadOp::Load;
  ERHIStoreOp depth_store_op = ERHIStoreOp::Store;
  ERHILoadOp stencil_load_op = ERHILoadOp::DontCare;
  ERHIStoreOp stencil_store_op = ERHIStoreOp::DontCare;
  RHIDepthStencilClearValue clear_value = {};
};

struct RHIRenderPassBeginDesc {
  RHIRect render_area = {};
  std::uint32_t layer_count = 1;
  std::uint32_t color_attachment_count = 0;
  RHIRenderPassColorAttachmentBeginDesc color_attachments[kMaxColorAttachments] = {};
  bool has_depth_stencil_attachment = false;
  RHIRenderPassDepthStencilAttachmentBeginDesc depth_stencil_attachment = {};
};

}  // namespace Fabrica::RHI

