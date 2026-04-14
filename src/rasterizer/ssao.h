#ifndef SSAO_H
#define SSAO_H

#include "../core/vec.h"
#include "../image/tgaimage.h"
#include "zbuffer.h"
#include <algorithm>
#include <cmath>

// ============================================================
// Screen-Space Ambient Occlusion (SSAO)
// ============================================================

class SSAO {
public:
    // Compute AO factor for a single pixel
    static double compute(const ZBuffer& zbuf, int x, int y, int radius = 5) {
        double center_depth = zbuf.get(x, y);
        if (center_depth < -1e20) return 1.0;  // No geometry

        int occluded = 0;
        int total = 0;

        for (int dx = -radius; dx <= radius; dx++) {
            for (int dy = -radius; dy <= radius; dy++) {
                if (dx == 0 && dy == 0) continue;

                double sample_depth = zbuf.get(x + dx, y + dy);
                if (sample_depth < -1e20) continue;

                total++;
                // If nearby pixel is in front, it occludes us
                if (sample_depth > center_depth + 0.01) {
                    occluded++;
                }
            }
        }

        if (total == 0) return 1.0;
        return 1.0 - static_cast<double>(occluded) / total * 0.6;
    }

    // Apply SSAO to entire image
    static void apply(TGAImage& image, const ZBuffer& zbuf, int radius = 5) {
        TGAImage temp(image.width(), image.height(), TGAImage::RGB);

        // Copy original image
        for (int y = 0; y < image.height(); y++) {
            for (int x = 0; x < image.width(); x++) {
                temp.set(x, y, image.get(x, y));
            }
        }

        // Apply AO
        for (int y = 0; y < image.height(); y++) {
            for (int x = 0; x < image.width(); x++) {
                double ao = compute(zbuf, x, y, radius);
                TGAColor c = temp.get(x, y);
                image.set(x, y, c * ao);
            }
        }
    }

    // Generate AO visualization (for debugging)
    static void visualize(TGAImage& image, const ZBuffer& zbuf, int radius = 5) {
        for (int y = 0; y < image.height(); y++) {
            for (int x = 0; x < image.width(); x++) {
                double ao = compute(zbuf, x, y, radius);
                uint8_t val = static_cast<uint8_t>(255 * ao);
                image.set(x, y, TGAColor(val, val, val));
            }
        }
    }
};

#endif
