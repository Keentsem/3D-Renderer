#ifndef ZBUFFER_H
#define ZBUFFER_H

#include "../core/vec.h"
#include "../image/tgaimage.h"
#include "triangle.h"
#include <vector>
#include <limits>
#include <algorithm>

// ============================================================
// Z-Buffer Class
// ============================================================

class ZBuffer {
public:
    ZBuffer(int w, int h) : width(w), height(h) {
        // Initialize to -infinity (infinitely far away)
        buffer.resize(w * h, -std::numeric_limits<double>::max());
    }

    void clear() {
        std::fill(buffer.begin(), buffer.end(),
                  -std::numeric_limits<double>::max());
    }

    // Test depth and update if closer
    // Returns true if pixel should be drawn
    bool test_and_set(int x, int y, double z) {
        if (x < 0 || x >= width || y < 0 || y >= height)
            return false;

        int idx = x + y * width;
        if (z > buffer[idx]) {  // Closer to camera
            buffer[idx] = z;
            return true;
        }
        return false;
    }

    double get(int x, int y) const {
        if (x < 0 || x >= width || y < 0 || y >= height)
            return -std::numeric_limits<double>::max();
        return buffer[x + y * width];
    }

    void set(int x, int y, double z) {
        if (x < 0 || x >= width || y < 0 || y >= height) return;
        buffer[x + y * width] = z;
    }

    // Visualize depth as grayscale image (for debugging)
    void to_image(TGAImage& img) const {
        // Find min/max depth
        double minz = std::numeric_limits<double>::max();
        double maxz = -std::numeric_limits<double>::max();

        for (double z : buffer) {
            if (z > -std::numeric_limits<double>::max() / 2) {
                minz = std::min(minz, z);
                maxz = std::max(maxz, z);
            }
        }

        if (maxz <= minz) return;  // No valid depth

        // Map depth to grayscale
        for (int i = 0; i < width * height; i++) {
            if (buffer[i] > -std::numeric_limits<double>::max() / 2) {
                double normalized = (buffer[i] - minz) / (maxz - minz);
                uint8_t val = static_cast<uint8_t>(255 * normalized);
                img.set(i % width, i / width, TGAColor(val));
            }
        }
    }

    int get_width() const { return width; }
    int get_height() const { return height; }

private:
    int width, height;
    std::vector<double> buffer;
};

// ============================================================
// Triangle with Z-Buffer
// ============================================================

void triangle_zbuf(const Vec3 pts[3], ZBuffer& zbuf, TGAImage& image,
                   const TGAColor& color) {
    // Bounding box
    Vec2 bboxmin(image.width() - 1, image.height() - 1);
    Vec2 bboxmax(0, 0);

    for (int i = 0; i < 3; i++) {
        bboxmin.x = std::max(0.0, std::min(bboxmin.x, pts[i].x));
        bboxmin.y = std::max(0.0, std::min(bboxmin.y, pts[i].y));
        bboxmax.x = std::min((double)image.width() - 1, std::max(bboxmax.x, pts[i].x));
        bboxmax.y = std::min((double)image.height() - 1, std::max(bboxmax.y, pts[i].y));
    }

    // 2D vertices for barycentric
    Vec2 pts2d[3] = {
        Vec2(pts[0].x, pts[0].y),
        Vec2(pts[1].x, pts[1].y),
        Vec2(pts[2].x, pts[2].y)
    };

    // Rasterize
    for (int x = (int)bboxmin.x; x <= (int)bboxmax.x; x++) {
        for (int y = (int)bboxmin.y; y <= (int)bboxmax.y; y++) {
            Vec2 P(x, y);
            Vec3 bc = barycentric(pts2d, P);

            if (bc.x < 0 || bc.y < 0 || bc.z < 0) continue;

            // Interpolate Z using barycentric coordinates
            double z = pts[0].z * bc.x + pts[1].z * bc.y + pts[2].z * bc.z;

            // Z-buffer test
            if (zbuf.test_and_set(x, y, z)) {
                image.set(x, y, color);
            }
        }
    }
}

// ============================================================
// Variations with Interpolation
// ============================================================

// Interpolate vertex colors
void triangle_zbuf_colored(const Vec3 pts[3], const TGAColor colors[3],
                           ZBuffer& zbuf, TGAImage& image) {
    Vec2 bboxmin(image.width() - 1, image.height() - 1);
    Vec2 bboxmax(0, 0);

    for (int i = 0; i < 3; i++) {
        bboxmin.x = std::max(0.0, std::min(bboxmin.x, pts[i].x));
        bboxmin.y = std::max(0.0, std::min(bboxmin.y, pts[i].y));
        bboxmax.x = std::min((double)image.width() - 1, std::max(bboxmax.x, pts[i].x));
        bboxmax.y = std::min((double)image.height() - 1, std::max(bboxmax.y, pts[i].y));
    }

    Vec2 pts2d[3] = {Vec2(pts[0].x, pts[0].y), Vec2(pts[1].x, pts[1].y), Vec2(pts[2].x, pts[2].y)};

    for (int x = (int)bboxmin.x; x <= (int)bboxmax.x; x++) {
        for (int y = (int)bboxmin.y; y <= (int)bboxmax.y; y++) {
            Vec2 P(x, y);
            Vec3 bc = barycentric(pts2d, P);

            if (bc.x < 0 || bc.y < 0 || bc.z < 0) continue;

            double z = pts[0].z * bc.x + pts[1].z * bc.y + pts[2].z * bc.z;

            if (zbuf.test_and_set(x, y, z)) {
                // Interpolate color
                TGAColor color;
                for (int c = 0; c < 3; c++) {
                    double val = colors[0][c] * bc.x +
                                 colors[1][c] * bc.y +
                                 colors[2][c] * bc.z;
                    color[c] = static_cast<uint8_t>(std::min(255.0, std::max(0.0, val)));
                }
                color[3] = 255;
                image.set(x, y, color);
            }
        }
    }
}

// Interpolate lighting intensity (Gouraud shading)
void triangle_zbuf_gouraud(const Vec3 pts[3], const double intensities[3],
                           const TGAColor& base_color, ZBuffer& zbuf, TGAImage& image) {
    Vec2 bboxmin(image.width() - 1, image.height() - 1);
    Vec2 bboxmax(0, 0);

    for (int i = 0; i < 3; i++) {
        bboxmin.x = std::max(0.0, std::min(bboxmin.x, pts[i].x));
        bboxmin.y = std::max(0.0, std::min(bboxmin.y, pts[i].y));
        bboxmax.x = std::min((double)image.width() - 1, std::max(bboxmax.x, pts[i].x));
        bboxmax.y = std::min((double)image.height() - 1, std::max(bboxmax.y, pts[i].y));
    }

    Vec2 pts2d[3] = {Vec2(pts[0].x, pts[0].y), Vec2(pts[1].x, pts[1].y), Vec2(pts[2].x, pts[2].y)};

    for (int x = (int)bboxmin.x; x <= (int)bboxmax.x; x++) {
        for (int y = (int)bboxmin.y; y <= (int)bboxmax.y; y++) {
            Vec2 P(x, y);
            Vec3 bc = barycentric(pts2d, P);

            if (bc.x < 0 || bc.y < 0 || bc.z < 0) continue;

            double z = pts[0].z * bc.x + pts[1].z * bc.y + pts[2].z * bc.z;

            if (zbuf.test_and_set(x, y, z)) {
                double intensity = intensities[0] * bc.x +
                                   intensities[1] * bc.y +
                                   intensities[2] * bc.z;
                intensity = std::max(0.0, std::min(1.0, intensity));
                image.set(x, y, base_color * intensity);
            }
        }
    }
}

#endif
