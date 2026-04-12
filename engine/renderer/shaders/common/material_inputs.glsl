#ifndef FABRICA_RENDERER_MATERIAL_INPUTS_GLSL
#define FABRICA_RENDERER_MATERIAL_INPUTS_GLSL

struct DirectionalLightData {
  vec4 direction_intensity;
  vec4 color;
};

layout(std140, set = 0, binding = 0) uniform FrameUniforms {
  mat4 view_projection;
  vec4 camera_position_exposure;
  DirectionalLightData directional_light;
} u_frame;

layout(push_constant) uniform DrawPushConstants {
  mat4 model;
  vec4 base_color;
  vec4 material_params;
  vec4 emissive_exposure;
} u_draw;

#endif
