#version 450
#extension GL_GOOGLE_include_directive : require

#include "common/tonemapping.glsl"

layout(set = 0, binding = 0) uniform texture2D u_hdr_texture;
layout(set = 0, binding = 1) uniform sampler u_hdr_sampler;

layout(location = 0) in vec2 in_uv;
layout(location = 0) out vec4 out_color;

void main() {
  vec3 hdr = texture(sampler2D(u_hdr_texture, u_hdr_sampler), in_uv).rgb;
  vec3 ldr = ACESFilm(hdr);
  out_color = vec4(linearToSRGB(ldr), 1.0);
}
