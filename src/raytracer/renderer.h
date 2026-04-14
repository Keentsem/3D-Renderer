#ifndef RENDERER_H
#define RENDERER_H

#include "../core/vec.h"
#include "../image/tgaimage.h"
#include "ray.h"
#include "material.h"
#include "scene.h"
#include "camera.h"
#include "sky.h"
#include <limits>
#include <iostream>
#include <cmath>
#include <algorithm>
#include <vector>
#ifdef _OPENMP
#include <omp.h>
#endif

// ============================================================
// Sun direction for sky model (configurable)
// ============================================================
static Vec3 g_sun_dir = normalized(Vec3(0.3, 0.4, 0.8));

// ============================================================
// Ray Color — with direct illumination, shadow rays, and
// Nishita sky. Adapted from scratchapixel reference.
// ============================================================

inline Vec3 ray_color(const Ray& r, const Scene& world, int depth) {
    if (depth <= 0)
        return Vec3(0, 0, 0);

    HitRecord rec;

    if (world.hit(r, 0.001, std::numeric_limits<double>::infinity(), rec)) {
        Vec3 view_dir = normalized(r.direction * -1.0);
        Vec3 hit_color(0, 0, 0);

        // ---- Direct illumination: shadow rays to each light ----
        PhongBRDF* phong = dynamic_cast<PhongBRDF*>(rec.material.get());
        if (phong && !world.lights.empty()) {
            for (const auto& light : world.lights) {
                RTLightInfo li = light->illuminate(rec.point);

                // Shadow ray — offset along normal to avoid self-intersection
                Vec3 shadow_orig = rec.point + rec.normal * 0.001;
                Ray shadow_ray(shadow_orig, li.direction);
                HitRecord shadow_rec;
                bool in_shadow = world.hit(shadow_ray, 0.001, li.distance - 0.01, shadow_rec);

                if (!in_shadow) {
                    hit_color = hit_color + phong->evaluate_direct(
                        view_dir, li.direction, rec.normal, li.intensity);
                }
            }
        }

        // ---- Indirect illumination: scatter bounce ----
        Ray scattered;
        Vec3 attenuation;
        if (rec.material && rec.material->scatter(r, rec, attenuation, scattered)) {
            Vec3 indirect = ray_color(scattered, world, depth - 1);
            hit_color = hit_color + Vec3(
                attenuation.x * indirect.x,
                attenuation.y * indirect.y,
                attenuation.z * indirect.z);
        }

        return hit_color;
    }

    // ---- Sky: Nishita atmospheric scattering ----
    Vec3 unit_dir = normalized(r.direction);
    Vec3 sky_hdr = Sky::compute(unit_dir, g_sun_dir);
    return Sky::tonemap(sky_hdr);
}

// ============================================================
// Render Function (offline — writes full image)
// ============================================================

inline void render(TGAImage& image, const Scene& world, const RTCamera& camera,
            int samples, int max_depth) {
    int w = image.width();
    int h = image.height();

    std::vector<TGAColor> pixels(w * h);

    #pragma omp parallel for schedule(dynamic, 4)
    for (int j = h - 1; j >= 0; j--) {
        #ifdef _OPENMP
        if (omp_get_thread_num() == 0)
        #endif
        std::cerr << "\rScanlines remaining: " << j << "  " << std::flush;

        for (int i = 0; i < w; i++) {
            Vec3 color(0, 0, 0);

            for (int s = 0; s < samples; s++) {
                double u = (i + random_double()) / (w - 1);
                double v = (j + random_double()) / (h - 1);
                Ray r = camera.get_ray(u, v);
                color = color + ray_color(r, world, max_depth);
            }

            double scale = 1.0 / samples;
            color = Vec3(std::sqrt(std::max(0.0, color.x * scale)),
                         std::sqrt(std::max(0.0, color.y * scale)),
                         std::sqrt(std::max(0.0, color.z * scale)));

            color.x = std::min(1.0, color.x);
            color.y = std::min(1.0, color.y);
            color.z = std::min(1.0, color.z);

            pixels[j * w + i] = TGAColor(
                static_cast<uint8_t>(255.999 * color.x),
                static_cast<uint8_t>(255.999 * color.y),
                static_cast<uint8_t>(255.999 * color.z)
            );
        }
    }

    for (int j = 0; j < h; j++)
        for (int i = 0; i < w; i++)
            image.set(i, j, pixels[j * w + i]);

    std::cerr << "\rDone!                    " << std::endl;
}

#endif
