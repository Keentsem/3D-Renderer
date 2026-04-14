#ifndef SCENE_H
#define SCENE_H

#include "hittable.h"
#include "light.h"
#include <vector>
#include <memory>

// ============================================================
// Scene (Hittable List + Lights)
// ============================================================

class Scene : public Hittable {
public:
    std::vector<std::shared_ptr<Hittable>> objects;
    std::vector<std::shared_ptr<RTLight>>  lights;

    Scene() = default;

    void add(std::shared_ptr<Hittable> object) {
        objects.push_back(object);
    }

    void add_light(std::shared_ptr<RTLight> light) {
        lights.push_back(light);
    }

    void clear() {
        objects.clear();
        lights.clear();
    }

    bool hit(const Ray& r, double t_min, double t_max, HitRecord& rec) const override {
        HitRecord temp_rec;
        bool hit_anything = false;
        double closest = t_max;

        for (const auto& object : objects) {
            if (object->hit(r, t_min, closest, temp_rec)) {
                hit_anything = true;
                closest = temp_rec.t;
                rec = temp_rec;
            }
        }

        return hit_anything;
    }
};

#endif
