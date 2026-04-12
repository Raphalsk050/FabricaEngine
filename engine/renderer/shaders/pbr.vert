#version 450
#extension GL_GOOGLE_include_directive : require

#include "common/material_inputs.glsl"

layout(location = 0) in vec3 in_position;
layout(location = 1) in vec3 in_normal;
layout(location = 2) in vec2 in_uv;

layout(location = 0) out vec3 out_world_position;
layout(location = 1) out vec3 out_world_normal;
layout(location = 2) out vec2 out_uv;

void main() {
  vec4 world_position = u_draw.model * vec4(in_position, 1.0);
  mat3 normal_matrix = mat3(u_draw.model);

  out_world_position = world_position.xyz;
  out_world_normal = normalize(normal_matrix * in_normal);
  out_uv = in_uv;
  gl_Position = u_frame.view_projection * world_position;
}
