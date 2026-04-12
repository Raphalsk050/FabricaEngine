#ifndef FABRICA_RENDERER_LIGHTING_GLSL
#define FABRICA_RENDERER_LIGHTING_GLSL

#include "brdf.glsl"

void evaluateBRDF(vec3 n,
                  vec3 v,
                  vec3 l,
                  vec3 diffuse_color,
                  vec3 f0,
                  float roughness,
                  out vec3 Fd,
                  out vec3 Fr) {
  vec3 h = normalize(v + l);
  float NoV = abs(dot(n, v)) + 1.0e-5;
  float NoL = saturate(dot(n, l));
  float NoH = saturate(dot(n, h));
  float LoH = saturate(dot(l, h));

  float D = D_GGX(roughness, NoH, n, h);
  vec3 F = F_Schlick(LoH, f0);
  float V = V_SmithGGXCorrelatedFast(NoV, NoL, roughness);

  Fr = (D * V) * F;
  Fd = diffuse_color * Fd_Lambert();
}

vec3 evaluateDirectionalLight(vec3 light_direction,
                              vec3 light_color,
                              float illuminance,
                              vec3 n,
                              vec3 v,
                              vec3 diffuse_color,
                              vec3 f0,
                              float roughness) {
  vec3 l = normalize(-light_direction);
  float NoL = saturate(dot(n, l));
  vec3 Fd;
  vec3 Fr;
  evaluateBRDF(n, v, l, diffuse_color, f0, roughness, Fd, Fr);
  return (Fd + Fr) * (illuminance * NoL) * light_color;
}

#endif
