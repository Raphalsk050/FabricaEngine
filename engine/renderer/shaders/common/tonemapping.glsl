#ifndef FABRICA_RENDERER_TONEMAPPING_GLSL
#define FABRICA_RENDERER_TONEMAPPING_GLSL

vec3 ACESFilm(vec3 x) {
  const float a = 2.51;
  const float b = 0.03;
  const float c = 2.43;
  const float d = 0.59;
  const float e = 0.14;
  return clamp((x * (a * x + b)) / (x * (c * x + d) + e), 0.0, 1.0);
}

vec3 linearToSRGB(vec3 linear) {
  vec3 lo = linear * 12.92;
  vec3 hi = 1.055 * pow(linear, vec3(1.0 / 2.4)) - 0.055;
  return mix(lo, hi, step(vec3(0.0031308), linear));
}

#endif
