#ifndef TRIANGLE_HIT_H
#define TRIANGLE_HIT_H

#include "../core/vec.h"
#include "hittable.h"
#include "bvh.h"
#include "material.h"
#include <cmath>
#include <memory>

// ============================================================
// Triangle Hittable — Moller-Trumbore ray-triangle intersection
// ============================================================

class TriangleHit : public Hittable {
public:
    Vec3 v0, v1, v2;
    Vec3 n0, n1, n2;  // per-vertex normals for smooth shading
    std::shared_ptr<Material> material;

    TriangleHit(const Vec3& v0, const Vec3& v1, const Vec3& v2,
                const Vec3& n0, const Vec3& n1, const Vec3& n2,
                std::shared_ptr<Material> mat)
        : v0(v0), v1(v1), v2(v2), n0(n0), n1(n1), n2(n2), material(mat) {}

    // Flat-normal constructor
    TriangleHit(const Vec3& v0, const Vec3& v1, const Vec3& v2,
                std::shared_ptr<Material> mat)
        : v0(v0), v1(v1), v2(v2), material(mat) {
        Vec3 fn = normalized(cross(v1 - v0, v2 - v0));
        n0 = n1 = n2 = fn;
    }

    bool hit(const Ray& r, double t_min, double t_max, HitRecord& rec) const override {
        Vec3 e1 = v1 - v0;
        Vec3 e2 = v2 - v0;
        Vec3 pvec = cross(r.direction, e2);
        double det = e1 * pvec;

        // Backface culling disabled for raytracing (hit both sides)
        if (std::abs(det) < 1e-10) return false;

        double inv_det = 1.0 / det;
        Vec3 tvec = r.origin - v0;
        double u = (tvec * pvec) * inv_det;
        if (u < 0.0 || u > 1.0) return false;

        Vec3 qvec = cross(tvec, e1);
        double v = (r.direction * qvec) * inv_det;
        if (v < 0.0 || u + v > 1.0) return false;

        double t = (e2 * qvec) * inv_det;
        if (t < t_min || t > t_max) return false;

        rec.t = t;
        rec.point = r.at(t);

        // Interpolate smooth normal via barycentric coords
        double w0 = 1.0 - u - v;
        Vec3 interp_normal = normalized(n0 * w0 + n1 * u + n2 * v);
        rec.set_face_normal(r, interp_normal);
        rec.material = material;

        return true;
    }

    bool bounding_box(AABB& output) const override {
        Vec3 mn(std::min({v0.x, v1.x, v2.x}) - 1e-4,
                std::min({v0.y, v1.y, v2.y}) - 1e-4,
                std::min({v0.z, v1.z, v2.z}) - 1e-4);
        Vec3 mx(std::max({v0.x, v1.x, v2.x}) + 1e-4,
                std::max({v0.y, v1.y, v2.y}) + 1e-4,
                std::max({v0.z, v1.z, v2.z}) + 1e-4);
        output = AABB(mn, mx);
        return true;
    }
};

#endif
