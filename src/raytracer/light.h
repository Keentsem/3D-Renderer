#ifndef RT_LIGHT_H
#define RT_LIGHT_H

#include "../core/vec.h"
#include <cmath>

// ============================================================
// Light system — adapted from scratchapixel reference
// ============================================================

struct RTLightInfo {
    Vec3 direction;    // from hit point toward light
    Vec3 intensity;    // light color * power (pre-attenuated)
    double distance;   // for shadow ray tmax
};

class RTLight {
public:
    Vec3 color;
    double intensity;

    RTLight(const Vec3& color, double intensity) : color(color), intensity(intensity) {}
    virtual ~RTLight() = default;

    virtual RTLightInfo illuminate(const Vec3& hit_point) const = 0;
};

// Directional light — infinite distance, no falloff
class DistantLight : public RTLight {
public:
    Vec3 direction;  // points FROM the light toward the scene

    DistantLight(const Vec3& dir, const Vec3& color, double intensity)
        : RTLight(color, intensity), direction(normalized(dir)) {}

    RTLightInfo illuminate(const Vec3&) const override {
        RTLightInfo info;
        info.direction = direction * -1.0;  // toward light
        info.intensity = color * intensity;
        info.distance = 1e30;
        return info;
    }
};

// Point light — inverse square falloff
class PointLight : public RTLight {
public:
    Vec3 position;

    PointLight(const Vec3& pos, const Vec3& color, double intensity)
        : RTLight(color, intensity), position(pos) {}

    RTLightInfo illuminate(const Vec3& hit_point) const override {
        RTLightInfo info;
        Vec3 delta = position - hit_point;
        double r2 = delta.norm2();
        info.distance = std::sqrt(r2);
        info.direction = delta / info.distance;
        info.intensity = color * (intensity / (4.0 * M_PI * r2 + 1e-6));
        return info;
    }
};

#endif
