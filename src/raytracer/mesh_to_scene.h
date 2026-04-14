#ifndef MESH_TO_SCENE_H
#define MESH_TO_SCENE_H

#include "../scene/editable_mesh.h"
#include "../core/mat.h"
#include "scene.h"
#include "sphere.h"
#include "triangle_hit.h"
#include "material.h"
#include "light.h"
#include "bvh.h"
#include <memory>

// ============================================================
// Convert EditableMesh + transform into a raytracing Scene
// with lights, PhongBRDF materials, and accent objects.
// ============================================================

inline Scene mesh_to_rt_scene(const EditableMesh& mesh, const Mat4& model_mat,
                              const Vec3& mesh_color, double reflectivity) {
    Scene scene;

    // Compute smooth vertex normals
    auto vnormals = mesh.compute_vertex_normals();

    // Transform verts and normals into world space
    std::vector<Vec3> world_verts(mesh.verts.size());
    std::vector<Vec3> world_normals(mesh.verts.size());
    for (size_t i = 0; i < mesh.verts.size(); i++) {
        Vec4 wp = model_mat * embed(mesh.verts[i]);
        world_verts[i] = Vec3(wp.x, wp.y, wp.z);
        Vec4 np = model_mat * Vec4(vnormals[i].x, vnormals[i].y, vnormals[i].z, 0.0);
        world_normals[i] = normalized(Vec3(np.x, np.y, np.z));
    }

    // Main mesh: PhongBRDF for proper specular highlights
    auto mesh_mat = std::make_shared<PhongBRDF>(
        mesh_color,
        0.65,                        // Kd (diffuse weight)
        0.15 + reflectivity * 0.4,   // Ks (specular, scales with slider)
        24.0 + reflectivity * 80.0   // shininess
    );

    for (const auto& tri : mesh.faces) {
        scene.add(std::make_shared<TriangleHit>(
            world_verts[tri[0]], world_verts[tri[1]], world_verts[tri[2]],
            world_normals[tri[0]], world_normals[tri[1]], world_normals[tri[2]],
            mesh_mat));
    }

    // Ground plane — PhongBRDF for light interaction
    auto ground_mat = std::make_shared<PhongBRDF>(Vec3(0.18, 0.18, 0.22), 0.8, 0.05, 8.0);
    double gs = 10.0;
    Vec3 g0(-gs, -1.2, -gs), g1(gs, -1.2, -gs), g2(gs, -1.2, gs), g3(-gs, -1.2, gs);
    Vec3 up(0, 1, 0);
    scene.add(std::make_shared<TriangleHit>(g0, g1, g2, up, up, up, ground_mat));
    scene.add(std::make_shared<TriangleHit>(g0, g2, g3, up, up, up, ground_mat));

    // Glass sphere — refraction + internal reflections
    auto glass_mat = std::make_shared<Dielectric>(1.5);
    scene.add(std::make_shared<Sphere>(Vec3(1.8, -0.6, -0.5), 0.6, glass_mat));

    // Chrome sphere — mirror-like
    auto metal_mat = std::make_shared<Metal>(Vec3(0.85, 0.80, 0.75), 0.02);
    scene.add(std::make_shared<Sphere>(Vec3(-1.6, -0.8, 0.8), 0.4, metal_mat));

    // Small colored sphere
    auto red_mat = std::make_shared<PhongBRDF>(Vec3(0.9, 0.15, 0.15), 0.6, 0.3, 64.0);
    scene.add(std::make_shared<Sphere>(Vec3(0.9, -1.0, 1.2), 0.2, red_mat));

    // ---- Lights ----
    // Key light — warm directional from upper-right
    scene.add_light(std::make_shared<DistantLight>(
        Vec3(-0.5, -0.8, -0.4), Vec3(1.0, 0.95, 0.85), 2.5));

    // Fill light — cool directional from left
    scene.add_light(std::make_shared<DistantLight>(
        Vec3(0.6, -0.3, 0.5), Vec3(0.6, 0.7, 0.9), 1.0));

    // Rim/back light — creates edge highlights
    scene.add_light(std::make_shared<DistantLight>(
        Vec3(0.0, -0.4, 0.9), Vec3(0.9, 0.85, 1.0), 1.5));

    // Point light near the glass sphere for caustic-like highlights
    scene.add_light(std::make_shared<PointLight>(
        Vec3(1.5, 1.0, -0.5), Vec3(1.0, 0.9, 0.7), 25.0));

    return scene;
}

#endif
