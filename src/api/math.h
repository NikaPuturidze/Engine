#pragma once
#include <algorithm>
#include <cmath>
#include <numbers>
// Distances are in blocks. Public angles are in degrees.
struct Vec2 { float x = 0, y = 0; };
struct Vec3 { float x = 0, y = 0, z = 0; bool operator==(const Vec3&) const = default; };
inline Vec3 operator+(const Vec3 a, const Vec3 b) { return {.x = a.x+b.x, .y = a.y+b.y, .z = a.z+b.z}; }
inline Vec3 operator-(const Vec3 a, const Vec3 b) { return {.x = a.x-b.x, .y = a.y-b.y, .z = a.z-b.z}; }
inline Vec3 operator*(const Vec3 a, const float s) { return {.x = a.x*s, .y = a.y*s, .z = a.z*s}; }
inline float Length(const Vec3 v) { return std::sqrt(v.x*v.x+v.y*v.y+v.z*v.z); }
inline float Distance(const Vec3 a, const Vec3 b) { return Length(a-b); }
inline Vec3 Normalize(const Vec3 v) { float n=Length(v); return n>0 ? v*(1/n) : Vec3{}; }
inline Vec3 Lerp(const Vec3 a, const Vec3 b, const float alpha) { return a+(b-a)*alpha; }
inline float Radians(const float degrees) { return degrees*std::numbers::pi_v<float>/180; }
inline Vec3 Forward(const float yaw, const float pitch)
{
    const float y=Radians(yaw), p=Radians(pitch);
    return {.x = std::sin(y)*std::cos(p), .y = -std::sin(p), .z = std::cos(y)*std::cos(p)};
}
struct Bounds { Vec3 minimum, maximum; };
struct Color { float r=1, g=1, b=1; };
