#pragma once
#include <cmath>
#include <functional>

namespace Utils {
    inline float Clamp01(float v) {
        if (v > 1.0f) return 1.0f;
        if (v < 0.0f) return 0.0f;
        return v;
    }

    inline float LerpStep(float l, float h, float v) {
        return Clamp01((v - l) / (h - l));
    }

    inline float Lerp(float a, float b, float t) {
        return a + (b - a) * Clamp01(t);
    }

    inline float SmoothStep(float p_Min, float p_Max, float p_X) {
        float num = Clamp01((p_X - p_Min) / (p_Max - p_Min));
        return num * num * (3.0f - 2.0f * num);
    }

    inline float LerpSmooth(float a, float b, float dt, float h) {
        return b + (a - b) * std::pow(2.0f, -dt / h);
    }

    inline float Length(float x, float y) {
        return std::sqrt(x * x + y * y);
    }

    inline double Length(double x, double y) {
        return std::sqrt(x * x + y * y);
    }
}

// Global Vector2i and Hash for Map/Set usage
struct Vector2i {
    int x;
    int z;

    bool operator==(const Vector2i& other) const {
        return x == other.x && z == other.z;
    }
    bool operator!=(const Vector2i& other) const {
        return !(*this == other);
    }
};

struct Vector2iHash {
    std::size_t operator()(const Vector2i& v) const {
        std::size_t h1 = std::hash<int>{}(v.x);
        std::size_t h2 = std::hash<int>{}(v.z);
        return h1 ^ (h2 << 1);
    }
};
