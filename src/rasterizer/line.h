#ifndef LINE_H
#define LINE_H

#include "../image/tgaimage.h"
#include "../core/vec.h"
#include <cmath>
#include <algorithm>

// ============================================================
// Bresenham's Line Drawing Algorithm
// From TinyRenderer Lesson 1
// ============================================================

void line(int x0, int y0, int x1, int y1, TGAImage& image, const TGAColor& color) {
    bool steep = false;

    // STEP 1: Handle steep lines by transposing
    // If line is more vertical than horizontal, swap x and y
    if (std::abs(x0 - x1) < std::abs(y0 - y1)) {
        std::swap(x0, y0);
        std::swap(x1, y1);
        steep = true;
    }

    // STEP 2: Ensure we draw left to right
    if (x0 > x1) {
        std::swap(x0, x1);
        std::swap(y0, y1);
    }

    int dx = x1 - x0;
    int dy = y1 - y0;

    // STEP 3: Integer-only error tracking
    // derror2 = 2 * |dy| (multiply by 2 to avoid 0.5)
    int derror2 = std::abs(dy) * 2;
    int error2 = 0;
    int y = y0;

    // STEP 4: Draw the line
    for (int x = x0; x <= x1; x++) {
        if (steep) {
            image.set(y, x, color);  // De-transpose
        } else {
            image.set(x, y, color);
        }

        error2 += derror2;
        if (error2 > dx) {
            y += (y1 > y0 ? 1 : -1);  // Step up or down
            error2 -= dx * 2;
        }
    }
}

// Convenience overloads
void line(Vec2 p0, Vec2 p1, TGAImage& image, const TGAColor& color) {
    line(static_cast<int>(p0.x + 0.5), static_cast<int>(p0.y + 0.5),
         static_cast<int>(p1.x + 0.5), static_cast<int>(p1.y + 0.5),
         image, color);
}

void line(Vec2i p0, Vec2i p1, TGAImage& image, const TGAColor& color) {
    line(p0.x, p0.y, p1.x, p1.y, image, color);
}

void line(Vec3 p0, Vec3 p1, TGAImage& image, const TGAColor& color) {
    line(static_cast<int>(p0.x + 0.5), static_cast<int>(p0.y + 0.5),
         static_cast<int>(p1.x + 0.5), static_cast<int>(p1.y + 0.5),
         image, color);
}

// ============================================================
// Wu's Anti-Aliased Line Algorithm (Optional)
// Draws smoother lines by varying pixel intensity
// ============================================================

namespace wu {

inline double fpart(double x) { return x - std::floor(x); }
inline double rfpart(double x) { return 1.0 - fpart(x); }

void line_aa(double x0, double y0, double x1, double y1,
             TGAImage& image, const TGAColor& color) {
    bool steep = std::abs(y1 - y0) > std::abs(x1 - x0);

    if (steep) { std::swap(x0, y0); std::swap(x1, y1); }
    if (x0 > x1) { std::swap(x0, x1); std::swap(y0, y1); }

    double dx = x1 - x0;
    double dy = y1 - y0;
    double gradient = (dx < 1e-10) ? 1.0 : dy / dx;

    // First endpoint
    double xend = std::round(x0);
    double yend = y0 + gradient * (xend - x0);
    double xgap = rfpart(x0 + 0.5);
    int xpxl1 = static_cast<int>(xend);
    int ypxl1 = static_cast<int>(std::floor(yend));

    if (steep) {
        image.set(ypxl1, xpxl1, color * (rfpart(yend) * xgap));
        image.set(ypxl1 + 1, xpxl1, color * (fpart(yend) * xgap));
    } else {
        image.set(xpxl1, ypxl1, color * (rfpart(yend) * xgap));
        image.set(xpxl1, ypxl1 + 1, color * (fpart(yend) * xgap));
    }

    double intery = yend + gradient;

    // Second endpoint
    xend = std::round(x1);
    yend = y1 + gradient * (xend - x1);
    xgap = fpart(x1 + 0.5);
    int xpxl2 = static_cast<int>(xend);
    int ypxl2 = static_cast<int>(std::floor(yend));

    if (steep) {
        image.set(ypxl2, xpxl2, color * (rfpart(yend) * xgap));
        image.set(ypxl2 + 1, xpxl2, color * (fpart(yend) * xgap));
    } else {
        image.set(xpxl2, ypxl2, color * (rfpart(yend) * xgap));
        image.set(xpxl2, ypxl2 + 1, color * (fpart(yend) * xgap));
    }

    // Main loop
    if (steep) {
        for (int x = xpxl1 + 1; x < xpxl2; x++) {
            int iy = static_cast<int>(std::floor(intery));
            image.set(iy, x, color * rfpart(intery));
            image.set(iy + 1, x, color * fpart(intery));
            intery += gradient;
        }
    } else {
        for (int x = xpxl1 + 1; x < xpxl2; x++) {
            int iy = static_cast<int>(std::floor(intery));
            image.set(x, iy, color * rfpart(intery));
            image.set(x, iy + 1, color * fpart(intery));
            intery += gradient;
        }
    }
}

} // namespace wu

#endif
