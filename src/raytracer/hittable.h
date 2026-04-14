#ifndef HITTABLE_H
#define HITTABLE_H

#include "../core/vec.h"
#include "ray.h"
#include <memory>

// Forward declarations
struct Material;
struct AABB;

// ============================================================
// Hit Record
// ============================================================

struct HitRecord {
    Vec3 point;
    Vec3 normal;
    double t;
    bool front_face;
    std::shared_ptr<Material> material;

    void set_face_normal(const Ray& r, const Vec3& outward_normal) {
        front_face = (r.direction * outward_normal) < 0;
        normal = front_face ? outward_normal : outward_normal * -1.0;
    }
};

// ============================================================
// Hittable Interface
// ============================================================

class Hittable {
public:
    virtual ~Hittable() = default;
    virtual bool hit(const Ray& r, double t_min, double t_max,
                     HitRecord& rec) const = 0;
    // Returns false if no bounding box (e.g. infinite planes)
    virtual bool bounding_box(AABB& output) const { return false; }
};

#endif
