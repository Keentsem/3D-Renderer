#ifndef PHONG_SHADER_H
#define PHONG_SHADER_H

#include "../core/vec.h"
#include "../image/tgaimage.h"
#include "triangle.h"
#include "zbuffer.h"
#include <cmath>
#include <algorithm>

// ============================================================
// PhongMaterial — everything the pixel shader needs per draw
// ============================================================
struct PhongMaterial {
    Vec3     base_color  = Vec3(0.9, 0.7, 0.3);   // linear-ish 0..1
    double   ambient     = 0.14;
    double   diffuse     = 0.8;
    double   specular    = 0.55;
    double   shininess   = 48.0;

    // Rim (fresnel-ish) — pops silhouettes.
    double   rim_strength = 0.55;
    double   rim_power    = 2.4;
    Vec3     rim_color    = Vec3(0.85, 0.9, 1.0);

    // Procedural surface pattern — triplanar stripes + checker.
    // 0 = off, 1 = stripes, 2 = checker, 3 = grid
    int      pattern      = 2;
    double   pattern_scale = 4.0;
    double   pattern_contrast = 0.35;

    // Depth fog — fade toward a sky color as things recede.
    Vec3     fog_color     = Vec3(0.04, 0.06, 0.10);
    double   fog_near      = 2.5;
    double   fog_far       = 12.0;
    double   fog_strength  = 0.55;
};

// ============================================================
// PhongLight — single directional with a configurable fill
// ============================================================
struct PhongLight {
    Vec3 key_dir   = normalized(Vec3(0.6, 1.0, 0.5));
    Vec3 key_color = Vec3(1.0, 0.95, 0.85);

    Vec3 fill_dir   = normalized(Vec3(-0.4, 0.3, -0.6));
    Vec3 fill_color = Vec3(0.28, 0.38, 0.55);  // cool fill from behind

    Vec3 ambient_color = Vec3(0.10, 0.13, 0.18);
};

// ============================================================
// Procedural surface pattern — triplanar so it doesn't need UVs.
// Returns a scalar in [0, 1] that you multiply the base color by.
// ============================================================
inline double triplanar_pattern(const Vec3& world, const Vec3& n, int mode,
                                double scale, double contrast) {
    if (mode == 0) return 1.0;

    // Pick the dominant axis for this surface so the pattern never stretches.
    Vec3 an(std::abs(n.x), std::abs(n.y), std::abs(n.z));
    double u, v;
    if (an.x > an.y && an.x > an.z) { u = world.y * scale; v = world.z * scale; }
    else if (an.y > an.z)            { u = world.x * scale; v = world.z * scale; }
    else                             { u = world.x * scale; v = world.y * scale; }

    double p = 1.0;
    if (mode == 1) {
        // Soft stripes
        double s = 0.5 + 0.5 * std::sin(u * 3.14159);
        p = 1.0 - contrast + contrast * s;
    } else if (mode == 2) {
        // Checker (antialiased a bit via smoothstep on fract)
        double fu = u - std::floor(u);
        double fv = v - std::floor(v);
        double cu = (fu < 0.5) ? 0.0 : 1.0;
        double cv = (fv < 0.5) ? 0.0 : 1.0;
        double k  = (cu == cv) ? 1.0 : 1.0 - contrast;
        p = k;
    } else if (mode == 3) {
        // Grid lines
        double fu = u - std::floor(u);
        double fv = v - std::floor(v);
        double du = std::min(fu, 1 - fu);
        double dv = std::min(fv, 1 - fv);
        double line = std::min(du, dv);
        double w = std::min(1.0, line * 12.0);
        p = 1.0 - contrast + contrast * w;
    }
    return p;
}

// ============================================================
// Per-pixel Phong + rim + procedural surface + fog
// Takes screen-space triangle verts AND matching world positions +
// world-space normals (one per vertex). Rasterizes with z-test, does
// full shading at each pixel.
// ============================================================
inline void triangle_phong(const Vec3 scr[3],
                           const Vec3 world[3],
                           const Vec3 nrm[3],
                           const Vec3& eye,
                           const PhongLight& L,
                           const PhongMaterial& M,
                           ZBuffer& zb, TGAImage& image) {
    // Bounding box
    Vec2 bmin(image.width() - 1, image.height() - 1);
    Vec2 bmax(0, 0);
    for (int i = 0; i < 3; i++) {
        bmin.x = std::max(0.0, std::min(bmin.x, scr[i].x));
        bmin.y = std::max(0.0, std::min(bmin.y, scr[i].y));
        bmax.x = std::min((double)image.width()  - 1, std::max(bmax.x, scr[i].x));
        bmax.y = std::min((double)image.height() - 1, std::max(bmax.y, scr[i].y));
    }

    Vec2 pts2d[3] = {
        Vec2(scr[0].x, scr[0].y),
        Vec2(scr[1].x, scr[1].y),
        Vec2(scr[2].x, scr[2].y),
    };

    for (int y = (int)bmin.y; y <= (int)bmax.y; y++) {
        for (int x = (int)bmin.x; x <= (int)bmax.x; x++) {
            Vec2 P(x, y);
            Vec3 bc = barycentric(pts2d, P);
            if (bc.x < 0 || bc.y < 0 || bc.z < 0) continue;

            double z = scr[0].z * bc.x + scr[1].z * bc.y + scr[2].z * bc.z;
            if (!zb.test_and_set(x, y, z)) continue;

            // Interpolate world pos + normal
            Vec3 wp = world[0] * bc.x + world[1] * bc.y + world[2] * bc.z;
            Vec3 nn = nrm[0]   * bc.x + nrm[1]   * bc.y + nrm[2]   * bc.z;
            double nlen = nn.norm();
            if (nlen > 1e-9) nn = nn / nlen;

            // View direction
            Vec3 view = eye - wp;
            double vlen = view.norm();
            Vec3 V = (vlen > 1e-9) ? view / vlen : Vec3(0, 0, 1);

            // -------- Key light (Blinn-Phong)
            double ndl_k = std::max(0.0, nn * L.key_dir);
            Vec3   h_k   = L.key_dir + V;
            double hlen  = h_k.norm();
            if (hlen > 1e-9) h_k = h_k / hlen;
            double ndh_k = std::max(0.0, nn * h_k);
            double spec  = std::pow(ndh_k, M.shininess) * M.specular;

            // -------- Fill light (diffuse only, cool tint)
            double ndl_f = std::max(0.0, nn * L.fill_dir);

            // -------- Rim (fresnel-ish)
            double rim = 1.0 - std::max(0.0, nn * V);
            rim = std::pow(rim, M.rim_power) * M.rim_strength;

            // -------- Procedural surface
            double pat = triplanar_pattern(wp, nn, M.pattern,
                                           M.pattern_scale, M.pattern_contrast);

            // -------- Compose (componentwise)
            Vec3 light_sum = M.ambient  * L.ambient_color
                           + (M.diffuse * ndl_k) * L.key_color
                           + (0.45      * ndl_f) * L.fill_color;
            Vec3 base = M.base_color * pat;
            Vec3 diff(base.x * light_sum.x,
                      base.y * light_sum.y,
                      base.z * light_sum.z);

            Vec3 col = diff
                     + L.key_color * spec
                     + M.rim_color * rim;

            // -------- Depth fog (distance from eye)
            double d = (wp - eye).norm();
            double t = (d - M.fog_near) / std::max(1e-6, (M.fog_far - M.fog_near));
            t = std::max(0.0, std::min(1.0, t)) * M.fog_strength;
            col = col * (1 - t) + M.fog_color * t;

            // Clamp + write
            auto c8 = [](double v) {
                if (v < 0) v = 0; if (v > 1) v = 1;
                return (uint8_t)(v * 255.0);
            };
            image.set(x, y, TGAColor(c8(col.x), c8(col.y), c8(col.z)));
        }
    }
}

#endif
