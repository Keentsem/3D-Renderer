#ifndef RAY_H
#define RAY_H

#include "../core/vec.h"
#include <random>

// ============================================================
// Ray Class
// ============================================================

class Ray {
public:
    Vec3 origin;
    Vec3 direction;

    Ray() = default;
    Ray(const Vec3& origin, const Vec3& direction)
        : origin(origin), direction(normalized(direction)) {}

    Vec3 at(double t) const {
        return origin + direction * t;
    }
};

// ============================================================
// Random Utilities for Ray Tracing (thread-safe via thread_local)
// ============================================================

inline double random_double() {
    thread_local std::mt19937 rng(std::random_device{}());
    thread_local std::uniform_real_distribution<double> dist(0.0, 1.0);
    return dist(rng);
}

inline double random_double(double min, double max) {
    return min + (max - min) * random_double();
}

inline Vec3 random_vec3() {
    return Vec3(random_double(), random_double(), random_double());
}

inline Vec3 random_vec3(double min, double max) {
    return Vec3(random_double(min, max),
                random_double(min, max),
                random_double(min, max));
}

inline Vec3 random_in_unit_sphere() {
    while (true) {
        Vec3 p = random_vec3(-1, 1);
        if (p.norm2() < 1) return p;
    }
}

inline Vec3 random_unit_vector() {
    return normalized(random_in_unit_sphere());
}

inline Vec3 random_in_unit_disk() {
    while (true) {
        Vec3 p(random_double(-1, 1), random_double(-1, 1), 0);
        if (p.norm2() < 1) return p;
    }
}

#endif
