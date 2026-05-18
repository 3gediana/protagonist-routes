#pragma once
//
// core/types/Vec2.h
//
// 2D floating-point vector. Used everywhere positions, velocities, or
// directions appear. Trivially copyable; designed for value semantics.
//
// Why not use a 3rd-party math lib (glm, eigen)?
//   - We need only 2D + basic ops; glm pulls in headers we don't need
//   - viz layer can convert to raylib's Vector2 explicitly
//   - core has zero rendering dependency

#include <cmath>

namespace neuro {

struct Vec2 {
    float x{0.0f};
    float y{0.0f};

    constexpr Vec2() = default;
    constexpr Vec2(float x_, float y_) : x(x_), y(y_) {}

    // ----- arithmetic ------------------------------------------------------
    constexpr Vec2 operator+(const Vec2& o) const { return {x + o.x, y + o.y}; }
    constexpr Vec2 operator-(const Vec2& o) const { return {x - o.x, y - o.y}; }
    constexpr Vec2 operator-() const               { return {-x, -y}; }
    constexpr Vec2 operator*(float s)        const { return {x * s, y * s}; }
    constexpr Vec2 operator/(float s)        const { return {x / s, y / s}; }

    Vec2& operator+=(const Vec2& o) { x += o.x; y += o.y; return *this; }
    Vec2& operator-=(const Vec2& o) { x -= o.x; y -= o.y; return *this; }
    Vec2& operator*=(float s)       { x *= s;   y *= s;   return *this; }
    Vec2& operator/=(float s)       { x /= s;   y /= s;   return *this; }

    constexpr bool operator==(const Vec2& o) const { return x == o.x && y == o.y; }
    constexpr bool operator!=(const Vec2& o) const { return !(*this == o); }

    // ----- geometry --------------------------------------------------------
    [[nodiscard]] float length()        const { return std::sqrt(x * x + y * y); }
    [[nodiscard]] float lengthSquared() const { return x * x + y * y; }

    [[nodiscard]] Vec2 normalized() const {
        const float n = length();
        return n > 0.0f ? Vec2{x / n, y / n} : Vec2{0.0f, 0.0f};
    }

    [[nodiscard]] static float distance(const Vec2& a, const Vec2& b) {
        return (a - b).length();
    }
    [[nodiscard]] static float distanceSquared(const Vec2& a, const Vec2& b) {
        return (a - b).lengthSquared();
    }
    [[nodiscard]] static float dot(const Vec2& a, const Vec2& b) {
        return a.x * b.x + a.y * b.y;
    }
    [[nodiscard]] static float cross(const Vec2& a, const Vec2& b) {
        return a.x * b.y - a.y * b.x;
    }
};

inline constexpr Vec2 kZeroVec2{0.0f, 0.0f};

}  // namespace neuro
