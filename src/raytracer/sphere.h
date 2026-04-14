#ifndef SPHERE_H
#define SPHERE_H

#include "../core/vec.h"
#include "hittable.h"
#include "bvh.h"
#include "material.h"
#include <cmath>
#include <memory>

// ============================================================
// Sphere Class
// ============================================================

class Sphere : public Hittable {
public:
    Vec3 center;
    double radius;
    std::shared_ptr<Material> material;

    Sphere(const Vec3& center, double radius, std::shared_ptr<Material> mat)
        : center(center), radius(radius), material(mat) {}

    bool hit(const Ray& r, double t_min, double t_max, HitRecord& rec) const override {
        // Solve: |origin + t*direction - center|² = radius²
        // This becomes: a*t² + 2*b*t + c = 0
        Vec3 oc = r.origin - center;
        double a = r.direction.norm2();
        double half_b = oc * r.direction;
        double c = oc.norm2() - radius * radius;
        double discriminant = half_b * half_b - a * c;

        if (discriminant < 0) return false;

        double sqrtd = std::sqrt(discriminant);

        // Find the nearest root that lies in the acceptable range
        double root = (-half_b - sqrtd) / a;
        if (root < t_min || root > t_max) {
            root = (-half_b + sqrtd) / a;
            if (root < t_min || root > t_max)
                return false;
        }

        rec.t = root;
        rec.point = r.at(rec.t);
        Vec3 outward_normal = (rec.point - center) / radius;
        rec.set_face_normal(r, outward_normal);
        rec.material = material;

        return true;
    }

    bool bounding_box(AABB& output) const override {
        output = AABB(
            center - Vec3(radius, radius, radius),
            center + Vec3(radius, radius, radius)
        );
        return true;
    }
};

#endif
