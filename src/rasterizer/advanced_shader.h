#ifndef ADVANCED_SHADER_H
#define ADVANCED_SHADER_H

#include "../core/vec.h"
#include "../core/mat.h"
#include "../image/tgaimage.h"
#include "shader.h"
#include "shadowmap.h"
#include <cmath>
#include <algorithm>

// ============================================================
// Normal Mapping Shader with TBN
// ============================================================

struct NormalMapShader : IShader {
    Mat4 uniform_MVP;
    Mat4 uniform_M;
    Mat4 uniform_MIT;
    Vec3 uniform_light_dir;
    Vec3 uniform_eye;
    ShadowMap* shadow_map = nullptr;

    mat<2, 3> varying_uv;
    mat<3, 3> varying_nrm;
    mat<3, 3> varying_pos;
    mat<3, 3> ndc_tri;  // For computing tangent

    double ambient = 0.1;
    double diffuse_k = 0.7;
    double specular_k = 0.3;

    Model* model = nullptr;

    Vec4 vertex(int iface, int nthvert) override {
        std::vector<int> face = model->face(iface);
        Vec3 pos = model->vert(face[nthvert]);

        // Get normal
        Vec3 normal;
        if (model->has_normals()) {
            normal = model->normal(iface, nthvert);
        } else {
            Vec3 v0 = model->vert(face[0]);
            Vec3 v1 = model->vert(face[1]);
            Vec3 v2 = model->vert(face[2]);
            normal = normalized(cross(v1 - v0, v2 - v0));
        }

        // Get UV
        Vec2 uv(0.5, 0.5);
        if (model->has_uvs()) {
            uv = model->uv(iface, nthvert);
        }

        varying_uv[0][nthvert] = uv.x;
        varying_uv[1][nthvert] = uv.y;
        varying_pos.set_col(nthvert, proj(uniform_M * embed(pos)));
        varying_nrm.set_col(nthvert, normalized(proj(uniform_MIT * embed(normal, 0.0))));

        Vec4 clip = uniform_MVP * embed(pos);
        ndc_tri.set_col(nthvert, proj(clip));

        return clip;
    }

    bool fragment(Vec3 bc, TGAColor& color) override {
        // Interpolate UV
        double u = varying_uv[0][0] * bc.x + varying_uv[0][1] * bc.y + varying_uv[0][2] * bc.z;
        double v = varying_uv[1][0] * bc.x + varying_uv[1][1] * bc.y + varying_uv[1][2] * bc.z;
        Vec2 uv(u, v);

        // Interpolate world position and geometric normal
        Vec3 world_pos = varying_pos * bc;
        Vec3 bn = normalized(varying_nrm * bc);

        // Compute TBN matrix from UV derivatives
        Vec3 e1 = varying_pos.col(1) - varying_pos.col(0);
        Vec3 e2 = varying_pos.col(2) - varying_pos.col(0);

        double du1 = varying_uv[0][1] - varying_uv[0][0];
        double dv1 = varying_uv[1][1] - varying_uv[1][0];
        double du2 = varying_uv[0][2] - varying_uv[0][0];
        double dv2 = varying_uv[1][2] - varying_uv[1][0];

        double det = du1 * dv2 - du2 * dv1;

        Vec3 n;
        if (std::abs(det) < 1e-6) {
            // Fallback to geometric normal
            n = bn;
        } else {
            // Compute tangent and bitangent
            double inv_det = 1.0 / det;
            Vec3 T = (e1 * dv2 - e2 * dv1) * inv_det;
            Vec3 B = (e2 * du1 - e1 * du2) * inv_det;

            // Gram-Schmidt orthogonalize
            T = normalized(T - bn * (bn * T));
            B = normalized(cross(bn, T));

            // Sample normal from normal map
            Vec3 n_tangent = model->normal_from_map(uv);

            // Transform to world space
            n.x = T.x * n_tangent.x + B.x * n_tangent.y + bn.x * n_tangent.z;
            n.y = T.y * n_tangent.x + B.y * n_tangent.y + bn.y * n_tangent.z;
            n.z = T.z * n_tangent.x + B.z * n_tangent.y + bn.z * n_tangent.z;
            n = normalized(n);
        }

        // Check shadow
        double shadow = 1.0;
        if (shadow_map) {
            shadow = shadow_map->in_shadow(world_pos) ? 0.3 : 1.0;
        }

        // View direction
        Vec3 view_dir = normalized(uniform_eye - world_pos);

        // Lighting (Blinn-Phong)
        double nl = n * uniform_light_dir;
        double diff = std::max(0.0, nl) * shadow;

        // Specular with specular map
        Vec3 h = normalized(uniform_light_dir + view_dir);
        double spec_power = 5.0 + model->specular(uv) * 95.0;  // Range 5-100
        double spec = std::pow(std::max(0.0, n * h), spec_power) * shadow;

        // Combine
        double intensity = ambient + diffuse_k * diff + specular_k * spec;
        intensity = std::min(1.0, intensity);

        // Sample diffuse texture
        TGAColor tex = model->diffuse(uv);
        color = tex * intensity;

        return true;
    }
};

// ============================================================
// Comparison Shader (No Normal Map)
// ============================================================

struct TexturedPhongShader : IShader {
    Mat4 uniform_MVP;
    Mat4 uniform_M;
    Mat4 uniform_MIT;
    Vec3 uniform_light_dir;
    Vec3 uniform_eye;
    ShadowMap* shadow_map = nullptr;

    mat<2, 3> varying_uv;
    mat<3, 3> varying_nrm;
    mat<3, 3> varying_pos;

    double ambient = 0.1;
    double diffuse_k = 0.7;
    double specular_k = 0.3;
    double shininess = 32.0;

    Model* model = nullptr;

    Vec4 vertex(int iface, int nthvert) override {
        std::vector<int> face = model->face(iface);
        Vec3 pos = model->vert(face[nthvert]);

        Vec3 normal;
        if (model->has_normals()) {
            normal = model->normal(iface, nthvert);
        } else {
            Vec3 v0 = model->vert(face[0]);
            Vec3 v1 = model->vert(face[1]);
            Vec3 v2 = model->vert(face[2]);
            normal = normalized(cross(v1 - v0, v2 - v0));
        }

        Vec2 uv(0.5, 0.5);
        if (model->has_uvs()) {
            uv = model->uv(iface, nthvert);
        }

        varying_uv[0][nthvert] = uv.x;
        varying_uv[1][nthvert] = uv.y;
        varying_pos.set_col(nthvert, proj(uniform_M * embed(pos)));
        varying_nrm.set_col(nthvert, normalized(proj(uniform_MIT * embed(normal, 0.0))));

        return uniform_MVP * embed(pos);
    }

    bool fragment(Vec3 bc, TGAColor& color) override {
        double u = varying_uv[0][0] * bc.x + varying_uv[0][1] * bc.y + varying_uv[0][2] * bc.z;
        double v = varying_uv[1][0] * bc.x + varying_uv[1][1] * bc.y + varying_uv[1][2] * bc.z;
        Vec2 uv(u, v);

        Vec3 world_pos = varying_pos * bc;
        Vec3 n = normalized(varying_nrm * bc);

        // Check shadow
        double shadow = 1.0;
        if (shadow_map) {
            shadow = shadow_map->in_shadow(world_pos) ? 0.3 : 1.0;
        }

        Vec3 view_dir = normalized(uniform_eye - world_pos);

        double nl = n * uniform_light_dir;
        double diff = std::max(0.0, nl) * shadow;

        Vec3 r = normalized(n * (nl * 2.0) - uniform_light_dir);
        double spec = std::pow(std::max(0.0, r * view_dir), shininess) * shadow;

        double intensity = ambient + diffuse_k * diff + specular_k * spec;
        intensity = std::min(1.0, intensity);

        TGAColor tex = model->diffuse(uv);
        color = tex * intensity;

        return true;
    }
};

#endif
