#ifndef SHADER_H
#define SHADER_H

#include "../core/vec.h"
#include "../core/mat.h"
#include "../image/tgaimage.h"
#include "zbuffer.h"
#include "triangle.h"
#include "../scene/model.h"
#include <algorithm>
#include <cmath>

// ============================================================
// Shader Interface
// ============================================================

struct IShader {
    virtual ~IShader() = default;

    // Vertex shader: called per vertex
    // Returns clip-space position (before perspective divide)
    virtual Vec4 vertex(int iface, int nthvert) = 0;

    // Fragment shader: called per pixel
    // bc: barycentric coordinates for interpolation
    // Returns false to discard pixel
    virtual bool fragment(Vec3 bc, TGAColor& color) = 0;
};

// ============================================================
// Triangle Rasterization with Shaders
// ============================================================

void triangle_shader(Vec4 clip_verts[3], IShader& shader,
                     TGAImage& image, ZBuffer& zbuf) {
    // Perspective divide to NDC
    Vec3 pts[3];
    for (int i = 0; i < 3; i++) {
        pts[i] = proj(clip_verts[i]);
    }

    // Bounding box
    Vec2 bboxmin(image.width() - 1, image.height() - 1);
    Vec2 bboxmax(0, 0);

    for (int i = 0; i < 3; i++) {
        bboxmin.x = std::max(0.0, std::min(bboxmin.x, pts[i].x));
        bboxmin.y = std::max(0.0, std::min(bboxmin.y, pts[i].y));
        bboxmax.x = std::min((double)image.width() - 1, std::max(bboxmax.x, pts[i].x));
        bboxmax.y = std::min((double)image.height() - 1, std::max(bboxmax.y, pts[i].y));
    }

    Vec2 pts2d[3] = {Vec2(pts[0].x, pts[0].y), Vec2(pts[1].x, pts[1].y), Vec2(pts[2].x, pts[2].y)};

    // Backface culling: skip triangles with non-positive signed area (facing away)
    double signed_area = (pts2d[1].x - pts2d[0].x) * (pts2d[2].y - pts2d[0].y)
                       - (pts2d[2].x - pts2d[0].x) * (pts2d[1].y - pts2d[0].y);
    if (signed_area <= 0) return;

    for (int x = (int)bboxmin.x; x <= (int)bboxmax.x; x++) {
        for (int y = (int)bboxmin.y; y <= (int)bboxmax.y; y++) {
            Vec3 bc_screen = barycentric(pts2d, Vec2(x, y));

            if (bc_screen.x < 0 || bc_screen.y < 0 || bc_screen.z < 0) continue;

            // Perspective-correct interpolation
            Vec3 bc_clip(
                bc_screen.x / clip_verts[0].w,
                bc_screen.y / clip_verts[1].w,
                bc_screen.z / clip_verts[2].w
            );
            bc_clip = bc_clip / (bc_clip.x + bc_clip.y + bc_clip.z);

            double z = pts[0].z * bc_screen.x + pts[1].z * bc_screen.y + pts[2].z * bc_screen.z;

            if (zbuf.test_and_set(x, y, z)) {
                TGAColor color;
                if (shader.fragment(bc_clip, color)) {
                    image.set(x, y, color);
                }
            }
        }
    }
}

// ============================================================
// Gouraud Shader - Per-Vertex Lighting
// ============================================================

struct GouraudShader : IShader {
    // Uniforms (constant for all vertices)
    Mat4 uniform_MVP;
    Mat4 uniform_MIT;        // (ModelView)^{-T} for normals
    Vec3 uniform_light_dir;

    // Varying (computed per-vertex, interpolated)
    Vec3 varying_intensity;  // Light intensity at each vertex

    Model* model = nullptr;
    int current_face = 0;

    Vec4 vertex(int iface, int nthvert) override {
        current_face = iface;

        // Get vertex data
        std::vector<int> face = model->face(iface);
        Vec3 pos = model->vert(face[nthvert]);

        // Get normal (if available, else compute from face)
        Vec3 normal;
        if (model->has_normals()) {
            normal = model->normal(iface, nthvert);
        } else {
            // Compute face normal
            Vec3 v0 = model->vert(face[0]);
            Vec3 v1 = model->vert(face[1]);
            Vec3 v2 = model->vert(face[2]);
            normal = normalized(cross(v1 - v0, v2 - v0));
        }

        // Transform normal to world space
        Vec3 n = normalized(proj(uniform_MIT * embed(normal, 0.0)));

        // Compute diffuse intensity at vertex
        varying_intensity[nthvert] = std::max(0.0, n * uniform_light_dir);

        // Return clip-space position
        return uniform_MVP * embed(pos);
    }

    bool fragment(Vec3 bc, TGAColor& color) override {
        // Interpolate intensity
        double intensity = varying_intensity.x * bc.x +
                           varying_intensity.y * bc.y +
                           varying_intensity.z * bc.z;

        color = TGAColor(255, 255, 255) * intensity;
        return true;
    }
};

// ============================================================
// Phong Shader - Per-Pixel Lighting
// ============================================================

struct PhongShader : IShader {
    // Uniforms
    Mat4 uniform_MVP;
    Mat4 uniform_M;          // Model matrix (for world position)
    Mat4 uniform_MIT;
    Vec3 uniform_light_dir;
    Vec3 uniform_eye;        // Camera position

    // Material
    double ambient = 0.1;
    double diffuse_k = 0.7;
    double specular_k = 0.2;
    double shininess = 32.0;

    // Varying
    mat<3, 3> varying_pos;   // World position (columns = vertices)
    mat<3, 3> varying_nrm;   // World normal

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

        // Store world-space position and normal
        varying_pos.set_col(nthvert, proj(uniform_M * embed(pos)));
        varying_nrm.set_col(nthvert, normalized(proj(uniform_MIT * embed(normal, 0.0))));

        return uniform_MVP * embed(pos);
    }

    bool fragment(Vec3 bc, TGAColor& color) override {
        // Interpolate position and normal
        Vec3 pos = varying_pos * bc;
        Vec3 n = normalized(varying_nrm * bc);

        // View direction
        Vec3 v = normalized(uniform_eye - pos);

        // Reflect light around normal: R = 2(N·L)N - L
        double nl = n * uniform_light_dir;
        Vec3 r = normalized(n * (nl * 2.0) - uniform_light_dir);

        // Phong lighting
        double diff = std::max(0.0, nl);
        double spec = std::pow(std::max(0.0, r * v), shininess);

        double intensity = ambient + diffuse_k * diff + specular_k * spec;
        intensity = std::min(1.0, intensity);

        color = TGAColor(255, 255, 255) * intensity;
        return true;
    }
};

// ============================================================
// Textured Shader - Texture Mapping with Lighting
// ============================================================

struct TexturedShader : IShader {
    Mat4 uniform_MVP;
    Mat4 uniform_MIT;
    Vec3 uniform_light_dir;

    Vec3 varying_intensity;
    mat<2, 3> varying_uv;    // UV coords at each vertex

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

        Vec3 n = normalized(proj(uniform_MIT * embed(normal, 0.0)));
        varying_intensity[nthvert] = std::max(0.0, n * uniform_light_dir);

        // Store UV for interpolation
        if (model->has_uvs()) {
            Vec2 uv = model->uv(iface, nthvert);
            varying_uv[0][nthvert] = uv.x;
            varying_uv[1][nthvert] = uv.y;
        } else {
            // Default UVs if not available
            varying_uv[0][nthvert] = 0.5;
            varying_uv[1][nthvert] = 0.5;
        }

        return uniform_MVP * embed(pos);
    }

    bool fragment(Vec3 bc, TGAColor& color) override {
        // Interpolate UV
        double u = varying_uv[0][0] * bc.x + varying_uv[0][1] * bc.y + varying_uv[0][2] * bc.z;
        double v = varying_uv[1][0] * bc.x + varying_uv[1][1] * bc.y + varying_uv[1][2] * bc.z;

        // Interpolate intensity
        double intensity = varying_intensity.x * bc.x +
                           varying_intensity.y * bc.y +
                           varying_intensity.z * bc.z;

        // Sample texture
        TGAColor tex = model->diffuse(Vec2(u, v));
        color = tex * intensity;

        return true;
    }
};

#endif
