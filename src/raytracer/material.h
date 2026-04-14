#ifndef MATERIAL_H
#define MATERIAL_H

#include "../core/vec.h"
#include "ray.h"
#include "hittable.h"
#include <cmath>
#include <algorithm>

// ============================================================
// Material Interface
// ============================================================

struct Material {
    virtual ~Material() = default;
    virtual bool scatter(const Ray& r_in, const HitRecord& rec,
                        Vec3& attenuation, Ray& scattered) const = 0;
};

// ============================================================
// Lambertian (Diffuse) Material
// ============================================================

struct Lambertian : Material {
    Vec3 albedo;

    Lambertian(const Vec3& albedo) : albedo(albedo) {}

    bool scatter(const Ray& r_in, const HitRecord& rec,
                Vec3& attenuation, Ray& scattered) const override {
        Vec3 scatter_dir = rec.normal + random_unit_vector();

        // Catch degenerate scatter direction
        if (scatter_dir.norm2() < 1e-8)
            scatter_dir = rec.normal;

        scattered = Ray(rec.point, scatter_dir);
        attenuation = albedo;
        return true;
    }
};

// ============================================================
// Metal (Reflective) Material
// ============================================================

struct Metal : Material {
    Vec3 albedo;
    double fuzz;

    Metal(const Vec3& albedo, double fuzz = 0.0)
        : albedo(albedo), fuzz(fuzz < 1 ? fuzz : 1) {}

    bool scatter(const Ray& r_in, const HitRecord& rec,
                Vec3& attenuation, Ray& scattered) const override {
        Vec3 reflected = r_in.direction - rec.normal * (2.0 * (r_in.direction * rec.normal));
        scattered = Ray(rec.point, reflected + random_in_unit_sphere() * fuzz);
        attenuation = albedo;
        return (scattered.direction * rec.normal) > 0;
    }
};

// ============================================================
// Dielectric (Glass) Material
// ============================================================

struct Dielectric : Material {
    double refraction_index;

    Dielectric(double ri) : refraction_index(ri) {}

    bool scatter(const Ray& r_in, const HitRecord& rec,
                Vec3& attenuation, Ray& scattered) const override {
        attenuation = Vec3(1, 1, 1);
        double ri = rec.front_face ? (1.0 / refraction_index) : refraction_index;

        double cos_theta = std::min(-(r_in.direction * rec.normal), 1.0);
        double sin_theta = std::sqrt(1.0 - cos_theta * cos_theta);

        bool cannot_refract = ri * sin_theta > 1.0;
        Vec3 direction;

        if (cannot_refract || schlick(cos_theta, ri) > random_double()) {
            // Reflect
            direction = r_in.direction - rec.normal * (2.0 * (r_in.direction * rec.normal));
        } else {
            // Refract
            Vec3 r_out_perp = (r_in.direction + rec.normal * cos_theta) * ri;
            Vec3 r_out_parallel = rec.normal * (-std::sqrt(std::abs(1.0 - r_out_perp.norm2())));
            direction = r_out_perp + r_out_parallel;
        }

        scattered = Ray(rec.point, direction);
        return true;
    }

    static double schlick(double cosine, double ri) {
        double r0 = (1 - ri) / (1 + ri);
        r0 = r0 * r0;
        return r0 + (1 - r0) * std::pow(1 - cosine, 5);
    }
};

// ============================================================
// Phong BRDF Material — diffuse + specular + indirect bounce
// Adapted from scratchapixel phong-shader-BRDF
// ============================================================

struct PhongBRDF : Material {
    Vec3 albedo;
    double Kd;         // diffuse weight
    double Ks;         // specular weight
    double shininess;  // Phong exponent

    PhongBRDF(const Vec3& albedo, double Kd = 0.7, double Ks = 0.3, double shininess = 32.0)
        : albedo(albedo), Kd(Kd), Ks(Ks), shininess(shininess) {}

    // scatter() provides the indirect bounce (GI contribution)
    bool scatter(const Ray& r_in, const HitRecord& rec,
                Vec3& attenuation, Ray& scattered) const override {
        (void)r_in;
        Vec3 scatter_dir = rec.normal + random_unit_vector();
        if (scatter_dir.norm2() < 1e-8) scatter_dir = rec.normal;
        scattered = Ray(rec.point, scatter_dir);
        attenuation = albedo * Kd;
        return true;
    }

    // Direct illumination evaluation (called from ray_color with lights)
    Vec3 evaluate_direct(const Vec3& view_dir, const Vec3& light_dir,
                         const Vec3& normal, const Vec3& light_intensity) const {
        double ndl = std::max(0.0, normal * light_dir);
        if (ndl <= 0) return Vec3(0, 0, 0);

        // Diffuse
        Vec3 diffuse(albedo.x * light_intensity.x * ndl,
                     albedo.y * light_intensity.y * ndl,
                     albedo.z * light_intensity.z * ndl);

        // Specular (Blinn-Phong half-vector)
        Vec3 half_v = normalized(light_dir + view_dir);
        double ndh = std::max(0.0, normal * half_v);
        double spec = std::pow(ndh, shininess);
        Vec3 specular = light_intensity * spec;

        return diffuse * Kd + specular * Ks;
    }
};

#endif
