#ifndef FABRICA_RENDERER_MATH_GLSL
#define FABRICA_RENDERER_MATH_GLSL

const float PI = 3.14159265359;

float saturate(float value) {
  return clamp(value, 0.0, 1.0);
}

vec2 saturate(vec2 value) {
  return clamp(value, vec2(0.0), vec2(1.0));
}

vec3 saturate(vec3 value) {
  return clamp(value, vec3(0.0), vec3(1.0));
}

float sq(float value) {
  return value * value;
}

#endif
