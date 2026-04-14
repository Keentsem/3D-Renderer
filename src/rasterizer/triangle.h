#ifndef TRIANGLE_H
#define TRIANGLE_H

#include "../core/vec.h"
#include "../image/tgaimage.h"
#include <algorithm>
#include <cmath>

// ============================================================
// Barycentric Coordinates
// ============================================================

Vec3 barycentric(const Vec2 pts[3], const Vec2& P) {
    Vec3 s0(pts[2].x - pts[0].x, pts[1].x - pts[0].x, pts[0].x - P.x);
    Vec3 s1(pts[2].y - pts[0].y, pts[1].y - pts[0].y, pts[0].y - P.y);

    Vec3 u = cross(s0, s1);

    // Degenerate triangle (all points collinear)
    if (std::abs(u.z) < 1e-2)
        return Vec3(-1, 1, 1);

    return Vec3(
        1.0 - (u.x + u.y) / u.z,
        u.y / u.z,
        u.x / u.z
    );
}

// Overload for Vec3 (ignores z, useful for screen coordinates)
Vec3 barycentric(const Vec3 pts[3], const Vec3& P) {
    Vec2 pts2d[3] = {
        Vec2(pts[0].x, pts[0].y),
        Vec2(pts[1].x, pts[1].y),
        Vec2(pts[2].x, pts[2].y)
    };
    return barycentric(pts2d, Vec2(P.x, P.y));
}

// ============================================================
// Triangle Rasterization - Bounding Box Method
// ============================================================

void triangle(const Vec2 pts[3], TGAImage& image, const TGAColor& color) {
    // STEP 1: Compute bounding box
    Vec2 bboxmin(image.width() - 1, image.height() - 1);
    Vec2 bboxmax(0, 0);
    Vec2 clamp(image.width() - 1, image.height() - 1);

    for (int i = 0; i < 3; i++) {
        bboxmin.x = std::max(0.0, std::min(bboxmin.x, pts[i].x));
        bboxmin.y = std::max(0.0, std::min(bboxmin.y, pts[i].y));
        bboxmax.x = std::min(clamp.x, std::max(bboxmax.x, pts[i].x));
        bboxmax.y = std::min(clamp.y, std::max(bboxmax.y, pts[i].y));
    }

    // STEP 2: Test each pixel in bounding box
    Vec2 P;
    for (P.x = bboxmin.x; P.x <= bboxmax.x; P.x++) {
        for (P.y = bboxmin.y; P.y <= bboxmax.y; P.y++) {
            Vec3 bc = barycentric(pts, P);

            // STEP 3: Skip if outside triangle
            if (bc.x < 0 || bc.y < 0 || bc.z < 0) continue;

            // STEP 4: Draw pixel
            image.set(static_cast<int>(P.x), static_cast<int>(P.y), color);
        }
    }
}

// Integer coordinate overload
void triangle(const Vec2i pts[3], TGAImage& image, const TGAColor& color) {
    Vec2 fpts[3] = {
        Vec2(pts[0].x, pts[0].y),
        Vec2(pts[1].x, pts[1].y),
        Vec2(pts[2].x, pts[2].y)
    };
    triangle(fpts, image, color);
}

// ============================================================
// Gradient Triangle (Interpolate Colors)
// ============================================================

void triangle_gradient(const Vec2 pts[3], const TGAColor colors[3], TGAImage& image) {
    // STEP 1: Compute bounding box
    Vec2 bboxmin(image.width() - 1, image.height() - 1);
    Vec2 bboxmax(0, 0);
    Vec2 clamp(image.width() - 1, image.height() - 1);

    for (int i = 0; i < 3; i++) {
        bboxmin.x = std::max(0.0, std::min(bboxmin.x, pts[i].x));
        bboxmin.y = std::max(0.0, std::min(bboxmin.y, pts[i].y));
        bboxmax.x = std::min(clamp.x, std::max(bboxmax.x, pts[i].x));
        bboxmax.y = std::min(clamp.y, std::max(bboxmax.y, pts[i].y));
    }

    // STEP 2: Test each pixel in bounding box
    Vec2 P;
    for (P.x = bboxmin.x; P.x <= bboxmax.x; P.x++) {
        for (P.y = bboxmin.y; P.y <= bboxmax.y; P.y++) {
            Vec3 bc = barycentric(pts, P);

            // STEP 3: Skip if outside triangle
            if (bc.x < 0 || bc.y < 0 || bc.z < 0) continue;

            // STEP 4: Interpolate color using barycentric coordinates
            TGAColor color;
            for (int c = 0; c < 3; c++) {
                double val = colors[0][c] * bc.x +
                             colors[1][c] * bc.y +
                             colors[2][c] * bc.z;
                color[c] = static_cast<uint8_t>(val);
            }
            color[3] = 255;

            image.set(static_cast<int>(P.x), static_cast<int>(P.y), color);
        }
    }
}

// ============================================================
// Back-face Culling
// ============================================================

// Returns true if triangle faces AWAY from camera (should be culled)
// Assumes counter-clockwise winding for front faces
bool is_back_face(const Vec3& v0, const Vec3& v1, const Vec3& v2, const Vec3& view_dir) {
    Vec3 edge1 = v1 - v0;
    Vec3 edge2 = v2 - v0;
    Vec3 normal = cross(edge1, edge2);

    // If dot product with view direction > 0, face points away
    return (normal * view_dir) > 0;
}

// Screen-space version (camera looks down -Z)
bool is_back_face(const Vec3& v0, const Vec3& v1, const Vec3& v2) {
    Vec3 edge1 = v1 - v0;
    Vec3 edge2 = v2 - v0;
    Vec3 normal = cross(edge1, edge2);

    // In screen space, +Z points out of screen
    // If normal.z <= 0, triangle faces away
    return normal.z <= 0;
}

// ============================================================
// Utilities
// ============================================================

double triangle_area(const Vec2& a, const Vec2& b, const Vec2& c) {
    return std::abs((b.x - a.x) * (c.y - a.y) - (c.x - a.x) * (b.y - a.y)) / 2.0;
}

double triangle_area(const Vec3& a, const Vec3& b, const Vec3& c) {
    Vec3 ab = b - a;
    Vec3 ac = c - a;
    return cross(ab, ac).norm() / 2.0;
}

// ============================================================
// Scanline Algorithm (Alternative Implementation)
// ============================================================

namespace scanline {

void triangle(Vec2 t0, Vec2 t1, Vec2 t2, TGAImage& image, const TGAColor& color) {
    // Sort by y coordinate
    if (t0.y > t1.y) std::swap(t0, t1);
    if (t0.y > t2.y) std::swap(t0, t2);
    if (t1.y > t2.y) std::swap(t1, t2);

    int total_height = static_cast<int>(t2.y - t0.y);
    if (total_height == 0) return;

    for (int i = 0; i < total_height; i++) {
        bool second_half = i > t1.y - t0.y || t1.y == t0.y;
        int segment_height = second_half ?
            static_cast<int>(t2.y - t1.y) : static_cast<int>(t1.y - t0.y);

        if (segment_height == 0) continue;

        double alpha = static_cast<double>(i) / total_height;
        double beta = static_cast<double>(i - (second_half ? t1.y - t0.y : 0)) / segment_height;

        Vec2 A = t0 + (t2 - t0) * alpha;
        Vec2 B = second_half ? t1 + (t2 - t1) * beta : t0 + (t1 - t0) * beta;

        if (A.x > B.x) std::swap(A, B);

        for (int j = static_cast<int>(A.x); j <= static_cast<int>(B.x); j++) {
            image.set(j, static_cast<int>(t0.y + i), color);
        }
    }
}

} // namespace scanline

#endif
