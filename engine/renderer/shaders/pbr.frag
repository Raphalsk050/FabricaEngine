#version 450
#extension GL_GOOGLE_include_directive : require

#include "common/lighting.glsl"
#include "common/material_inputs.glsl"

layout(location = 0) in vec3 in_world_position;
layout(location = 1) in vec3 in_world_normal;
layout(location = 2) in vec2 in_uv;

layout(location = 0) out vec4 out_color;

void main() {
  vec3 base_color = u_draw.base_color.rgb;
  float alpha = u_draw.base_color.a;
  float metallic = saturate(u_draw.material_params.x);
  float perceptual_roughness = saturate(u_draw.material_params.y);
  float reflectance = saturate(u_draw.material_params.z);

  vec3 n = normalize(in_world_normal);
  vec3 v = normalize(u_frame.camera_position_exposure.xyz - in_world_position);
  float roughness = max(perceptual_roughness * perceptual_roughness, 0.089 * 0.089);
  vec3 diffuse_color = (1.0 - metallic) * base_color;
  vec3 f0 = 0.16 * reflectance * reflectance * (1.0 - metallic) + base_color * metallic;

  vec3 color = evaluateDirectionalLight(u_frame.directional_light.direction_intensity.xyz,
                                        u_frame.directional_light.color.rgb,
                                        u_frame.directional_light.direction_intensity.w,
                                        n,
                                        v,
                                        diffuse_color,
                                        f0,
                                        roughness);

  color += u_draw.emissive_exposure.rgb * exp2(u_draw.emissive_exposure.w);
  out_color = vec4(color, alpha);
}
