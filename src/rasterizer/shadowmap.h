#ifndef SHADOWMAP_H
#define SHADOWMAP_H

#include "../core/vec.h"
#include "../core/mat.h"
#include "zbuffer.h"
#include "triangle.h"
#include "../scene/model.h"

// ============================================================
// Shadow Map Class
// ============================================================

class ShadowMap {
public:
    ZBuffer shadow_buffer;
    Mat4 light_MVP;
    int width, height;

    ShadowMap(int w, int h) : shadow_buffer(w, h), width(w), height(h) {}

    // First pass: render depth from light's POV
    void build(Model& model, const Mat4& light_view,
               const Mat4& light_proj, const Mat4& light_viewport) {
        light_MVP = light_viewport * light_proj * light_view;
        shadow_buffer.clear();

        for (int i = 0; i < model.nfaces(); i++) {
            std::vector<int> face = model.face(i);
            Vec3 pts[3];
            for (int j = 0; j < 3; j++) {
                Vec3 v = model.vert(face[j]);
                pts[j] = proj(light_MVP * embed(v));
            }
            rasterize_depth(pts, shadow_buffer);
        }
    }

    // Check if world position is in shadow
    bool in_shadow(const Vec3& world_pos, double bias = 0.005) const {
        Vec4 light_space = light_MVP * embed(world_pos);
        Vec3 proj_coords = proj(light_space);

        // Map to shadow buffer coordinates
        int x = static_cast<int>(proj_coords.x);
        int y = static_cast<int>(proj_coords.y);

        if (x < 0 || x >= width || y < 0 || y >= height)
            return false;  // Outside light frustum

        double current_depth = proj_coords.z;
        double closest_depth = shadow_buffer.get(x, y);

        // Shadow bias to prevent acne
        return current_depth - bias > closest_depth;
    }

private:
    void rasterize_depth(Vec3 pts[3], ZBuffer& zbuf) {
        Vec2 bboxmin(width - 1, height - 1);
        Vec2 bboxmax(0, 0);

        for (int i = 0; i < 3; i++) {
            bboxmin.x = std::max(0.0, std::min(bboxmin.x, pts[i].x));
            bboxmin.y = std::max(0.0, std::min(bboxmin.y, pts[i].y));
            bboxmax.x = std::min((double)width - 1, std::max(bboxmax.x, pts[i].x));
            bboxmax.y = std::min((double)height - 1, std::max(bboxmax.y, pts[i].y));
        }

        Vec2 pts2d[3] = {Vec2(pts[0].x, pts[0].y), Vec2(pts[1].x, pts[1].y), Vec2(pts[2].x, pts[2].y)};

        for (int x = (int)bboxmin.x; x <= (int)bboxmax.x; x++) {
            for (int y = (int)bboxmin.y; y <= (int)bboxmax.y; y++) {
                Vec3 bc = barycentric(pts2d, Vec2(x, y));
                if (bc.x < 0 || bc.y < 0 || bc.z < 0) continue;
                double z = pts[0].z * bc.x + pts[1].z * bc.y + pts[2].z * bc.z;
                zbuf.test_and_set(x, y, z);
            }
        }
    }
};

// ============================================================
// Shadow Shader - Phong with Shadows
// ============================================================

struct ShadowShader : IShader {
    Mat4 uniform_MVP;
    Mat4 uniform_M;
    Mat4 uniform_MIT;
    Vec3 uniform_light_dir;
    Vec3 uniform_eye;
    ShadowMap* shadow_map = nullptr;

    mat<3, 3> varying_pos;
    mat<3, 3> varying_nrm;

    double ambient = 0.1;
    double diffuse_k = 0.7;
    double specular_k = 0.2;
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

        varying_pos.set_col(nthvert, proj(uniform_M * embed(pos)));
        varying_nrm.set_col(nthvert, normalized(proj(uniform_MIT * embed(normal, 0.0))));

        return uniform_MVP * embed(pos);
    }

    bool fragment(Vec3 bc, TGAColor& color) override {
        Vec3 world_pos = varying_pos * bc;
        Vec3 n = normalized(varying_nrm * bc);

        // Check shadow
        double shadow = (shadow_map && shadow_map->in_shadow(world_pos)) ? 0.3 : 1.0;

        // View direction
        Vec3 v = normalized(uniform_eye - world_pos);

        // Lighting
        double nl = n * uniform_light_dir;
        Vec3 r = normalized(n * (nl * 2.0) - uniform_light_dir);

        double diff = std::max(0.0, nl) * shadow;
        double spec = std::pow(std::max(0.0, r * v), shininess) * shadow;

        double intensity = ambient + diffuse_k * diff + specular_k * spec;
        intensity = std::min(1.0, intensity);

        color = TGAColor(255, 255, 255) * intensity;
        return true;
    }
};

#endif
