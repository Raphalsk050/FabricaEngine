#ifndef FABRICA_RENDERER_BRDF_GLSL
#define FABRICA_RENDERER_BRDF_GLSL

#include "math.glsl"

float D_GGX(float roughness, float NoH, vec3 n, vec3 h) {
  vec3 NxH = cross(n, h);
  float a = NoH * roughness;
  float k = roughness / (dot(NxH, NxH) + a * a);
  float d = k * k * (1.0 / PI);
  return min(d, 65504.0);
}

float V_SmithGGXCorrelatedFast(float NoV, float NoL, float roughness) {
  float a = roughness;
  float GGXV = NoL * (NoV * (1.0 - a) + a);
  float GGXL = NoV * (NoL * (1.0 - a) + a);
  return 0.5 / max(GGXV + GGXL, 1.0e-5);
}

vec3 F_Schlick(float u, vec3 f0) {
  float f = pow(1.0 - u, 5.0);
  return f + f0 * (1.0 - f);
}

float Fd_Lambert() {
  return 1.0 / PI;
}

#endif
