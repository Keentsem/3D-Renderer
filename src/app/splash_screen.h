#ifndef SPLASH_SCREEN_H
#define SPLASH_SCREEN_H

#include "../image/tgaimage.h"
#include "app_blueprint.h"
#include "ui_effects.h"
#include <SDL.h>
#include <cmath>
#include <cstring>

// ============================================================
// Splash — animated loading screen with mascot + progress bar
// ============================================================
namespace Splash {

struct Particle {
    double x, y, vx, vy, life, max_life;
};

static const int N_PARTICLES = 35;

inline void init_particles(Particle* p, int cx, int cy) {
    for (int i = 0; i < N_PARTICLES; i++) {
        double a = (i * 2.399) + 0.5;  // golden angle spread
        double r = 30.0 + (i % 7) * 12.0;
        p[i].x = cx + std::cos(a) * r;
        p[i].y = cy + std::sin(a) * r;
        p[i].vx = std::cos(a + 1.5) * (0.3 + (i % 4) * 0.2);
        p[i].vy = std::sin(a + 1.5) * (0.3 + (i % 5) * 0.15);
        p[i].max_life = 60 + (i % 40);
        p[i].life = p[i].max_life * ((i % 3 + 1) / 3.0);
    }
}

inline void update_particles(Particle* p, int cx, int cy) {
    for (int i = 0; i < N_PARTICLES; i++) {
        p[i].x += p[i].vx;
        p[i].y += p[i].vy;
        p[i].life -= 1.0;
        if (p[i].life <= 0) {
            double a = (i * 2.399) + p[i].max_life * 0.1;
            double r = 20.0 + (i % 9) * 8.0;
            p[i].x = cx + std::cos(a) * r;
            p[i].y = cy + std::sin(a) * r;
            p[i].vx = std::cos(a + 1.5) * 0.5;
            p[i].vy = std::sin(a + 1.5) * 0.5;
            p[i].life = p[i].max_life;
        }
    }
}

inline void draw_particles(TGAImage& img, Particle* p) {
    for (int i = 0; i < N_PARTICLES; i++) {
        if (p[i].life <= 0) continue;
        double alpha = (p[i].life / p[i].max_life);
        TGAColor c = Theme::ACCENT * (alpha * 0.7);
        int px = (int)p[i].x, py = (int)p[i].y;
        img.set(px, py, c);
        img.set(px + 1, py, c);
        img.set(px, py + 1, c);
        if (alpha > 0.6) {
            TGAColor bright = Theme::HIGHLIGHT * (alpha * 0.4);
            img.set(px + 1, py + 1, bright);
        }
    }
}

inline void draw_progress_bar(TGAImage& img, int x, int y, int w, int h,
                              double progress, int frame) {
    // Recessed background
    for (int py = 0; py < h; py++)
        for (int px = 0; px < w; px++) {
            double g = 1.0 - (double)py / h * 0.3;
            img.set(x + px, y + py, Theme::BG_DARK * g);
        }
    // Border
    for (int px = 0; px < w; px++) {
        img.set(x + px, y, Theme::GRID * 0.6);
        img.set(x + px, y + h - 1, Theme::GRID * 0.3);
    }

    // Segmented fill
    int segments = 14;
    int gap = 2;
    int seg_w = (w - gap * (segments - 1)) / segments;
    int filled = (int)(progress * segments);
    for (int s = 0; s < segments; s++) {
        int sx = x + s * (seg_w + gap);
        bool lit = s < filled;
        bool leading = (s == filled && progress > 0);
        if (!lit && !leading) continue;

        double pulse = leading ? 0.5 + 0.5 * std::sin(frame * 0.15) : 1.0;
        TGAColor seg_col = lit ? Theme::ACCENT : Theme::ACCENT * (pulse * 0.6);

        for (int py = 1; py < h - 1; py++) {
            double gy = 1.0 - (double)py / h * 0.4;
            TGAColor row_c = seg_col * gy;
            for (int px = 0; px < seg_w; px++)
                img.set(sx + px, y + py, row_c);
        }
        // Top highlight on lit segments
        if (lit) {
            TGAColor hi = Theme::HIGHLIGHT * 0.35;
            for (int px = 1; px < seg_w - 1; px++)
                img.set(sx + px, y + 1, hi);
        }
    }
}

inline void draw_glow_text(TGAImage& img, int cx, int cy, const char* text,
                           const TGAColor& color, int scale) {
    int tw = Font::text_width(text) * scale;
    int tx = cx - tw / 2;
    // Glow passes
    TGAColor g = color * 0.12;
    for (int dx = -2; dx <= 2; dx++)
        for (int dy = -2; dy <= 2; dy++)
            if (dx*dx + dy*dy <= 4)
                Font::draw_string(img, tx + dx, cy + dy, text, g, scale);
    // Crisp text on top
    Font::draw_string(img, tx, cy, text, color, scale);
}

inline void present(TGAImage& fb, SDL_Window* win, SDL_Surface* scr, int w, int h) {
    SDL_LockSurface(scr);
    uint32_t* px = static_cast<uint32_t*>(scr->pixels);
    for (int y = 0; y < h; y++) {
        uint32_t* row = px + y * w;
        for (int x = 0; x < w; x++) {
            TGAColor c = fb.get(x, y);
            row[x] = 0xFF000000u | (c[2] << 16) | (c[1] << 8) | c[0];
        }
    }
    SDL_UnlockSurface(scr);
    SDL_UpdateWindowSurface(win);
}

inline bool check_skip() {
    SDL_Event e;
    while (SDL_PollEvent(&e)) {
        if (e.type == SDL_QUIT) return true;
        if (e.type == SDL_KEYDOWN) return true;
        if (e.type == SDL_MOUSEBUTTONDOWN) return true;
    }
    return false;
}

// ============================================================
// Main splash: ~180 frames at 35ms = ~6 seconds, skippable
// ============================================================
inline void run_splash(TGAImage& fb, SDL_Window* win, SDL_Surface* scr, int w, int h) {
    const int TOTAL = 180;
    int cx = w / 2, cy = h / 2 - 40;

    Particle particles[N_PARTICLES];
    init_particles(particles, cx, cy);

    const char* checks[] = {
        "[OK] RASTERIZER CORE",
        "[OK] Z-BUFFER SUBSYSTEM",
        "[OK] PHONG PIXEL SHADER",
        "[OK] PROCEDURAL TEXTURES",
        "[OK] EDIT MESH ENGINE",
        "[OK] GHOST AI CORE",
        "[OK] THEME PALETTE SYSTEM",
        "[OK] CAMERA CONTROLLER",
        "[OK] PARTICLE SYSTEM",
        "[  ] INITIALIZING VIEWPORT...",
        "[OK] SDL2 SURFACE READY",
        "[OK] ALL SYSTEMS NOMINAL",
    };
    const int N_CHECKS = 12;

    for (int frame = 0; frame < TOTAL; frame++) {
        if (check_skip()) return;
        fb.fill(TGAColor(0, 0, 0));

        // Background: vignette gradient
        for (int y = 0; y < h; y++) {
            for (int x = 0; x < w; x++) {
                double dx = (x - cx) / (double)w;
                double dy = (y - cy) / (double)h;
                double vignette = 1.0 - std::min(1.0, (dx*dx + dy*dy) * 2.5);
                double r = Theme::BG_DARK[2] * vignette * 0.7;
                double g = Theme::BG_DARK[1] * vignette * 0.7;
                double b = Theme::BG_DARK[0] * vignette * 0.7;
                // Subtle noise
                double nv = (UIFx::noise_value(x, y) - 128.0) / 255.0 * 0.03;
                r *= (1.0 + nv); g *= (1.0 + nv); b *= (1.0 + nv);
                fb.set(x, y, TGAColor(
                    (uint8_t)std::max(0.0, r),
                    (uint8_t)std::max(0.0, g),
                    (uint8_t)std::max(0.0, b)));
            }
        }

        // Mascot (3x scale) — fade in during first 30 frames
        double mascot_alpha = std::min(1.0, frame / 30.0);
        if (mascot_alpha > 0.1) {
            Mascot::State ms = (frame < 120) ? Mascot::RENDERING : Mascot::SUCCESS;
            int mx = cx - 24;  // 3x scale = 48 wide, center it
            int my = cy - 40;
            Mascot::draw_with_glow(fb, mx, my, ms, Theme::TEXT * mascot_alpha, 3);
        }

        // Particles around mascot
        if (frame > 20) {
            update_particles(particles, cx, cy - 16);
            draw_particles(fb, particles);
        }

        // Title text below mascot
        if (frame > 40) {
            double title_alpha = std::min(1.0, (frame - 40) / 20.0);
            draw_glow_text(fb, cx, cy + 40, "3D RENDERER", Theme::ACCENT * title_alpha, 3);
            if (frame > 60)
                draw_glow_text(fb, cx, cy + 72, "MESH EDITOR // PIXEL AI",
                               Theme::TEXT_DIM * std::min(1.0, (frame - 60) / 20.0), 1);
        }

        // Progress bar
        if (frame > 30) {
            int bar_w = 320, bar_h = 12;
            draw_progress_bar(fb, cx - bar_w/2, cy + 92, bar_w, bar_h,
                              std::min(1.0, (frame - 30.0) / 130.0), frame);
        }

        // System check messages
        if (frame > 50) {
            int lines_shown = std::min(N_CHECKS, (frame - 50) / 8 + 1);
            int ty = cy + 116;
            for (int i = 0; i < lines_shown; i++) {
                const char* line = checks[i];
                TGAColor lc = Theme::TEXT_DIM * 0.7;
                // Color the [OK] / [  ] prefix
                if (line[1] == 'O') {
                    Font::draw_string(fb, cx - 140, ty, "[OK]", Theme::SUCCESS);
                    Font::draw_string(fb, cx - 140 + Font::text_width("[OK] "), ty,
                                      line + 5, lc);
                } else {
                    Font::draw_string(fb, cx - 140, ty, "[..]", Theme::WARNING);
                    Font::draw_string(fb, cx - 140 + Font::text_width("[..] "), ty,
                                      line + 5, lc);
                }
                ty += 12;
            }
        }

        // Fade to black at the end
        if (frame > TOTAL - 20) {
            double fade = (double)(frame - (TOTAL - 20)) / 20.0;
            for (int y = 0; y < h; y++) {
                for (int x = 0; x < w; x++) {
                    TGAColor c = fb.get(x, y);
                    fb.set(x, y, c * (1.0 - fade));
                }
            }
        }

        present(fb, win, scr, w, h);
        SDL_Delay(35);
    }
}

} // namespace Splash

#endif
