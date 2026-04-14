#ifndef BVH_H
#define BVH_H

#include "hittable.h"
#include "ray.h"
#include "../core/vec.h"
#include <vector>
#include <memory>
#include <algorithm>
#include <limits>

// ============================================================
// Axis-Aligned Bounding Box
// ============================================================

struct AABB {
    Vec3 minimum;
    Vec3 maximum;

    AABB() = default;
    AABB(const Vec3& a, const Vec3& b) : minimum(a), maximum(b) {}

    bool hit(const Ray& r, double t_min, double t_max) const {
        for (int a = 0; a < 3; a++) {
            double inv_d = 1.0 / r.direction[a];
            double t0 = (minimum[a] - r.origin[a]) * inv_d;
            double t1 = (maximum[a] - r.origin[a]) * inv_d;
            if (inv_d < 0) std::swap(t0, t1);
            t_min = t0 > t_min ? t0 : t_min;
            t_max = t1 < t_max ? t1 : t_max;
            if (t_max <= t_min) return false;
        }
        return true;
    }
};

inline AABB surrounding_box(const AABB& a, const AABB& b) {
    Vec3 small(std::min(a.minimum.x, b.minimum.x),
               std::min(a.minimum.y, b.minimum.y),
               std::min(a.minimum.z, b.minimum.z));
    Vec3 big(std::max(a.maximum.x, b.maximum.x),
             std::max(a.maximum.y, b.maximum.y),
             std::max(a.maximum.z, b.maximum.z));
    return AABB(small, big);
}

// ============================================================
// BVH Node — O(log N) ray-scene intersection
// ============================================================

class BVHNode : public Hittable {
public:
    std::shared_ptr<Hittable> left;
    std::shared_ptr<Hittable> right;
    AABB box;

    BVHNode(const std::vector<std::shared_ptr<Hittable>>& objects,
            size_t start, size_t end) {
        auto src = objects; // mutable copy for sorting

        // Pick axis with largest extent
        AABB total = get_box(src[start]);
        for (size_t i = start + 1; i < end; i++)
            total = surrounding_box(total, get_box(src[i]));

        Vec3 extent = total.maximum - total.minimum;
        int axis = 0;
        if (extent.y > extent.x) axis = 1;
        if (extent.z > extent[axis]) axis = 2;

        size_t span = end - start;

        if (span == 1) {
            left = right = src[start];
        } else if (span == 2) {
            if (box_compare(src[start], src[start + 1], axis)) {
                left = src[start]; right = src[start + 1];
            } else {
                left = src[start + 1]; right = src[start];
            }
        } else {
            std::sort(src.begin() + start, src.begin() + end,
                      [axis](const auto& a, const auto& b) {
                          return box_compare(a, b, axis);
                      });
            size_t mid = start + span / 2;
            left  = std::make_shared<BVHNode>(src, start, mid);
            right = std::make_shared<BVHNode>(src, mid, end);
        }

        AABB lb = get_box(left);
        AABB rb = get_box(right);
        box = surrounding_box(lb, rb);
    }

    bool hit(const Ray& r, double t_min, double t_max, HitRecord& rec) const override {
        if (!box.hit(r, t_min, t_max)) return false;

        bool hit_left  = left->hit(r, t_min, t_max, rec);
        bool hit_right = right->hit(r, t_min, hit_left ? rec.t : t_max, rec);
        return hit_left || hit_right;
    }

    bool bounding_box(AABB& output) const override {
        output = box;
        return true;
    }

private:
    static AABB get_box(const std::shared_ptr<Hittable>& h) {
        AABB b;
        h->bounding_box(b);
        return b;
    }

    static bool box_compare(const std::shared_ptr<Hittable>& a,
                            const std::shared_ptr<Hittable>& b, int axis) {
        AABB ba, bb;
        a->bounding_box(ba);
        b->bounding_box(bb);
        return ba.minimum[axis] < bb.minimum[axis];
    }
};

#endif
