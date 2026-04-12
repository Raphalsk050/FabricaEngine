#pragma once

#include <array>
#include <cmath>

namespace Fabrica::Renderer::Math {

struct Vec2f {
  float x = 0.0f;
  float y = 0.0f;
};

struct Vec3f {
  float x = 0.0f;
  float y = 0.0f;
  float z = 0.0f;
};

struct alignas(16) Vec4f {
  float x = 0.0f;
  float y = 0.0f;
  float z = 0.0f;
  float w = 0.0f;
};

struct alignas(16) Mat4f {
  std::array<float, 16> elements{};

  [[nodiscard]] float* Data() { return elements.data(); }
  [[nodiscard]] const float* Data() const { return elements.data(); }
};

[[nodiscard]] inline constexpr float Pi() {
  return 3.14159265358979323846f;
}

[[nodiscard]] inline constexpr float Radians(float degrees) {
  return degrees * (Pi() / 180.0f);
}

[[nodiscard]] inline constexpr Vec3f operator+(const Vec3f& lhs, const Vec3f& rhs) {
  return {.x = lhs.x + rhs.x, .y = lhs.y + rhs.y, .z = lhs.z + rhs.z};
}

[[nodiscard]] inline constexpr Vec3f operator-(const Vec3f& lhs, const Vec3f& rhs) {
  return {.x = lhs.x - rhs.x, .y = lhs.y - rhs.y, .z = lhs.z - rhs.z};
}

[[nodiscard]] inline constexpr Vec3f operator*(const Vec3f& value, float scalar) {
  return {.x = value.x * scalar, .y = value.y * scalar, .z = value.z * scalar};
}

[[nodiscard]] inline constexpr float Dot(const Vec3f& lhs, const Vec3f& rhs) {
  return lhs.x * rhs.x + lhs.y * rhs.y + lhs.z * rhs.z;
}

[[nodiscard]] inline constexpr Vec3f Cross(const Vec3f& lhs, const Vec3f& rhs) {
  return {.x = lhs.y * rhs.z - lhs.z * rhs.y,
          .y = lhs.z * rhs.x - lhs.x * rhs.z,
          .z = lhs.x * rhs.y - lhs.y * rhs.x};
}

[[nodiscard]] inline float Length(const Vec3f& value) {
  return std::sqrt(Dot(value, value));
}

[[nodiscard]] inline Vec3f Normalize(const Vec3f& value) {
  const float length = Length(value);
  if (length <= 1.0e-6f) {
    return {};
  }
  return value * (1.0f / length);
}

[[nodiscard]] inline constexpr Mat4f Identity() {
  Mat4f matrix{};
  matrix.elements[0] = 1.0f;
  matrix.elements[5] = 1.0f;
  matrix.elements[10] = 1.0f;
  matrix.elements[15] = 1.0f;
  return matrix;
}

[[nodiscard]] inline Mat4f Multiply(const Mat4f& lhs, const Mat4f& rhs) {
  Mat4f result{};
  for (int column = 0; column < 4; ++column) {
    for (int row = 0; row < 4; ++row) {
      float value = 0.0f;
      for (int k = 0; k < 4; ++k) {
        value += lhs.elements[k * 4 + row] * rhs.elements[column * 4 + k];
      }
      result.elements[column * 4 + row] = value;
    }
  }
  return result;
}

[[nodiscard]] inline Mat4f Translation(const Vec3f& offset) {
  Mat4f matrix = Identity();
  matrix.elements[12] = offset.x;
  matrix.elements[13] = offset.y;
  matrix.elements[14] = offset.z;
  return matrix;
}

[[nodiscard]] inline Mat4f Perspective(float vertical_fov_radians,
                                       float aspect_ratio,
                                       float near_plane,
                                       float far_plane) {
  Mat4f matrix{};
  const float tan_half_fov = std::tan(vertical_fov_radians * 0.5f);
  matrix.elements[0] = 1.0f / (aspect_ratio * tan_half_fov);
  matrix.elements[5] = 1.0f / tan_half_fov;
  matrix.elements[10] = far_plane / (near_plane - far_plane);
  matrix.elements[11] = -1.0f;
  matrix.elements[14] = (far_plane * near_plane) / (near_plane - far_plane);
  return matrix;
}

[[nodiscard]] inline Mat4f LookAt(const Vec3f& eye,
                                  const Vec3f& target,
                                  const Vec3f& up) {
  const Vec3f forward = Normalize(target - eye);
  const Vec3f right = Normalize(Cross(forward, up));
  const Vec3f camera_up = Cross(right, forward);

  Mat4f matrix = Identity();
  matrix.elements[0] = right.x;
  matrix.elements[1] = camera_up.x;
  matrix.elements[2] = -forward.x;
  matrix.elements[4] = right.y;
  matrix.elements[5] = camera_up.y;
  matrix.elements[6] = -forward.y;
  matrix.elements[8] = right.z;
  matrix.elements[9] = camera_up.z;
  matrix.elements[10] = -forward.z;
  matrix.elements[12] = -Dot(right, eye);
  matrix.elements[13] = -Dot(camera_up, eye);
  matrix.elements[14] = Dot(forward, eye);
  return matrix;
}

}  // namespace Fabrica::Renderer::Math
