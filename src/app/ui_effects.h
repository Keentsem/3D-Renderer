#ifndef UI_EFFECTS_H
#define UI_EFFECTS_H

#include "../image/tgaimage.h"
#include <cstdint>
#include <cmath>
#include <algorithm>

// ============================================================
// UIFx — pixel-level depth, glass, and glow effects
// ============================================================
namespace UIFx {

inline uint8_t noise_value(int x, int y) {
    uint32_t n = (uint32_t)(x * 374761393u + y * 668265263u);
    n = (n ^ (n >> 13)) * 1274126177u;
    return (uint8_t)((n ^ (n >> 16)) & 0xFF);
}

inline TGAColor darken(const TGAColor& c, double f) {
    return TGAColor((uint8_t)(c[2]*f), (uint8_t)(c[1]*f), (uint8_t)(c[0]*f));
}

inline TGAColor lighten(const TGAColor& c, double a) {
    return TGAColor(
        (uint8_t)std::min(255.0, c[2]+255.0*a),
        (uint8_t)std::min(255.0, c[1]+255.0*a),
        (uint8_t)std::min(255.0, c[0]+255.0*a));
}

inline TGAColor add_colors(const TGAColor& a, const TGAColor& b) {
    return TGAColor(
        (uint8_t)std::min(255, a[2]+b[2]),
        (uint8_t)std::min(255, a[1]+b[1]),
        (uint8_t)std::min(255, a[0]+b[0]));
}

// ============================================================
// Dark glass panel — the hero effect. Visible gradient, strong
// inner shadow/bevel, noise grain, accent glow strip along top,
// frosted glass edge highlight.
// ============================================================
inline void draw_panel_pro(TGAImage& img, int x, int y, int w, int h,
                           const TGAColor& bg, const TGAColor& border,
                           const TGAColor& accent, int depth = 2) {
    if (w <= 0 || h <= 0) return;

    // -- Base fill: strong vertical gradient (top bright → bottom dark)
    for (int py = 0; py < h; py++) {
        double t = (double)py / h;
        // Ease-in curve for more dramatic gradient
        double grad = 1.0 - t * t * 0.30;
        for (int px = 0; px < w; px++) {
            double r = bg[2] * grad, g = bg[1] * grad, b = bg[0] * grad;
            // Noise grain (visible but not overwhelming)
            double nv = (noise_value(x + px, y + py) - 128.0) / 255.0;
            r += nv * 3.5; g += nv * 3.5; b += nv * 3.5;
            img.set(x + px, y + py, TGAColor(
                (uint8_t)std::max(0.0, std::min(255.0, r)),
                (uint8_t)std::max(0.0, std::min(255.0, g)),
                (uint8_t)std::max(0.0, std::min(255.0, b))));
        }
    }

    // -- Heavy inner shadow along top + left (makes it feel pressed in)
    int shadow_d = 3 + depth * 2;
    for (int py = 0; py < std::min(shadow_d, h); py++) {
        double s = 1.0 - (double)py / shadow_d;
        s = s * s * 0.35 * depth;  // strong quadratic falloff
        for (int px = 0; px < w; px++) {
            TGAColor c = img.get(x + px, y + py);
            img.set(x + px, y + py, darken(c, 1.0 - s));
        }
    }
    for (int px = 0; px < std::min(shadow_d - 1, w); px++) {
        double s = 1.0 - (double)px / (shadow_d - 1);
        s = s * s * 0.22 * depth;
        for (int py = shadow_d; py < h; py++) {
            TGAColor c = img.get(x + px, y + py);
            img.set(x + px, y + py, darken(c, 1.0 - s));
        }
    }

    // -- Bottom/right inner highlight (catch-light bevel)
    for (int py = std::max(0, h - 3); py < h; py++) {
        double s = (3 - (h - 1 - py)) / 3.0 * 0.06 * depth;
        for (int px = 0; px < w; px++) {
            TGAColor c = img.get(x + px, y + py);
            img.set(x + px, y + py, lighten(c, s));
        }
    }
    for (int px = std::max(0, w - 2); px < w; px++) {
        double s = (2 - (w - 1 - px)) / 2.0 * 0.04 * depth;
        for (int py = 0; py < h; py++) {
            TGAColor c = img.get(x + px, y + py);
            img.set(x + px, y + py, lighten(c, s));
        }
    }

    // -- Accent glow strip along the top (frosted glass highlight)
    for (int py = 0; py < std::min(6, h); py++) {
        double glow = (6.0 - py) / 6.0;
        glow = glow * glow * 0.18 * depth;  // visible!
        for (int px = 0; px < w; px++) {
            TGAColor c = img.get(x + px, y + py);
            img.set(x + px, y + py, TGAColor(
                (uint8_t)std::min(255.0, c[2] + accent[2] * glow),
                (uint8_t)std::min(255.0, c[1] + accent[1] * glow),
                (uint8_t)std::min(255.0, c[0] + accent[0] * glow)));
        }
    }

    // -- Outer border (bright edge on top/left, dark on bottom/right)
    TGAColor bright_edge = lighten(border, 0.15);
    TGAColor dark_edge   = darken(border, 0.5);
    // Top
    for (int px = 0; px < w; px++) img.set(x + px, y, bright_edge);
    // Left
    for (int py = 0; py < h; py++) img.set(x, y + py, bright_edge);
    // Bottom
    for (int px = 0; px < w; px++) img.set(x + px, y + h - 1, dark_edge);
    // Right
    for (int py = 0; py < h; py++) img.set(x + w - 1, y + py, dark_edge);

    // -- Inner border glow line (1px inside, accent-tinted)
    TGAColor inner_glow = TGAColor(
        (uint8_t)(accent[2] * 0.12), (uint8_t)(accent[1] * 0.12), (uint8_t)(accent[0] * 0.12));
    for (int px = 1; px < w - 1; px++) {
        TGAColor c = img.get(x + px, y + 1);
        img.set(x + px, y + 1, add_colors(c, inner_glow));
    }
}

// Embossed horizontal divider: dark line + light line underneath
inline void draw_emboss_line(TGAImage& img, int x1, int y, int x2,
                             const TGAColor& dark, const TGAColor& light) {
    for (int x = x1; x < x2; x++) {
        img.set(x, y,     dark);
        img.set(x, y + 1, light);
    }
}

// Inner shadow rectangle (viewport inset)
inline void draw_inner_shadow(TGAImage& img, int x, int y, int w, int h,
                              int radius, double intensity) {
    for (int py = 0; py < h; py++) {
        for (int px = 0; px < w; px++) {
            double dt = (py < radius) ? (double)py / radius : 1.0;
            double db = (py >= h - radius) ? (double)(h-1-py) / radius : 1.0;
            double dl = (px < radius) ? (double)px / radius : 1.0;
            double dr = (px >= w - radius) ? (double)(w-1-px) / radius : 1.0;
            double d = std::min({dt, db, dl, dr});
            if (d < 1.0) {
                double factor = 1.0 - (1.0 - d) * intensity;
                TGAColor c = img.get(x + px, y + py);
                img.set(x + px, y + py, darken(c, factor));
            }
        }
    }
}

// Horizontal glow bar — draws a soft horizontal glow stripe
inline void draw_glow_bar(TGAImage& img, int x, int y, int w, int thickness,
                          const TGAColor& color, double intensity) {
    for (int py = -thickness; py <= thickness; py++) {
        double falloff = 1.0 - std::abs((double)py / thickness);
        falloff *= falloff * intensity;
        TGAColor gc = color * falloff;
        for (int px = 0; px < w; px++) {
            TGAColor c = img.get(x + px, y + py);
            img.set(x + px, y + py, add_colors(c, gc));
        }
    }
}

// Value bar with glow fill and embossed track
inline void draw_value_bar_pro(TGAImage& img, int x, int y, int w,
                               double value, double vmin, double vmax,
                               const TGAColor& fill_color) {
    int h = 5;
    double norm = (value - vmin) / std::max(1e-9, vmax - vmin);
    norm = std::max(0.0, std::min(1.0, norm));
    int fill_w = (int)(w * norm);

    // Dark recessed track
    for (int py = 0; py < h; py++)
        for (int px = 0; px < w; px++) {
            double dark = 0.25 + (double)py / h * 0.1;
            img.set(x + px, y + py, TGAColor(
                (uint8_t)(fill_color[2] * dark * 0.3),
                (uint8_t)(fill_color[1] * dark * 0.3),
                (uint8_t)(fill_color[0] * dark * 0.3)));
        }

    // Bright fill with vertical gradient
    for (int py = 0; py < h; py++) {
        double g = 0.6 + 0.4 * (1.0 - (double)py / h);
        for (int px = 0; px < fill_w; px++)
            img.set(x + px, y + py, fill_color * g);
    }
    // Top highlight on fill
    if (fill_w > 2)
        for (int px = 1; px < fill_w - 1; px++)
            img.set(x + px, y, lighten(fill_color, 0.2));

    // Emboss edges
    for (int px = 0; px < w; px++) {
        TGAColor c = img.get(x + px, y);
        img.set(x + px, y, darken(c, 0.7));
        c = img.get(x + px, y + h - 1);
        img.set(x + px, y + h - 1, lighten(c, 0.08));
    }
}

} // namespace UIFx

#endif
