// Small value types and helpers shared by the generated code and the core.
#pragma once

#include <cctype>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <string>

namespace timerewind {

struct Vec3 {
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
};

inline float lerp_f32(float a, float b, float t) { return a + (b - a) * t; }

inline Vec3 lerp_vec3(const Vec3& a, const Vec3& b, float t) {
    return {lerp_f32(a.x, b.x, t), lerp_f32(a.y, b.y, t), lerp_f32(a.z, b.z, t)};
}

// Angles in degrees, interpolated along the shorter arc, result in [0, 360).
inline float lerp_angle_deg(float a, float b, float t) {
    float d = std::fmod(b - a, 360.0f);
    if (d > 180.0f) {
        d -= 360.0f;
    } else if (d < -180.0f) {
        d += 360.0f;
    }
    float r = std::fmod(a + d * t, 360.0f);
    if (r < 0.0f) r += 360.0f;
    return r;
}

inline int32_t clamp_i32(int32_t v, int32_t lo, int32_t hi) { return v < lo ? lo : (v > hi ? hi : v); }
inline float clamp_f32(float v, float lo, float hi) { return v < lo ? lo : (v > hi ? hi : v); }

// The parse helpers never throw: a value that does not parse keeps `fallback`.
inline int32_t parse_i32(const std::string& s, int32_t fallback) {
    if (s.empty()) return fallback;
    const bool hex = s.size() > 2 && s[0] == '0' && (s[1] == 'x' || s[1] == 'X');
    char* end = nullptr;
    const long long v = std::strtoll(s.c_str(), &end, hex ? 16 : 10);
    if (end == s.c_str() || *end != '\0') return fallback;
    if (v < INT32_MIN || v > INT32_MAX) return fallback;
    return static_cast<int32_t>(v);
}

inline float parse_f32(const std::string& s, float fallback) {
    if (s.empty()) return fallback;
    char* end = nullptr;
    const float v = std::strtof(s.c_str(), &end);
    if (end == s.c_str() || *end != '\0' || !std::isfinite(v)) return fallback;
    return v;
}

inline bool parse_bool(const std::string& s, bool fallback) {
    std::string l;
    for (char c : s) l += static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    if (l == "1" || l == "true" || l == "yes" || l == "on") return true;
    if (l == "0" || l == "false" || l == "no" || l == "off") return false;
    return fallback;
}

}  // namespace timerewind
