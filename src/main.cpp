#include "core/vec.h"
#include "core/mat.h"
#include "image/tgaimage.h"
#include "rasterizer/line.h"
#include "rasterizer/triangle.h"
#include "rasterizer/zbuffer.h"
#include "rasterizer/shader.h"
#include "rasterizer/shadowmap.h"
#include "rasterizer/ssao.h"
#include "rasterizer/advanced_shader.h"
#include "transform/transform.h"
#include "scene/model.h"
#include <iostream>
#include <cmath>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

const int WIDTH = 800;
const int HEIGHT = 800;

int main() {
    std::cout << "=== Phase 9: Advanced Features ===" << std::endl;
    std::cout << "Implementing shadow mapping, SSAO, and normal mapping...\n" << std::endl;

    // Load model
    std::cout << "Loading cube model..." << std::endl;
    Model model("models/cube.obj");

    if (model.nfaces() == 0) {
        std::cerr << "Error: No faces loaded!" << std::endl;
        return 1;
    }

    // Camera setup
    Vec3 eye(2, 2, 3);
    Vec3 center(0, 0, 0);
    Vec3 up(0, 1, 0);
    Vec3 light_dir = normalized(Vec3(1, 1, 1));

    // Transformation matrices
    Mat4 Model_mat = rotate_y(0.5) * rotate_x(0.3);
    Mat4 View = lookat(eye, center, up);
    Mat4 Projection = perspective(-1.0 / (eye - center).norm());
    Mat4 Viewport_mat = viewport(WIDTH/8, HEIGHT/8, WIDTH*3/4, HEIGHT*3/4);

    Mat4 MVP = Viewport_mat * Projection * View * Model_mat;
    Mat4 MV = View * Model_mat;
    Mat4 MIT = transpose(inverse(MV));

    // Test 1: Shadow Mapping
    std::cout << "\nTest 1: Shadow Mapping..." << std::endl;
    {
        TGAImage image(WIDTH, HEIGHT, TGAImage::RGB);
        ZBuffer zbuf(WIDTH, HEIGHT);

        // Build shadow map from light's perspective
        std::cout << "  Building shadow map..." << std::endl;
        ShadowMap shadows(512, 512);

        Vec3 light_pos = light_dir * 5.0;
        Mat4 light_view = lookat(light_pos, center, Vec3(0, 1, 0));
        Mat4 light_proj = perspective(-0.2);
        Mat4 light_viewport = viewport(0, 0, 512, 512);

        shadows.build(model, light_view, light_proj, light_viewport);
        std::cout << "  Shadow map built!" << std::endl;

        // Render with shadows
        ShadowShader shader;
        shader.model = &model;
        shader.uniform_MVP = MVP;
        shader.uniform_M = Model_mat;
        shader.uniform_MIT = MIT;
        shader.uniform_light_dir = light_dir;
        shader.uniform_eye = eye;
        shader.shadow_map = &shadows;

        std::cout << "  Rendering with shadows..." << std::endl;
        for (int i = 0; i < model.nfaces(); i++) {
            Vec4 clip_verts[3];
            for (int j = 0; j < 3; j++) {
                clip_verts[j] = shader.vertex(i, j);
            }
            triangle_shader(clip_verts, shader, image, zbuf);
        }

        image.flip_vertically();
        image.write_tga_file("output/advanced_shadows.tga");
        std::cout << "  ✓ Saved to output/advanced_shadows.tga" << std::endl;
    }

    // Test 2: SSAO (Screen-Space Ambient Occlusion)
    std::cout << "\nTest 2: Screen-Space Ambient Occlusion..." << std::endl;
    {
        TGAImage image(WIDTH, HEIGHT, TGAImage::RGB);
        ZBuffer zbuf(WIDTH, HEIGHT);

        // Render base image
        PhongShader shader;
        shader.model = &model;
        shader.uniform_MVP = MVP;
        shader.uniform_M = Model_mat;
        shader.uniform_MIT = MIT;
        shader.uniform_light_dir = light_dir;
        shader.uniform_eye = eye;

        std::cout << "  Rendering base image..." << std::endl;
        for (int i = 0; i < model.nfaces(); i++) {
            Vec4 clip_verts[3];
            for (int j = 0; j < 3; j++) {
                clip_verts[j] = shader.vertex(i, j);
            }
            triangle_shader(clip_verts, shader, image, zbuf);
        }

        // Save without SSAO
        TGAImage no_ao = image;
        no_ao.flip_vertically();
        no_ao.write_tga_file("output/advanced_no_ssao.tga");

        // Apply SSAO
        std::cout << "  Applying SSAO..." << std::endl;
        SSAO::apply(image, zbuf, 5);

        image.flip_vertically();
        image.write_tga_file("output/advanced_with_ssao.tga");
        std::cout << "  ✓ Saved comparison:" << std::endl;
        std::cout << "    - output/advanced_no_ssao.tga (without SSAO)" << std::endl;
        std::cout << "    - output/advanced_with_ssao.tga (with SSAO)" << std::endl;

        // SSAO visualization
        TGAImage ao_viz(WIDTH, HEIGHT, TGAImage::RGB);
        SSAO::visualize(ao_viz, zbuf, 5);
        ao_viz.flip_vertically();
        ao_viz.write_tga_file("output/advanced_ssao_viz.tga");
        std::cout << "    - output/advanced_ssao_viz.tga (SSAO visualization)" << std::endl;
    }

    // Test 3: Combined Features
    std::cout << "\nTest 3: Combined (Shadows + SSAO)..." << std::endl;
    {
        TGAImage image(WIDTH, HEIGHT, TGAImage::RGB);
        ZBuffer zbuf(WIDTH, HEIGHT);

        // Build shadow map
        ShadowMap shadows(512, 512);
        Vec3 light_pos = light_dir * 5.0;
        Mat4 light_view = lookat(light_pos, center, Vec3(0, 1, 0));
        Mat4 light_proj = perspective(-0.2);
        Mat4 light_viewport = viewport(0, 0, 512, 512);
        shadows.build(model, light_view, light_proj, light_viewport);

        // Render with shadows
        ShadowShader shader;
        shader.model = &model;
        shader.uniform_MVP = MVP;
        shader.uniform_M = Model_mat;
        shader.uniform_MIT = MIT;
        shader.uniform_light_dir = light_dir;
        shader.uniform_eye = eye;
        shader.shadow_map = &shadows;

        for (int i = 0; i < model.nfaces(); i++) {
            Vec4 clip_verts[3];
            for (int j = 0; j < 3; j++) {
                clip_verts[j] = shader.vertex(i, j);
            }
            triangle_shader(clip_verts, shader, image, zbuf);
        }

        // Apply SSAO
        SSAO::apply(image, zbuf, 5);

        image.flip_vertically();
        image.write_tga_file("output/advanced_combined.tga");
        std::cout << "  ✓ Saved to output/advanced_combined.tga" << std::endl;
    }

    // Test 4: Multiple cubes scene with shadows
    std::cout << "\nTest 4: Scene with Multiple Objects..." << std::endl;
    {
        const int SCENE_WIDTH = 1000;
        const int SCENE_HEIGHT = 800;
        TGAImage image(SCENE_WIDTH, SCENE_HEIGHT, TGAImage::RGB);
        ZBuffer zbuf(SCENE_WIDTH, SCENE_HEIGHT);

        // Different viewport for wider scene
        Mat4 scene_viewport = viewport(SCENE_WIDTH/8, SCENE_HEIGHT/8,
                                       SCENE_WIDTH*3/4, SCENE_HEIGHT*3/4);

        // Build shadow map
        ShadowMap shadows(1024, 1024);
        Vec3 light_pos = light_dir * 8.0;
        Mat4 light_view = lookat(light_pos, center, Vec3(0, 1, 0));
        Mat4 light_proj = perspective(-0.1);
        Mat4 light_viewport = viewport(0, 0, 1024, 1024);

        // We'll render shadow map for all cubes
        Vec3 cube_positions[3] = {
            Vec3(-1.5, 0, 0),
            Vec3(0, 0, 0),
            Vec3(1.5, 0, 0)
        };
        double rotations[3] = {0.2, 0.5, 0.8};
        double scales[3] = {0.6, 1.0, 0.8};

        // Build combined shadow map
        shadows.shadow_buffer.clear();
        for (int cube = 0; cube < 3; cube++) {
            Mat4 cube_model = translate(cube_positions[cube]) *
                             rotate_y(rotations[cube]) *
                             rotate_x(rotations[cube] * 0.5) *
                             scale(scales[cube]);

            // Create temp model matrices for shadow pass
            Mat4 shadow_mvp = light_viewport * light_proj * light_view * cube_model;

            // Render to shadow map
            for (int i = 0; i < model.nfaces(); i++) {
                std::vector<int> face = model.face(i);
                Vec3 pts[3];
                for (int j = 0; j < 3; j++) {
                    Vec3 v = model.vert(face[j]);
                    pts[j] = proj(shadow_mvp * embed(v));
                }
                // Manually rasterize depth
                Vec2 bboxmin(1024 - 1, 1024 - 1);
                Vec2 bboxmax(0, 0);
                for (int k = 0; k < 3; k++) {
                    bboxmin.x = std::max(0.0, std::min(bboxmin.x, pts[k].x));
                    bboxmin.y = std::max(0.0, std::min(bboxmin.y, pts[k].y));
                    bboxmax.x = std::min(1023.0, std::max(bboxmax.x, pts[k].x));
                    bboxmax.y = std::min(1023.0, std::max(bboxmax.y, pts[k].y));
                }
                Vec2 pts2d[3] = {Vec2(pts[0].x, pts[0].y), Vec2(pts[1].x, pts[1].y), Vec2(pts[2].x, pts[2].y)};
                for (int x = (int)bboxmin.x; x <= (int)bboxmax.x; x++) {
                    for (int y = (int)bboxmin.y; y <= (int)bboxmax.y; y++) {
                        Vec3 bc = barycentric(pts2d, Vec2(x, y));
                        if (bc.x < 0 || bc.y < 0 || bc.z < 0) continue;
                        double z = pts[0].z * bc.x + pts[1].z * bc.y + pts[2].z * bc.z;
                        shadows.shadow_buffer.test_and_set(x, y, z);
                    }
                }
            }
        }
        shadows.light_MVP = light_viewport * light_proj * light_view;

        // Render each cube
        std::cout << "  Rendering 3 cubes with shadows..." << std::endl;
        for (int cube = 0; cube < 3; cube++) {
            Mat4 cube_model = translate(cube_positions[cube]) *
                             rotate_y(rotations[cube]) *
                             rotate_x(rotations[cube] * 0.5) *
                             scale(scales[cube]);

            Mat4 cube_mv = View * cube_model;
            Mat4 cube_mit = transpose(inverse(cube_mv));
            Mat4 cube_mvp = scene_viewport * Projection * cube_mv;

            ShadowShader shader;
            shader.model = &model;
            shader.uniform_MVP = cube_mvp;
            shader.uniform_M = cube_model;
            shader.uniform_MIT = cube_mit;
            shader.uniform_light_dir = light_dir;
            shader.uniform_eye = eye;
            shader.shadow_map = &shadows;

            for (int i = 0; i < model.nfaces(); i++) {
                Vec4 clip_verts[3];
                for (int j = 0; j < 3; j++) {
                    clip_verts[j] = shader.vertex(i, j);
                }
                triangle_shader(clip_verts, shader, image, zbuf);
            }
        }

        // Apply SSAO
        SSAO::apply(image, zbuf, 4);

        image.flip_vertically();
        image.write_tga_file("output/advanced_scene.tga");
        std::cout << "  ✓ Saved to output/advanced_scene.tga" << std::endl;
    }

    std::cout << "\n=== Phase 9 Complete ===" << std::endl;
    std::cout << "Advanced rendering features implemented:" << std::endl;
    std::cout << "  - Shadow Mapping: Two-pass rendering from light's POV" << std::endl;
    std::cout << "  - SSAO: Screen-space ambient occlusion for depth perception" << std::endl;
    std::cout << "  - Shadow bias to prevent shadow acne" << std::endl;
    std::cout << "  - Combined shadows + SSAO" << std::endl;
    std::cout << "  - Multi-object scenes with unified shadow maps" << std::endl;

    std::cout << "\nVerification checklist:" << std::endl;
    std::cout << "  - advanced_shadows.tga: Shadow mapping demonstration" << std::endl;
    std::cout << "  - advanced_no_ssao.tga: Base rendering without SSAO" << std::endl;
    std::cout << "  - advanced_with_ssao.tga: With SSAO applied" << std::endl;
    std::cout << "  - advanced_ssao_viz.tga: SSAO factor visualization" << std::endl;
    std::cout << "  - advanced_combined.tga: Shadows + SSAO combined" << std::endl;
    std::cout << "  - advanced_scene.tga: Complex scene with 3 cubes" << std::endl;

    std::cout << "\nTechniques explained:" << std::endl;
    std::cout << "  Shadow Mapping:" << std::endl;
    std::cout << "    1. Render scene from light's perspective (depth only)" << std::endl;
    std::cout << "    2. For each pixel, check if point is closer/farther than shadow map" << std::endl;
    std::cout << "    3. If farther, it's in shadow (another object blocks light)" << std::endl;

    std::cout << "\n  SSAO (Screen-Space Ambient Occlusion):" << std::endl;
    std::cout << "    1. For each pixel, sample nearby depth values" << std::endl;
    std::cout << "    2. Count how many samples are in front (occluding)" << std::endl;
    std::cout << "    3. Darken pixel proportionally to occlusion" << std::endl;
    std::cout << "    4. Crevices and corners appear darker (more realistic)" << std::endl;

    std::cout << "\nAll 9 phases complete!" << std::endl;
    std::cout << "You've built a complete 3D renderer with:" << std::endl;
    std::cout << "  ✓ Rasterization (lines, triangles, z-buffer)" << std::endl;
    std::cout << "  ✓ 3D transformations (MVP matrices)" << std::endl;
    std::cout << "  ✓ Model loading (OBJ format)" << std::endl;
    std::cout << "  ✓ Programmable shaders (Gouraud, Phong, Flat)" << std::endl;
    std::cout << "  ✓ Ray tracing (spheres, materials, reflections)" << std::endl;
    std::cout << "  ✓ Advanced features (shadows, SSAO)" << std::endl;

    std::cout << "\nThis covers the fundamentals of modern rendering engines!" << std::endl;

    return 0;
}
