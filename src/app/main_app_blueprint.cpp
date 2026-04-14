#include "app_blueprint.h"
#include "ui_effects.h"
#include "splash_screen.h"
#include "../scene/editable_mesh.h"
#include "../rasterizer/phong_shader.h"
#include "../raytracer/ray.h"
#include "../raytracer/material.h"
#include "../raytracer/scene.h"
#include "../raytracer/camera.h"
#include "../raytracer/triangle_hit.h"
#include "../raytracer/mesh_to_scene.h"
#include "../raytracer/renderer.h"
#include <cstdio>
#include <cmath>
#include <algorithm>
#include <memory>
#include <vector>

// ============================================================
// Extended App State — mesh, editing, display options
// ============================================================
struct ExtState : AppState {
    // Live mesh being rendered / edited
    EditableMesh mesh = EditableMesh::make_cube();
    int primitive_idx = 0;                      // 0 cube, 1 octa, 2 tetra, 3 ghost

    // Model transform
    double model_rot_x = 0.0;
    double model_rot_y = 0.35;                  // start slightly rotated
    double model_rot_z = 0.0;
    double auto_spin_speed = 0.008;
    bool   auto_spin  = true;
    bool   show_grid  = true;
    bool   scanlines  = true;                   // CRT overlay on viewport

    // Material properties
    double mat_ambient   = 0.14;
    double mat_diffuse   = 0.78;
    double mat_specular  = 0.55;
    double mat_shininess = 48.0;

    double fov_deg = 60.0;

    // Cached projection state for mouse picking — recomputed every frame.
    Mat4  cached_MVP;
    Mat4  cached_Model;     // for inverse-projecting mouse drags
    Mat4  cached_PV;
    bool  picking_ready = false;
    std::vector<Vec3> screen_verts;             // projected verts (viewport-local)
    std::vector<double> depth_verts;            // clip.w

    // Vertex drag state
    bool dragging_vertex = false;
    int  drag_last_mx = 0, drag_last_my = 0;

    // Raytracing progressive accumulation buffer
    std::vector<Vec3> rt_accum;   // accumulated color per pixel
    int rt_accum_w = 0, rt_accum_h = 0;
    double rt_last_rot_y = -999, rt_last_rot_x = -999, rt_last_rot_z = -999;
    double rt_last_yaw = -999, rt_last_pitch = -999, rt_last_dist = -999;
    int    rt_last_vert_count = -1;

    ExtState() : AppState() {
        camera.yaw   = -45.0;
        camera.pitch =  25.0;
        camera.distance = 3.8;
        camera.update();
        light_dir = normalized(Vec3(0.6, 1.0, 0.5));
    }

    void set_primitive(int idx) {
        primitive_idx = idx;
        switch (idx) {
            case 1:  mesh = EditableMesh::make_octahedron(); break;
            case 2:  mesh = EditableMesh::make_tetrahedron(); break;
            case 3:  mesh = EditableMesh::make_mascot(); break;
            default: mesh = EditableMesh::make_cube(); break;
        }
        selected_vertex = -1;
        selected_face   = -1;
    }
    const char* primitive_name() const {
        switch (primitive_idx) {
            case 0: return "CUBE";
            case 1: return "OCTAHEDRON";
            case 2: return "TETRAHEDRON";
            case 3: return "GHOST";
            default: return "CUSTOM OBJ";
        }
    }
};

// ------------------------------------------------------------
// Clamp helper
// ------------------------------------------------------------
static inline double clampd(double v, double lo, double hi) {
    return v < lo ? lo : (v > hi ? hi : v);
}

// ============================================================
// Boot Sequence
// ============================================================
void boot_sequence(TGAImage& fb, SDL_Window* win, SDL_Surface* scr, int w, int h) {
    const char* lines[] = {
        "", "  3D RENDERER v4.0 - MESH EDITOR",
        "  ========================================", "",
        "  [OK] BLINN-PHONG SHADING",
        "  [OK] PERSPECTIVE_FOV PROJECTION",
        "  [OK] Z-BUFFER DEPTH TESTING",
        "  [OK] BACKFACE CULLING",
        "  [OK] OPENMP PARALLEL RENDER",
        "  [OK] EDITABLE MESH / EXTRUDE / SUBDIVIDE",
        "  [OK] VERTEX + FACE PICKING", "",
        "  LOADING PIXEL AI...", ""
    };
    int n = (int)(sizeof(lines) / sizeof(lines[0]));

    for (int i = 0; i < n; i++) {
        fb.fill(Theme::BG_DARK);
        for (int y = 0; y < h; y += 40)
            for (int x = 0; x < w; x++)
                fb.set(x, y, TGAColor(8, 14, 10));
        for (int x = 0; x < w; x += 40)
            for (int y = 0; y < h; y++)
                fb.set(x, y, TGAColor(8, 14, 10));

        int ty = 110;
        for (int j = 0; j <= i && j < n; j++) {
            TGAColor col = (j == i) ? Theme::ACCENT : Theme::TEXT_DIM;
            Font::draw_string(fb, w/2 - 220, ty, lines[j], col);
            ty += 17;
        }
        if (i >= 12) {
            Mascot::draw_with_glow(fb, w/2 - 8, ty + 10, Mascot::IDLE, Theme::TEXT);
            Font::draw_string(fb, w/2 - 40, ty + 50, "PIXEL AI v4.0", Theme::ACCENT, 2);
            Font::draw_string(fb, w/2 - 90, ty + 85, "RETRO-MODERN RENDER ASSISTANT", Theme::TEXT_DIM);
        }

        SDL_LockSurface(scr);
        {
            uint32_t* px = static_cast<uint32_t*>(scr->pixels);
            for (int y = 0; y < h; y++) {
                uint32_t* row = px + y * w;
                for (int x = 0; x < w; x++) {
                    TGAColor c = fb.get(x, y);
                    row[x] = 0xFF000000u | (c[2] << 16) | (c[1] << 8) | c[0];
                }
            }
        }
        SDL_UnlockSurface(scr);
        SDL_UpdateWindowSurface(win);
        SDL_Delay(70);

        SDL_Event e;
        while (SDL_PollEvent(&e))
            if (e.type == SDL_KEYDOWN || e.type == SDL_QUIT) return;
    }
}

// ============================================================
// Sky background — vertical gradient + horizon glow + star field
// ============================================================
// Deterministic cheap hash so stars stay pinned on resize/redraw.
static inline uint32_t hash2u(int x, int y) {
    uint32_t n = (uint32_t)(x * 374761393u + y * 668265263u);
    n = (n ^ (n >> 13)) * 1274126177u;
    return n ^ (n >> 16);
}

void draw_viewport_bg(TGAImage& vp) {
    const int w = vp.width(), h = vp.height();

    // Pull accent from the live palette so sky tints with the theme.
    const TGAColor& acc = Theme::ACCENT;
    Vec3 accent(acc[2] / 255.0, acc[1] / 255.0, acc[0] / 255.0);

    // Two sky colors: deep top, warmer horizon.
    Vec3 sky_top(0.015, 0.025, 0.055);
    Vec3 sky_mid(0.04,  0.06,  0.10);
    Vec3 horizon = Vec3(0.08, 0.10, 0.14) * 0.6 + accent * 0.15;

    // Vignette center (screen center, slightly higher than midpoint).
    const double cx = w * 0.5;
    const double cy = h * 0.44;
    const double maxd = std::sqrt(cx*cx + cy*cy);

    for (int y = 0; y < h; y++) {
        double t = (double)y / h;                     // 0 at top
        // ease-in-out for smoother horizon band
        double s = t * t * (3 - 2 * t);
        Vec3 col;
        if (t < 0.65) {
            double k = s / 0.65;
            col = sky_top * (1 - k) + sky_mid * k;
        } else {
            double k = (t - 0.65) / 0.35;
            col = sky_mid * (1 - k) + horizon * k;
        }
        // Radial vignette on top of gradient
        for (int x = 0; x < w; x++) {
            double dx = x - cx, dy = y - cy;
            double rd = std::sqrt(dx*dx + dy*dy) / maxd;
            double v = 1.0 - rd * 0.45;
            Vec3 c = col * v;
            vp.set(x, y, TGAColor(
                (uint8_t)std::min(255.0, c.x * 255.0),
                (uint8_t)std::min(255.0, c.y * 255.0),
                (uint8_t)std::min(255.0, c.z * 255.0)));
        }
    }

    // Horizon glow band — subtle accent at ~72% down
    int hy = (int)(h * 0.72);
    for (int dy = -8; dy <= 8; dy++) {
        int yy = hy + dy;
        if (yy < 0 || yy >= h) continue;
        double a = std::exp(-dy*dy / 18.0) * 0.22;
        for (int x = 0; x < w; x++) {
            TGAColor c = vp.get(x, yy);
            double e = std::exp(-std::pow((x - w/2.0) / (w*0.42), 2)) * a;
            vp.set(x, yy, TGAColor(
                (uint8_t)std::min(255.0, c[2] + accent.x * 255 * e),
                (uint8_t)std::min(255.0, c[1] + accent.y * 255 * e),
                (uint8_t)std::min(255.0, c[0] + accent.z * 255 * e)));
        }
    }

    // Star field — sparse twinkle in upper 65% of the sky
    int star_count = (w * h) / 900;
    for (int i = 0; i < star_count; i++) {
        uint32_t h1 = hash2u(i, 17);
        uint32_t h2 = hash2u(i, 91);
        int sx = h1 % w;
        int sy = h2 % (int)(h * 0.65);
        double bri = 0.35 + ((h1 >> 8) & 0xFF) / 512.0;
        TGAColor c = vp.get(sx, sy);
        int nv = (int)std::min(255.0, c[2] + bri * 180);
        int ng = (int)std::min(255.0, c[1] + bri * 190);
        int nb = (int)std::min(255.0, c[0] + bri * 220);
        vp.set(sx, sy, TGAColor(nv, ng, nb));
        // Occasional bigger stars
        if ((h1 & 0x1F) == 0 && sx > 0 && sy > 0 && sx < w-1 && sy < h-1) {
            TGAColor halo(nv / 2, ng / 2, nb / 2);
            vp.set(sx - 1, sy, halo);
            vp.set(sx + 1, sy, halo);
            vp.set(sx, sy - 1, halo);
            vp.set(sx, sy + 1, halo);
        }
    }
}

// ============================================================
// Project a 3D world point → 2D screen point
// ============================================================
bool project_point(const Vec3& p, const Mat4& MVP, Vec3& out, double& wclip) {
    Vec4 clip = MVP * embed(p);
    if (clip.w <= 0.001) return false;
    out   = proj(clip);
    wclip = clip.w;
    return true;
}

// ============================================================
// Floor grid
// ============================================================
void draw_floor_grid(TGAImage& vp, const Mat4& MVP) {
    float extent = 2.0f;
    int   steps  = 8;
    float step   = (2.0f * extent) / steps;
    TGAColor gc_minor(0, 42, 30);
    TGAColor gc_major(0, 70, 50);
    for (int i = 0; i <= steps; i++) {
        float t = -extent + i * step;
        bool major = (i == steps/2);
        TGAColor gc = major ? gc_major : gc_minor;
        Vec3 a3d(t, -0.52f, -extent), b3d(t, -0.52f,  extent);
        Vec3 as, bs; double wa, wb;
        if (project_point(a3d, MVP, as, wa) && project_point(b3d, MVP, bs, wb))
            line(as, bs, vp, gc);
        Vec3 c3d(-extent, -0.52f, t), d3d(extent, -0.52f, t);
        Vec3 cs, ds; double wc, wd;
        if (project_point(c3d, MVP, cs, wc) && project_point(d3d, MVP, ds, wd))
            line(cs, ds, vp, gc);
    }
    Vec3 ox0, ox1, oz0, oz1; double w0;
    if (project_point(Vec3(-extent, -0.52f, 0), MVP, ox0, w0) &&
        project_point(Vec3( extent, -0.52f, 0), MVP, ox1, w0))
        line(ox0, ox1, vp, TGAColor(30, 40, 180));
    if (project_point(Vec3(0, -0.52f, -extent), MVP, oz0, w0) &&
        project_point(Vec3(0, -0.52f,  extent), MVP, oz1, w0))
        line(oz0, oz1, vp, TGAColor(30, 140, 30));
}

// ============================================================
// Square marker (for vertex handles)
// ============================================================
static void draw_square(TGAImage& vp, int cx, int cy, int half, const TGAColor& fill,
                        const TGAColor& border) {
    int w = vp.width(), h = vp.height();
    for (int dy = -half; dy <= half; dy++) {
        for (int dx = -half; dx <= half; dx++) {
            int x = cx + dx, y = cy + dy;
            if (x < 0 || y < 0 || x >= w || y >= h) continue;
            bool edge = (std::abs(dx) == half || std::abs(dy) == half);
            vp.set(x, y, edge ? border : fill);
        }
    }
}

// ============================================================
// Render 3D scene — now driven by EditableMesh
// ============================================================
void render_scene(TGAImage& vp, ZBuffer& zb, ExtState& state) {
    draw_viewport_bg(vp);
    zb.clear();

    state.camera.update();

    int   vw = vp.width(), vh = vp.height();
    double aspect = (double)vw / vh;
    double fov    = state.fov_deg * M_PI / 180.0;

    Mat4 Proj  = perspective_fov(fov, aspect, 0.1, 100.0);
    Mat4 View  = state.camera.get_view_matrix();
    Mat4 Model = rotate_y(state.model_rot_y)
               * rotate_x(state.model_rot_x)
               * rotate_z(state.model_rot_z);
    Mat4 VP    = viewport(0, 0, vw, vh);
    Mat4 PV    = Proj * View;
    Mat4 MVPw  = VP * PV;
    Mat4 MVP   = VP * PV * Model;

    state.cached_MVP   = MVP;
    state.cached_Model = Model;
    state.cached_PV    = PV;

    if (state.light_rotate) {
        state.light_angle += 0.018;
        state.light_dir = normalized(Vec3(
            std::cos(state.light_angle) * 0.8, 0.9,
            std::sin(state.light_angle) * 0.8));
    }

    if (state.show_grid) draw_floor_grid(vp, MVPw);

    // -------- Project every vertex once (picking + rendering share this)
    const auto& mesh = state.mesh;
    state.screen_verts.assign(mesh.vertex_count(), Vec3());
    state.depth_verts.assign(mesh.vertex_count(), -1.0);
    for (int i = 0; i < mesh.vertex_count(); i++) {
        Vec3 s; double wc;
        if (project_point(mesh.verts[i], MVP, s, wc)) {
            state.screen_verts[i] = s;
            state.depth_verts[i]  = wc;
        }
    }
    state.picking_ready = true;

    // -------- Face rendering
    const TGAColor& hue = Theme::CUBE_HUE;
    auto tint = [&](double s) {
        return TGAColor(
            static_cast<uint8_t>(std::min(255.0, hue[2] * s)),
            static_cast<uint8_t>(std::min(255.0, hue[1] * s)),
            static_cast<uint8_t>(std::min(255.0, hue[0] * s))
        );
    };

    Vec3 eye = state.camera.position;
    Vec3 ld  = state.light_dir;

    // Smooth per-vertex normals (model space), transformed by Model into world space.
    std::vector<Vec3> vn_world;
    vn_world.reserve(mesh.vertex_count());
    {
        auto vn_local = mesh.compute_vertex_normals();
        for (const auto& vn : vn_local) {
            Vec4 mn = Model * embed(vn, 0.0);  // rotation-only → safe
            Vec3 w(mn.x, mn.y, mn.z);
            double l = w.norm();
            vn_world.push_back(l > 1e-9 ? w / l : Vec3(0,1,0));
        }
    }

    // Build the Phong light + material from theme + state sliders.
    PhongLight    L;
    L.key_dir      = ld;
    L.key_color    = Vec3(Theme::HIGHLIGHT[2]/255.0 * 0.55 + 0.55,
                          Theme::HIGHLIGHT[1]/255.0 * 0.55 + 0.5,
                          Theme::HIGHLIGHT[0]/255.0 * 0.55 + 0.4);
    L.fill_dir     = normalized(Vec3(-ld.x*0.6, 0.35, -ld.z*0.6));
    L.fill_color   = Vec3(Theme::ACCENT[2]/255.0 * 0.45,
                          Theme::ACCENT[1]/255.0 * 0.50,
                          Theme::ACCENT[0]/255.0 * 0.60);
    L.ambient_color = Vec3(0.10, 0.13, 0.18);

    PhongMaterial M;
    M.base_color  = Vec3(hue[2]/255.0, hue[1]/255.0, hue[0]/255.0);
    M.ambient     = state.mat_ambient;
    M.diffuse     = state.mat_diffuse;
    M.specular    = state.mat_specular;
    M.shininess   = state.mat_shininess;
    M.rim_color   = Vec3(Theme::HIGHLIGHT[2]/255.0,
                         Theme::HIGHLIGHT[1]/255.0,
                         Theme::HIGHLIGHT[0]/255.0);
    M.rim_strength  = 0.55;
    M.rim_power     = 2.4;
    M.pattern       = 2;
    M.pattern_scale = 4.0;
    M.pattern_contrast = 0.30;
    M.fog_color     = Vec3(0.04, 0.06, 0.11);
    M.fog_near      = 3.0;
    M.fog_far       = 13.0;
    M.fog_strength  = 0.55;

    if (state.render_mode == AppState::RAYTRACE) {
        // -------- Progressive path tracing --------
        // Auto-detect when scene changes → reset accumulator
        if (state.model_rot_y != state.rt_last_rot_y ||
            state.model_rot_x != state.rt_last_rot_x ||
            state.model_rot_z != state.rt_last_rot_z ||
            state.camera.yaw != state.rt_last_yaw ||
            state.camera.pitch != state.rt_last_pitch ||
            state.camera.distance != state.rt_last_dist ||
            mesh.vertex_count() != state.rt_last_vert_count) {
            state.rt_dirty = true;
            state.rt_last_rot_y = state.model_rot_y;
            state.rt_last_rot_x = state.model_rot_x;
            state.rt_last_rot_z = state.model_rot_z;
            state.rt_last_yaw   = state.camera.yaw;
            state.rt_last_pitch = state.camera.pitch;
            state.rt_last_dist  = state.camera.distance;
            state.rt_last_vert_count = mesh.vertex_count();
        }

        // Build RT scene from current mesh + model transform
        Vec3 mesh_col(hue[2]/255.0, hue[1]/255.0, hue[0]/255.0);
        Scene rt_scene = mesh_to_rt_scene(mesh, Model, mesh_col, state.mat_specular);

        // Build RT camera matching the rasterizer camera
        double vfov = state.fov_deg;
        double ar = (double)vw / vh;
        Vec3 lookfrom = state.camera.position;
        Vec3 lookat   = state.camera.target;
        Vec3 vup(0, 1, 0);
        RTCamera rt_cam(lookfrom, lookat, vup, vfov, ar);

        // Reset accumulator when scene changes
        if (state.rt_dirty || state.rt_accum_w != vw || state.rt_accum_h != vh) {
            state.rt_accum.assign(vw * vh, Vec3(0, 0, 0));
            state.rt_accum_w = vw;
            state.rt_accum_h = vh;
            state.rt_samples = 0;
            state.rt_dirty = false;
        }

        // Add one sample per pixel per frame (progressive refinement)
        int max_samples = 64;
        if (state.rt_samples < max_samples) {
            state.rt_samples++;
            for (int j = 0; j < vh; j++) {
                for (int i = 0; i < vw; i++) {
                    double u = (i + random_double()) / (vw - 1);
                    double v_coord = (j + random_double()) / (vh - 1);
                    Ray r = rt_cam.get_ray(u, v_coord);
                    Vec3 c = ray_color(r, rt_scene, 6);
                    state.rt_accum[j * vw + i] = state.rt_accum[j * vw + i] + c;
                }
            }
        }

        // Write accumulated result to viewport
        double inv_s = 1.0 / state.rt_samples;
        for (int j = 0; j < vh; j++) {
            for (int i = 0; i < vw; i++) {
                Vec3 c = state.rt_accum[j * vw + i] * inv_s;
                // Gamma correction (gamma=2)
                c = Vec3(std::sqrt(std::max(0.0, c.x)),
                         std::sqrt(std::max(0.0, c.y)),
                         std::sqrt(std::max(0.0, c.z)));
                c.x = std::min(1.0, c.x);
                c.y = std::min(1.0, c.y);
                c.z = std::min(1.0, c.z);
                // TGAImage is BGRA: set(x, y, TGAColor(R, G, B))
                vp.set(i, j, TGAColor(
                    (uint8_t)(c.x * 255),
                    (uint8_t)(c.y * 255),
                    (uint8_t)(c.z * 255)));
            }
        }
    } else {
    // -------- Rasterization path (WIREFRAME / FLAT / GOURAUD) --------
    for (int fi = 0; fi < mesh.face_count(); fi++) {
        const auto& tri = mesh.faces[fi];

        Vec3 wld[3]; Vec3 scr[3]; Vec3 nrm[3]; bool ok = true;
        for (int v = 0; v < 3; v++) {
            Vec4 mp = Model * embed(mesh.verts[tri[v]]);
            wld[v]  = Vec3(mp.x, mp.y, mp.z);
            Vec4 cp = Proj * View * mp;
            if (cp.w <= 0.001) { ok = false; break; }
            Vec4 sp = VP * cp;
            scr[v]  = proj(sp);
            nrm[v]  = vn_world[tri[v]];
        }
        if (!ok) continue;

        Vec3 fn = normalized(cross(wld[1]-wld[0], wld[2]-wld[0]));
        double sa = (scr[1].x-scr[0].x)*(scr[2].y-scr[0].y)
                  - (scr[2].x-scr[0].x)*(scr[1].y-scr[0].y);
        if (sa <= 0) continue;

        if (state.render_mode == AppState::WIREFRAME) {
            double face_brightness = 0.5 + 0.5 * std::max(0.0, fn.y*0.6 + fn.x*0.3 + 0.4);
            TGAColor wc = tint(face_brightness) * 0.85;
            line(scr[0], scr[1], vp, wc);
            line(scr[1], scr[2], vp, wc);
            line(scr[2], scr[0], vp, wc);
        } else {
            Vec3 use_nrm[3];
            if (state.render_mode == AppState::FLAT) {
                use_nrm[0] = use_nrm[1] = use_nrm[2] = fn;
            } else {
                use_nrm[0] = nrm[0]; use_nrm[1] = nrm[1]; use_nrm[2] = nrm[2];
            }
            triangle_phong(scr, wld, use_nrm, eye, L, M, zb, vp);
        }

        // Highlight selected face in edit mode
        if (state.edit_mode == AppState::EDIT_MODE &&
            state.select_mode == AppState::SEL_FACE &&
            fi == state.selected_face) {
            double pulse = 0.55 + 0.45 * std::sin(state.ui_phase * 2.0 * M_PI);
            TGAColor glow = Theme::HIGHLIGHT * pulse;
            line(scr[0], scr[1], vp, glow);
            line(scr[1], scr[2], vp, glow);
            line(scr[2], scr[0], vp, glow);
        }
    }

    // -------- Vertex handles (edit mode, vertex sub-mode)
    if (state.edit_mode == AppState::EDIT_MODE &&
        state.select_mode == AppState::SEL_VERT) {
        for (int vi = 0; vi < mesh.vertex_count(); vi++) {
            if (state.depth_verts[vi] <= 0) continue;
            int px = (int)state.screen_verts[vi].x;
            int py = (int)state.screen_verts[vi].y;
            bool is_sel = (vi == state.selected_vertex);
            TGAColor fill   = is_sel ? Theme::HIGHLIGHT : Theme::BG_DARK;
            TGAColor border = is_sel ? Theme::ACCENT    : Theme::TEXT;
            int half = is_sel ? 4 : 3;
            if (is_sel) {
                double pulse = 0.55 + 0.45 * std::sin(state.ui_phase * 2.0 * M_PI);
                fill = Theme::HIGHLIGHT * pulse;
            }
            draw_square(vp, px, py, half, fill, border);
        }
    }
    } // end rasterization else

    // -------- Scanline overlay (CRT feel)
    if (state.scanlines) {
        for (int y = 0; y < vh; y += 3) {
            for (int x = 0; x < vw; x++) {
                TGAColor c = vp.get(x, y);
                vp.set(x, y, TGAColor(
                    static_cast<uint8_t>(c[2] * 0.82),
                    static_cast<uint8_t>(c[1] * 0.82),
                    static_cast<uint8_t>(c[0] * 0.82)));
            }
        }
    }
}

// ============================================================
// Compact value bar
// ============================================================
void draw_value_bar(TGAImage& fb, int x, int y, int w, double val, double vmin, double vmax,
                    const TGAColor& col) {
    double t = clampd((val - vmin) / (vmax - vmin), 0, 1);
    int fill = static_cast<int>(t * w);
    for (int px = 0; px < w; px++) {
        TGAColor bc = (px < fill) ? col * 0.75 : Theme::BG_DARK;
        fb.set(x + px, y,   bc);
        fb.set(x + px, y+1, bc);
    }
}

// ============================================================
// Corner brackets — L-shaped viewport frame instead of full border
// ============================================================
static void draw_corner_brackets(TGAImage& fb, int x, int y, int w, int h,
                                 int arm, const TGAColor& c) {
    auto hline = [&](int px, int py, int len) {
        for (int i = 0; i < len; i++) fb.set(px + i, py, c);
    };
    auto vline = [&](int px, int py, int len) {
        for (int i = 0; i < len; i++) fb.set(px, py + i, c);
    };
    // TL
    hline(x, y, arm);         vline(x, y, arm);
    hline(x, y + 1, arm - 1); vline(x + 1, y, arm - 1);
    // TR
    hline(x + w - arm, y, arm);             vline(x + w - 1, y, arm);
    hline(x + w - arm + 1, y + 1, arm - 1); vline(x + w - 2, y, arm - 1);
    // BL
    hline(x, y + h - 1, arm);         vline(x, y + h - arm, arm);
    hline(x, y + h - 2, arm - 1);     vline(x + 1, y + h - arm + 1, arm - 1);
    // BR
    hline(x + w - arm, y + h - 1, arm);       vline(x + w - 1, y + h - arm, arm);
    hline(x + w - arm + 1, y + h - 2, arm-1); vline(x + w - 2, y + h - arm + 1, arm-1);
}

// ============================================================
// Mode pill badge (OBJECT / EDIT)
// ============================================================
static void draw_mode_pill(TGAImage& fb, int x, int y, const char* txt,
                           const TGAColor& tint, double pulse) {
    int pad = 8;
    int w = Font::text_width(txt) * 2 + pad * 2;
    int h = 22;
    // Fill
    TGAColor bg = tint * (0.35 + 0.15 * pulse);
    for (int py = 0; py < h; py++)
        for (int px = 0; px < w; px++)
            fb.set(x + px, y + py, bg);
    // Border
    for (int px = 0; px < w; px++) {
        fb.set(x + px, y,         tint);
        fb.set(x + px, y + h - 1, tint * 0.45);
    }
    for (int py = 0; py < h; py++) {
        fb.set(x,         y + py, tint);
        fb.set(x + w - 1, y + py, tint * 0.45);
    }
    Font::draw_string(fb, x + pad, y + 4, txt, Theme::BG_DARK, 2);
}

// ============================================================
// Frame-time histogram (sparkline) for the inspector
// ============================================================
static void draw_frame_histogram(TGAImage& fb, int x, int y, int w, int h,
                                 const double* hist, int count, int head) {
    // Border rect
    for (int i = 0; i < w; i++) { fb.set(x+i, y, Theme::GRID); fb.set(x+i, y+h-1, Theme::GRID); }
    for (int i = 0; i < h; i++) { fb.set(x, y+i, Theme::GRID); fb.set(x+w-1, y+i, Theme::GRID); }

    // Find max for auto-scale, clamp so one outlier doesn't nuke the axis.
    double mx = 1.0 / 30.0;
    for (int i = 0; i < count; i++) mx = std::max(mx, hist[i]);
    mx = std::min(mx, 1.0 / 15.0);

    for (int i = 0; i < w - 2 && i < count; i++) {
        int idx = (head + i) % count;
        double t = clampd(hist[idx] / mx, 0, 1);
        int bh = (int)(t * (h - 4));
        int px = x + 1 + i;
        for (int yy = 0; yy < bh; yy++) {
            double k = (double)yy / (h - 4);
            TGAColor c = (k > 0.8) ? Theme::WARNING
                        : (k > 0.5) ? Theme::ACCENT
                        : Theme::SUCCESS;
            fb.set(px, y + h - 2 - yy, c * 0.85);
        }
    }

    // 60fps baseline
    int baseline = y + h - 2 - (int)((1.0/60.0) / mx * (h - 4));
    if (baseline > y && baseline < y + h)
        for (int i = 2; i < w - 2; i += 3)
            fb.set(x + i, baseline, Theme::TEXT_DIM);
}

// ============================================================
// Draw full UI — bento layout with left transform / right inspector
// ============================================================
void draw_ui(TGAImage& fb, ExtState& state) {
    char buf[128];
    int W = state.width, H = state.height;

    const int LEFT_W   = 256;
    const int RIGHT_W  = 200;
    const int HEADER_H = 52;
    const int FOOTER_H = 30;
    const int right_x  = W - RIGHT_W;

    // ---------- Top header panel ----------
    UIFx::draw_panel_pro(fb, 0, 0, W, HEADER_H, Theme::BG_PANEL, Theme::GRID, Theme::ACCENT, 3);
    // Strong accent glow bar across top
    UIFx::draw_glow_bar(fb, 0, 2, W, 4, Theme::ACCENT, 0.5);
    Font::draw_string(fb, 16, 16, "3D RENDERER", Theme::ACCENT, 2);
    snprintf(buf, sizeof(buf), "v4.0  |  MESH EDITOR  |  %s", Theme::live.name);
    Font::draw_string(fb, 180, 22, buf, Theme::TEXT_DIM);

    // Mode badge (OBJECT / EDIT) — hero element, bounce with pulse.
    double ui_pulse = 0.5 + 0.5 * std::sin(state.ui_phase * 2.0 * M_PI);
    bool editing = (state.edit_mode == AppState::EDIT_MODE);
    draw_mode_pill(fb, 338, 14,
                   editing ? "EDIT" : "OBJECT",
                   editing ? Theme::WARNING : Theme::ACCENT,
                   ui_pulse);

    // Render-mode button bar
    for (auto& btn : state.mode_buttons)
        btn.draw(fb, state.ui_phase);

    // FPS display (right of header)
    snprintf(buf, sizeof(buf), "%.0f FPS", state.fps);
    Font::draw_string(fb, W - 92, 10, buf, Theme::SUCCESS, 2);
    snprintf(buf, sizeof(buf), "%.2f ms", state.frame_time * 1000.0);
    Font::draw_string(fb, W - 92, 30, buf, Theme::TEXT_DIM);

    // ---------- Left transform sidebar ----------
    UIFx::draw_panel_pro(fb, 0, HEADER_H, LEFT_W, H - HEADER_H - FOOTER_H,
                         Theme::BG_PANEL, Theme::GRID, Theme::ACCENT, 3);
    // Vertical accent glow strip on left edge
    for (int y = HEADER_H; y < H - FOOTER_H; y++) {
        fb.set(0, y, Theme::ACCENT * 0.65);
        fb.set(1, y, Theme::ACCENT * 0.35);
        fb.set(2, y, Theme::ACCENT * 0.12);
    }

    int sy = HEADER_H + 14;
    auto section = [&](const char* title) {
        sy += 4;
        // Accent bar with gradient fade
        for (int by = 0; by < 12; by++) {
            double fade = 1.0 - by / 18.0;
            fb.set(6, sy + by, Theme::ACCENT * fade);
            fb.set(7, sy + by, Theme::ACCENT * fade);
            fb.set(8, sy + by, Theme::ACCENT * (fade * 0.55));
            fb.set(9, sy + by, Theme::ACCENT * (fade * 0.25));
        }
        Font::draw_string(fb, 16, sy + 2, title, Theme::ACCENT);
        sy += 16;
        UIFx::draw_emboss_line(fb, 6, sy, LEFT_W - 6,
                               Theme::GRID * 0.5, Theme::BG_PANEL * 1.3);
        sy += 10;
    };
    auto row = [&](const char* label, const char* value, const TGAColor& vc = Theme::TEXT_DIM) {
        Font::draw_string(fb, 14, sy, label, Theme::TEXT_DIM);
        Font::draw_string(fb, 112, sy, value, vc);
        sy += 13;
    };

    // Mascot — prominent in sidebar at 2x scale
    {
        int mx = LEFT_W / 2 - 16;  // 2x=32px wide, center it
        Mascot::draw_with_glow(fb, mx, sy, state.get_ghost_state(), Theme::TEXT, 2);
        sy += 38;
        const char* tip = Mascot::get_message(state.get_ghost_state());
        int tw = Font::text_width(tip);
        Font::draw_string(fb, LEFT_W / 2 - tw / 2, sy, tip, Theme::ACCENT);
        sy += 14;
        // Context tip
        const char* ctx = editing ? "careful with those verts"
                        : state.auto_spin ? "watching the spin..."
                        : "press TAB to edit";
        int cw = Font::text_width(ctx);
        Font::draw_string(fb, LEFT_W / 2 - cw / 2, sy, ctx, Theme::TEXT_DIM);
        sy += 14;
        UIFx::draw_emboss_line(fb, 12, sy, LEFT_W - 12,
                               Theme::GRID * 0.4, Theme::BG_PANEL * 1.2);
        sy += 10;
    }

    // [ PRIMITIVE ]
    section("[ PRIMITIVE ]");
    snprintf(buf, sizeof(buf), "> %s", state.primitive_name());
    Font::draw_string(fb, 18, sy, buf, Theme::TEXT); sy += 13;
    Font::draw_string(fb, 18, sy, "1 cube 2 octa 3 tetra 4 ghost", Theme::TEXT_DIM);
    sy += 18;

    // [ TRANSFORM ]
    section("[ TRANSFORM ]");
    snprintf(buf, sizeof(buf), "%7.1f deg", state.model_rot_x * 180.0 / M_PI);
    row("ROT X", buf);
    UIFx::draw_value_bar_pro(fb, 14, sy, LEFT_W - 28,
                   fmod(state.model_rot_x + 2*M_PI, 2*M_PI)/(2*M_PI), 0, 1,
                   TGAColor(80, 80, 220)); sy += 7;
    snprintf(buf, sizeof(buf), "%7.1f deg", state.model_rot_y * 180.0 / M_PI);
    row("ROT Y", buf);
    UIFx::draw_value_bar_pro(fb, 14, sy, LEFT_W - 28,
                   fmod(state.model_rot_y + 2*M_PI, 2*M_PI)/(2*M_PI), 0, 1,
                   TGAColor(80, 220, 80)); sy += 7;
    snprintf(buf, sizeof(buf), "%7.1f deg", state.model_rot_z * 180.0 / M_PI);
    row("ROT Z", buf);
    UIFx::draw_value_bar_pro(fb, 14, sy, LEFT_W - 28,
                   fmod(state.model_rot_z + 2*M_PI, 2*M_PI)/(2*M_PI), 0, 1,
                   TGAColor(220, 100, 80)); sy += 7;
    row("AUTO-SPIN", state.auto_spin ? "ON" : "OFF",
        state.auto_spin ? Theme::SUCCESS : Theme::TEXT_DIM);
    sy += 6;

    // [ MATERIAL ]
    section("[ MATERIAL ]");
    snprintf(buf, sizeof(buf), "%.2f", state.mat_ambient);
    row("AMBIENT", buf);
    UIFx::draw_value_bar_pro(fb, 14, sy, LEFT_W - 28, state.mat_ambient, 0, 0.5, Theme::TEXT); sy += 7;
    snprintf(buf, sizeof(buf), "%.2f", state.mat_diffuse);
    row("DIFFUSE", buf);
    UIFx::draw_value_bar_pro(fb, 14, sy, LEFT_W - 28, state.mat_diffuse, 0, 1.0, Theme::ACCENT); sy += 7;
    snprintf(buf, sizeof(buf), "%.2f", state.mat_specular);
    row("SPECULAR", buf);
    UIFx::draw_value_bar_pro(fb, 14, sy, LEFT_W - 28, state.mat_specular, 0, 1.0, Theme::HIGHLIGHT); sy += 7;
    snprintf(buf, sizeof(buf), "%.0f", state.mat_shininess);
    row("SHININESS", buf);
    sy += 6;

    // [ CAMERA ]
    section("[ CAMERA ]");
    Vec3 pos = state.camera.position;
    snprintf(buf, sizeof(buf), "%+6.2f", pos.x); row("POS X", buf);
    snprintf(buf, sizeof(buf), "%+6.2f", pos.y); row("POS Y", buf);
    snprintf(buf, sizeof(buf), "%+6.2f", pos.z); row("POS Z", buf);
    snprintf(buf, sizeof(buf), "%5.2f",  state.camera.distance); row("DIST", buf);
    snprintf(buf, sizeof(buf), "%3.0f deg", state.fov_deg); row("FOV", buf);
    sy += 6;

    // [ DISPLAY ]
    section("[ DISPLAY ]");
    row("LIGHT",     state.light_rotate ? "ROTATING" : "STATIC",
        state.light_rotate ? Theme::WARNING : Theme::TEXT_DIM);
    row("GRID",      state.show_grid  ? "ON" : "OFF",
        state.show_grid  ? Theme::SUCCESS : Theme::TEXT_DIM);
    row("SCANLINES", state.scanlines  ? "ON" : "OFF",
        state.scanlines  ? Theme::SUCCESS : Theme::TEXT_DIM);

    // ---------- Right inspector panel ----------
    UIFx::draw_panel_pro(fb, right_x, HEADER_H, RIGHT_W, H - HEADER_H - FOOTER_H,
                         Theme::BG_PANEL, Theme::GRID, Theme::ACCENT, 3);
    for (int y = HEADER_H; y < H - FOOTER_H; y++) {
        fb.set(W - 1, y, Theme::ACCENT * 0.65);
        fb.set(W - 2, y, Theme::ACCENT * 0.35);
        fb.set(W - 3, y, Theme::ACCENT * 0.12);
    }

    int iy = HEADER_H + 14;
    auto isection = [&](const char* title) {
        iy += 4;
        for (int by = 0; by < 12; by++) {
            double fade = 1.0 - by / 18.0;
            fb.set(right_x + 6, iy + by, Theme::ACCENT * fade);
            fb.set(right_x + 7, iy + by, Theme::ACCENT * fade);
            fb.set(right_x + 8, iy + by, Theme::ACCENT * (fade * 0.5));
        }
        Font::draw_string(fb, right_x + 14, iy + 2, title, Theme::ACCENT);
        iy += 16;
        UIFx::draw_emboss_line(fb, right_x + 6, iy, right_x + RIGHT_W - 6,
                               Theme::GRID * 0.5, Theme::BG_PANEL * 1.3);
        iy += 10;
    };
    auto irow = [&](const char* label, const char* value, const TGAColor& vc = Theme::TEXT) {
        Font::draw_string(fb, right_x + 12, iy, label, Theme::TEXT_DIM);
        Font::draw_string(fb, right_x + 90, iy, value, vc);
        iy += 13;
    };

    // [ STATS ] — hero numbers
    isection("[ STATS ]");
    // Big verts / faces / tris numerics
    snprintf(buf, sizeof(buf), "%d", state.mesh.vertex_count());
    Font::draw_string(fb, right_x + 12, iy, "VERTS", Theme::TEXT_DIM);
    Font::draw_string(fb, right_x + RIGHT_W - 12 - Font::text_width(buf)*2, iy - 4,
                      buf, Theme::HIGHLIGHT, 2); iy += 18;
    snprintf(buf, sizeof(buf), "%d", state.mesh.face_count());
    Font::draw_string(fb, right_x + 12, iy, "FACES", Theme::TEXT_DIM);
    Font::draw_string(fb, right_x + RIGHT_W - 12 - Font::text_width(buf)*2, iy - 4,
                      buf, Theme::ACCENT, 2); iy += 18;
    snprintf(buf, sizeof(buf), "%d", state.mesh.face_count() * 3);
    Font::draw_string(fb, right_x + 12, iy, "TRIS", Theme::TEXT_DIM);
    Font::draw_string(fb, right_x + RIGHT_W - 12 - Font::text_width(buf)*2, iy - 4,
                      buf, Theme::SUCCESS, 2); iy += 22;

    // [ SELECTION ]
    isection("[ SELECTION ]");
    if (state.edit_mode == AppState::EDIT_MODE &&
        state.select_mode == AppState::SEL_VERT &&
        state.selected_vertex >= 0 &&
        state.selected_vertex < state.mesh.vertex_count()) {
        Vec3 v = state.mesh.verts[state.selected_vertex];
        snprintf(buf, sizeof(buf), "v%d", state.selected_vertex); irow("INDEX", buf);
        snprintf(buf, sizeof(buf), "%+6.3f", v.x); irow("X", buf);
        snprintf(buf, sizeof(buf), "%+6.3f", v.y); irow("Y", buf);
        snprintf(buf, sizeof(buf), "%+6.3f", v.z); irow("Z", buf);
    } else if (state.edit_mode == AppState::EDIT_MODE &&
               state.select_mode == AppState::SEL_FACE &&
               state.selected_face >= 0 &&
               state.selected_face < state.mesh.face_count()) {
        Vec3 c = state.mesh.face_center(state.selected_face);
        Vec3 n = state.mesh.face_normal(state.selected_face);
        snprintf(buf, sizeof(buf), "f%d", state.selected_face); irow("INDEX", buf);
        snprintf(buf, sizeof(buf), "%+5.2f,%+5.2f,%+5.2f", c.x, c.y, c.z);
        Font::draw_string(fb, right_x + 12, iy, "CENTER", Theme::TEXT_DIM); iy += 11;
        Font::draw_string(fb, right_x + 12, iy, buf, Theme::TEXT); iy += 14;
        snprintf(buf, sizeof(buf), "%+5.2f,%+5.2f,%+5.2f", n.x, n.y, n.z);
        Font::draw_string(fb, right_x + 12, iy, "NORMAL", Theme::TEXT_DIM); iy += 11;
        Font::draw_string(fb, right_x + 12, iy, buf, Theme::TEXT); iy += 14;
    } else {
        Font::draw_string(fb, right_x + 12, iy,
                          editing ? "click to select" : "TAB = edit mode",
                          Theme::TEXT_DIM);
        iy += 20;
    }
    iy += 6;

    // [ FRAME TIME ]
    isection("[ FRAME TIME ]");
    draw_frame_histogram(fb, right_x + 12, iy, RIGHT_W - 24, 48,
                         state.frame_hist, AppState::FT_HIST, state.frame_hist_i);
    iy += 52;
    Font::draw_string(fb, right_x + 12, iy, "target 60fps", Theme::TEXT_DIM);
    iy += 16;

    // [ OPS ]
    isection("[ OPS ]");
    auto op = [&](const char* key, const char* name) {
        Font::draw_string(fb, right_x + 12, iy, key, Theme::HIGHLIGHT);
        Font::draw_string(fb, right_x + 40, iy, name, Theme::TEXT);
        iy += 12;
    };
    op("TAB", "toggle edit");
    op("V/F/B", "vert/face");
    op("LMB", "pick");
    op("ARR", "nudge");
    op("E",   "extrude");
    op(",",   "subdivide");
    op(".",   "inset");
    op("Z",   "reset mesh");

    // ---------- Viewport frame ----------
    int vx = state.viewport_x, vy = state.viewport_y;
    int vw = state.viewport_w, vh = state.viewport_h;

    // Outer glow rail — wider, brighter
    for (int g = 1; g <= 5; g++) {
        double glow = (6.0 - g) / 6.0;
        glow = glow * glow * 0.35;
        TGAColor gc = Theme::ACCENT * glow;
        for (int x = vx - g; x < vx + vw + g; x++) {
            TGAColor ct = UIFx::add_colors(fb.get(x, vy - g), gc);
            fb.set(x, vy - g, ct);
            TGAColor cb = UIFx::add_colors(fb.get(x, vy + vh + g - 1), gc);
            fb.set(x, vy + vh + g - 1, cb);
        }
        for (int y = vy - g; y < vy + vh + g; y++) {
            TGAColor cl = UIFx::add_colors(fb.get(vx - g, y), gc);
            fb.set(vx - g, y, cl);
            TGAColor cr = UIFx::add_colors(fb.get(vx + vw + g - 1, y), gc);
            fb.set(vx + vw + g - 1, y, cr);
        }
    }

    // Corner brackets — the real viewport frame
    draw_corner_brackets(fb, vx - 1, vy - 1, vw + 2, vh + 2, 22, Theme::ACCENT);
    // Inset shadow for recessed viewport feel
    UIFx::draw_inner_shadow(fb, vx, vy, vw, vh, 8, 0.20);

    // HUD watermark inside the viewport (top-left)
    snprintf(buf, sizeof(buf), editing ? "// EDIT MODE" : "// OBJECT MODE");
    Font::draw_string(fb, vx + 14, vy + 14, buf,
                      editing ? Theme::WARNING : Theme::ACCENT);
    snprintf(buf, sizeof(buf), "%s  /  %s",
             editing ? (state.select_mode == AppState::SEL_VERT ? "VERTEX" : "FACE")
                     : state.mode_name(),
             state.primitive_name());
    Font::draw_string(fb, vx + 14, vy + 28, buf, Theme::TEXT_DIM);

    // HUD meta in the top-right of the viewport
    snprintf(buf, sizeof(buf), "FOV %.0f  %dx%d", state.fov_deg, vw, vh);
    Font::draw_string(fb, vx + vw - Font::text_width(buf) - 14, vy + 14,
                      buf, Theme::TEXT_DIM);
    snprintf(buf, sizeof(buf), "%d verts / %d faces",
             state.mesh.vertex_count(), state.mesh.face_count());
    Font::draw_string(fb, vx + vw - Font::text_width(buf) - 14, vy + 28,
                      buf, Theme::TEXT);

    // Bottom HUD inside the viewport — shortcut strip (edit mode only)
    if (editing) {
        const char* hints = "ARR move  E extrude  , subdiv  . inset  Z reset";
        int tw = Font::text_width(hints);
        int bx = vx + vw/2 - tw/2;
        int by = vy + vh - 18;
        for (int px = -8; px < tw + 8; px++)
            for (int py = -3; py < 12; py++)
                fb.set(bx + px, by + py, Theme::BG_DARK);
        Font::draw_string(fb, bx, by, hints, Theme::HIGHLIGHT);
    }

    // Mascot now lives in the left sidebar (drawn above)

    // ---------- Bottom breadcrumb footer ----------
    int fy = H - FOOTER_H;
    UIFx::draw_panel_pro(fb, 0, fy, W, FOOTER_H, Theme::BG_PANEL, Theme::GRID, Theme::ACCENT, 3);
    UIFx::draw_glow_bar(fb, 0, fy + 1, W, 3, Theme::ACCENT, 0.35);
    // Breadcrumb chips
    auto chip = [&](int x, const char* label, const TGAColor& col) {
        int tw = Font::text_width(label);
        for (int py = 8; py < 22; py++)
            for (int px = -6; px < tw + 6; px++)
                fb.set(x + px, fy + py, col * 0.28);
        for (int px = -6; px < tw + 6; px++) {
            fb.set(x + px, fy + 8,  col);
            fb.set(x + px, fy + 21, col * 0.4);
        }
        Font::draw_string(fb, x, fy + 11, label, col);
        return x + tw + 20;
    };
    int cx = 14;
    cx = chip(cx, "SCENE", Theme::ACCENT);
    cx = chip(cx, state.primitive_name(), Theme::HIGHLIGHT);
    cx = chip(cx, editing ? "EDIT" : "OBJECT",
              editing ? Theme::WARNING : Theme::SUCCESS);
    cx = chip(cx, state.mode_name(), Theme::TEXT);
    if (state.render_mode == AppState::RAYTRACE) {
        snprintf(buf, sizeof(buf), "SPP %d/64", state.rt_samples);
        cx = chip(cx, buf, state.rt_samples >= 64 ? Theme::SUCCESS : Theme::WARNING);
    }

    snprintf(buf, sizeof(buf), "FPS %.1f  |  %.2fms  |  CAM %+5.1f %+5.1f %+5.1f  dist %.1f",
             state.fps, state.frame_time * 1000.0,
             state.camera.position.x, state.camera.position.y,
             state.camera.position.z, state.camera.distance);
    Font::draw_string(fb, W - Font::text_width(buf) - 14, fy + 11, buf, Theme::TEXT_DIM);
}

// ============================================================
// Mouse picking — nearest projected vertex / face within radius
// ============================================================
static int pick_vertex(ExtState& state, int mouse_x, int mouse_y) {
    if (!state.picking_ready) return -1;
    double lx = mouse_x - state.viewport_x;
    double ly = mouse_y - state.viewport_y;
    int best = -1;
    double best_d = 14.0 * 14.0;
    for (int i = 0; i < (int)state.screen_verts.size(); i++) {
        if (state.depth_verts[i] <= 0) continue;
        double dx = state.screen_verts[i].x - lx;
        double dy = state.screen_verts[i].y - ly;
        double d  = dx*dx + dy*dy;
        if (d < best_d) { best_d = d; best = i; }
    }
    return best;
}

static int pick_face(ExtState& state, int mouse_x, int mouse_y) {
    if (!state.picking_ready) return -1;
    double lx = mouse_x - state.viewport_x;
    double ly = mouse_y - state.viewport_y;
    int best = -1;
    double best_d = 1e18;
    for (int fi = 0; fi < state.mesh.face_count(); fi++) {
        const auto& t = state.mesh.faces[fi];
        if (state.depth_verts[t[0]] <= 0 ||
            state.depth_verts[t[1]] <= 0 ||
            state.depth_verts[t[2]] <= 0) continue;
        Vec3 a = state.screen_verts[t[0]];
        Vec3 b = state.screen_verts[t[1]];
        Vec3 c = state.screen_verts[t[2]];

        // Backface cull (same as render)
        double sa = (b.x-a.x)*(c.y-a.y) - (c.x-a.x)*(b.y-a.y);
        if (sa <= 0) continue;

        // Barycentric check
        double denom = (b.y-c.y)*(a.x-c.x) + (c.x-b.x)*(a.y-c.y);
        if (std::abs(denom) < 1e-9) continue;
        double u = ((b.y-c.y)*(lx-c.x) + (c.x-b.x)*(ly-c.y)) / denom;
        double v = ((c.y-a.y)*(lx-c.x) + (a.x-c.x)*(ly-c.y)) / denom;
        double w = 1 - u - v;
        if (u >= 0 && v >= 0 && w >= 0) {
            // Depth tie-break using avg screen z
            double z = (a.z + b.z + c.z) / 3.0;
            if (z < best_d) { best_d = z; best = fi; }
        }
    }
    return best;
}

// ============================================================
// Main
// ============================================================
int main(int /*argc*/, char* /*argv*/[]) {
    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
        fprintf(stderr, "SDL_Init failed: %s\n", SDL_GetError());
        return 1;
    }

    ExtState state;

    SDL_Window* win = SDL_CreateWindow(
        "3D Renderer - Mesh Editor v4.0",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        state.width, state.height, SDL_WINDOW_SHOWN);
    if (!win) { SDL_Quit(); return 1; }

    SDL_Surface* scr = SDL_GetWindowSurface(win);
    TGAImage fb(state.width, state.height, TGAImage::RGB);
    TGAImage vp(state.viewport_w, state.viewport_h, TGAImage::RGB);
    ZBuffer  zb(state.viewport_w, state.viewport_h);

    Splash::run_splash(fb, win, scr, state.width, state.height);
    SDL_SetRelativeMouseMode(SDL_FALSE);

    bool running = true;
    double fps_timer = 0;
    int    frame_count = 0;

    while (running) {
        auto t0 = std::chrono::high_resolution_clock::now();

        SDL_Event e;
        bool input = false;
        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_QUIT) running = false;

            // Drag-and-drop .obj files onto the window
            if (e.type == SDL_DROPFILE) {
                char* dropped = e.drop.file;
                std::string path(dropped);
                SDL_free(dropped);
                // Check if it ends with .obj (case-insensitive)
                if (path.size() > 4) {
                    std::string ext = path.substr(path.size() - 4);
                    for (auto& c : ext) c = std::tolower(c);
                    if (ext == ".obj") {
                        auto loaded = EditableMesh::load_obj(path);
                        if (!loaded.verts.empty()) {
                            state.mesh = loaded;
                            state.primitive_idx = -1;  // custom
                            state.selected_vertex = -1;
                            state.selected_face = -1;
                            state.rt_dirty = true;
                        }
                    }
                }
            }

            if (e.type == SDL_KEYDOWN) {
                input = true;
                state.idle_timer = 0;
                bool edit = (state.edit_mode == AppState::EDIT_MODE);
                switch (e.key.keysym.sym) {
                    case SDLK_ESCAPE: running = false; break;
                    case SDLK_1:
                        if (edit) state.render_mode = AppState::WIREFRAME;
                        else      state.set_primitive(0);
                        break;
                    case SDLK_2:
                        if (edit) state.render_mode = AppState::FLAT;
                        else      state.set_primitive(1);
                        break;
                    case SDLK_3:
                        if (edit) state.render_mode = AppState::GOURAUD;
                        else      state.set_primitive(2);
                        break;
                    case SDLK_4:
                        if (edit) state.render_mode = AppState::RAYTRACE;
                        else      state.set_primitive(3);
                        break;
                    case SDLK_5:
                        if (edit) { state.render_mode = AppState::RAYTRACE; state.rt_dirty = true; }
                        break;
                    case SDLK_TAB:
                        state.edit_mode = edit ? AppState::OBJECT_MODE
                                               : AppState::EDIT_MODE;
                        state.auto_spin = false;   // freeze for editing
                        break;
                    case SDLK_v: state.select_mode = AppState::SEL_VERT; break;
                    case SDLK_f:
                        if (edit) state.select_mode = AppState::SEL_FACE;
                        else { state.fov_deg = (state.fov_deg < 70) ? 75.0 :
                                               (state.fov_deg < 85) ? 90.0 : 45.0; }
                        break;
                    case SDLK_b: state.select_mode = AppState::SEL_FACE; break;
                    case SDLK_e:
                        if (edit && state.select_mode == AppState::SEL_FACE &&
                            state.selected_face >= 0) {
                            state.mesh.extrude_face(state.selected_face, 0.25);
                        }
                        break;
                    case SDLK_COMMA:
                        if (edit) state.mesh.subdivide_all();
                        break;
                    case SDLK_PERIOD:
                        if (edit && state.select_mode == AppState::SEL_FACE &&
                            state.selected_face >= 0) {
                            state.mesh.inset_face(state.selected_face, 0.2);
                        }
                        break;
                    case SDLK_z:
                        if (edit) {
                            state.set_primitive(state.primitive_idx);
                        }
                        break;
                    case SDLK_l: state.light_rotate = !state.light_rotate; break;
                    case SDLK_g: state.show_grid    = !state.show_grid;   break;
                    case SDLK_o: state.scanlines    = !state.scanlines;   break;
                    case SDLK_t: Theme::cycle();                          break;
                    case SDLK_SPACE:
                        if (!edit) state.auto_spin = !state.auto_spin;
                        break;
                    case SDLK_r:
                        state.camera = CameraController();
                        state.camera.yaw   = -45.0;
                        state.camera.pitch =  25.0;
                        state.camera.distance = 3.8;
                        state.camera.update();
                        state.model_rot_x = 0;
                        state.model_rot_y = 0.35;
                        state.model_rot_z = 0;
                        break;
                    // Vertex nudge on arrow keys (edit + vert mode)
                    case SDLK_LEFT:
                        if (edit && state.select_mode == AppState::SEL_VERT &&
                            state.selected_vertex >= 0)
                            state.mesh.translate_vertex(state.selected_vertex,
                                                        Vec3(-0.05, 0, 0));
                        break;
                    case SDLK_RIGHT:
                        if (edit && state.select_mode == AppState::SEL_VERT &&
                            state.selected_vertex >= 0)
                            state.mesh.translate_vertex(state.selected_vertex,
                                                        Vec3( 0.05, 0, 0));
                        break;
                    case SDLK_UP:
                        if (edit && state.select_mode == AppState::SEL_VERT &&
                            state.selected_vertex >= 0) {
                            Vec3 d = (e.key.keysym.mod & KMOD_SHIFT)
                                     ? Vec3(0, 0,-0.05) : Vec3(0, 0.05, 0);
                            state.mesh.translate_vertex(state.selected_vertex, d);
                        }
                        break;
                    case SDLK_DOWN:
                        if (edit && state.select_mode == AppState::SEL_VERT &&
                            state.selected_vertex >= 0) {
                            Vec3 d = (e.key.keysym.mod & KMOD_SHIFT)
                                     ? Vec3(0, 0, 0.05) : Vec3(0,-0.05, 0);
                            state.mesh.translate_vertex(state.selected_vertex, d);
                        }
                        break;
                }
            }

            if (e.type == SDL_MOUSEMOTION) {
                state.mouse_x = e.motion.x;
                state.mouse_y = e.motion.y;
                if (e.motion.state & SDL_BUTTON_RMASK) {
                    input = true;
                    state.idle_timer = 0;
                    state.camera.process_mouse(e.motion.xrel, e.motion.yrel);
                }
                // Drag vertex: translate in screen-plane mapped to world
                if (state.dragging_vertex &&
                    state.selected_vertex >= 0 &&
                    (e.motion.state & SDL_BUTTON_LMASK)) {
                    int dx = e.motion.x - state.drag_last_mx;
                    int dy = e.motion.y - state.drag_last_my;
                    state.drag_last_mx = e.motion.x;
                    state.drag_last_my = e.motion.y;
                    // Map screen pixels to model-space movement.
                    // Use camera right/up scaled by distance for intuitive dragging.
                    Vec3 front = normalized(state.camera.target - state.camera.position);
                    Vec3 cam_right = normalized(cross(front, state.camera.up));
                    Vec3 cam_up    = normalized(cross(cam_right, front));
                    double scale = state.camera.distance * 0.0012;
                    Vec3 world_delta = cam_right * (dx * scale) + cam_up * (-dy * scale);
                    state.mesh.translate_vertex(state.selected_vertex, world_delta);
                    input = true; state.idle_timer = 0;
                }
            }

            if (e.type == SDL_MOUSEBUTTONDOWN && e.button.button == SDL_BUTTON_LEFT) {
                // Top-bar render mode buttons
                bool hit_button = false;
                for (size_t i = 0; i < state.mode_buttons.size(); i++)
                    if (state.mode_buttons[i].contains(e.button.x, e.button.y)) {
                        state.render_mode = static_cast<AppState::RenderMode>(i);
                        if (i == AppState::RAYTRACE) state.rt_dirty = true;
                        hit_button = true;
                    }

                // Viewport pick in edit mode
                if (!hit_button &&
                    state.edit_mode == AppState::EDIT_MODE &&
                    e.button.x >= state.viewport_x &&
                    e.button.y >= state.viewport_y &&
                    e.button.x <  state.viewport_x + state.viewport_w &&
                    e.button.y <  state.viewport_y + state.viewport_h) {
                    if (state.select_mode == AppState::SEL_VERT) {
                        int picked = pick_vertex(state, e.button.x, e.button.y);
                        state.selected_vertex = picked;
                        if (picked >= 0) {
                            state.dragging_vertex = true;
                            state.drag_last_mx = e.button.x;
                            state.drag_last_my = e.button.y;
                        }
                    } else {
                        state.selected_face = pick_face(state, e.button.x, e.button.y);
                    }
                }
                input = true; state.idle_timer = 0;
            }

            if (e.type == SDL_MOUSEBUTTONUP && e.button.button == SDL_BUTTON_LEFT) {
                state.dragging_vertex = false;
            }

            if (e.type == SDL_MOUSEWHEEL) {
                state.camera.distance -= e.wheel.y * 0.25;
                state.camera.distance = std::max(0.5, std::min(20.0, state.camera.distance));
                state.camera.update();
            }
        }

        state.update_buttons();

        // Continuous key-held
        const uint8_t* keys = SDL_GetKeyboardState(NULL);
        if (keys[SDL_SCANCODE_W]||keys[SDL_SCANCODE_A]||
            keys[SDL_SCANCODE_S]||keys[SDL_SCANCODE_D]||
            keys[SDL_SCANCODE_Q]||keys[SDL_SCANCODE_E]) {
            // Only drive camera W/A/S/D when not in edit mode (E is extrude there)
            if (state.edit_mode == AppState::OBJECT_MODE) {
                input = true; state.idle_timer = 0;
            }
        }
        if (state.edit_mode == AppState::OBJECT_MODE)
            state.camera.process_keyboard(keys);

        double rot_speed = 0.04;
        if (state.edit_mode == AppState::OBJECT_MODE) {
            if (keys[SDL_SCANCODE_J]) { state.model_rot_y -= rot_speed; state.auto_spin = false; }
            if (keys[SDL_SCANCODE_K]) { state.model_rot_y += rot_speed; state.auto_spin = false; }
            if (keys[SDL_SCANCODE_U]) { state.model_rot_x -= rot_speed; state.auto_spin = false; }
            if (keys[SDL_SCANCODE_I]) { state.model_rot_x += rot_speed; state.auto_spin = false; }
            if (keys[SDL_SCANCODE_N]) { state.model_rot_z -= rot_speed; state.auto_spin = false; }
            if (keys[SDL_SCANCODE_M]) { state.model_rot_z += rot_speed; state.auto_spin = false; }
        }

        if (keys[SDL_SCANCODE_LEFTBRACKET])  state.mat_ambient  = std::max(0.0, state.mat_ambient  - 0.005);
        if (keys[SDL_SCANCODE_RIGHTBRACKET]) state.mat_ambient  = std::min(0.5, state.mat_ambient  + 0.005);
        if (keys[SDL_SCANCODE_MINUS])        state.mat_diffuse  = std::max(0.0, state.mat_diffuse  - 0.01);
        if (keys[SDL_SCANCODE_EQUALS])       state.mat_diffuse  = std::min(1.0, state.mat_diffuse  + 0.01);
        if (keys[SDL_SCANCODE_SEMICOLON])    state.mat_specular = std::max(0.0, state.mat_specular - 0.01);
        if (keys[SDL_SCANCODE_APOSTROPHE])   state.mat_specular = std::min(1.0, state.mat_specular + 0.01);

        if (state.auto_spin && state.edit_mode == AppState::OBJECT_MODE)
            state.model_rot_y += state.auto_spin_speed;

        if (!input) state.idle_timer += state.frame_time;

        // UI pulse phase (60fps-independent-ish — driven by wall time indirectly)
        state.ui_phase = std::fmod(state.ui_phase + state.frame_time * 1.1, 1.0);

        // ---- Render ----
        fb.fill(Theme::BG_DARK);
        render_scene(vp, zb, state);
        fb.blit_from(vp, state.viewport_x, state.viewport_y);
        draw_ui(fb, state);

        // Present
        SDL_LockSurface(scr);
        {
            uint32_t* px = static_cast<uint32_t*>(scr->pixels);
            const int W = state.width, H = state.height;
            for (int y = 0; y < H; y++) {
                uint32_t* row = px + y * W;
                for (int x = 0; x < W; x++) {
                    TGAColor c = fb.get(x, y);
                    row[x] = 0xFF000000u | (c[2] << 16) | (c[1] << 8) | c[0];
                }
            }
        }
        SDL_UnlockSurface(scr);
        SDL_UpdateWindowSurface(win);

        auto t1 = std::chrono::high_resolution_clock::now();
        state.frame_time = std::chrono::duration<double>(t1-t0).count();
        state.push_frame_time(state.frame_time);
        fps_timer   += state.frame_time;
        frame_count++;
        if (fps_timer >= 1.0) {
            state.fps   = frame_count / fps_timer;
            fps_timer   = 0; frame_count = 0;
        }
    }

    SDL_DestroyWindow(win);
    SDL_Quit();
    return 0;
}
