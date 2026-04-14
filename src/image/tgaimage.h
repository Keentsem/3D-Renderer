#ifndef TGAIMAGE_H
#define TGAIMAGE_H

#include <cstdint>
#include <vector>
#include <string>

#pragma pack(push, 1)
struct TGAHeader {
    uint8_t  idlength = 0;
    uint8_t  colormaptype = 0;
    uint8_t  datatypecode = 0;
    uint16_t colormaporigin = 0;
    uint16_t colormaplength = 0;
    uint8_t  colormapdepth = 0;
    uint16_t x_origin = 0;
    uint16_t y_origin = 0;
    uint16_t width = 0;
    uint16_t height = 0;
    uint8_t  bitsperpixel = 0;
    uint8_t  imagedescriptor = 0;
};
#pragma pack(pop)

struct TGAColor {
    uint8_t bgra[4] = {0, 0, 0, 255};
    uint8_t bytespp = 4;

    TGAColor() = default;
    TGAColor(uint8_t r, uint8_t g, uint8_t b, uint8_t a = 255)
        : bgra{b, g, r, a}, bytespp(4) {}
    TGAColor(uint8_t v) : bgra{v, v, v, 255}, bytespp(1) {}

    uint8_t& operator[](const int i) { return bgra[i]; }
    uint8_t operator[](const int i) const { return bgra[i]; }

    TGAColor operator*(double intensity) const {
        TGAColor res = *this;
        intensity = std::max(0.0, std::min(1.0, intensity));
        for (int i = 0; i < 3; i++)
            res.bgra[i] = static_cast<uint8_t>(bgra[i] * intensity);
        return res;
    }
};

const TGAColor WHITE(255, 255, 255);
const TGAColor BLACK(0, 0, 0);
const TGAColor RED(255, 0, 0);
const TGAColor GREEN(0, 255, 0);
const TGAColor BLUE(0, 0, 255);

class TGAImage {
public:
    enum Format { GRAYSCALE = 1, RGB = 3, RGBA = 4 };

    TGAImage() = default;
    TGAImage(int w, int h, int bpp);

    bool read_tga_file(const std::string& filename);
    bool write_tga_file(const std::string& filename, bool vflip = true, bool rle = true) const;

    void flip_horizontally();
    void flip_vertically();

    TGAColor get(int x, int y) const;
    void set(int x, int y, const TGAColor& c);

    // Fast bulk operations — skip per-pixel bounds checks and function
    // calls. Used by the live render loop for framebuffer clear and blit.
    void fill(const TGAColor& c);
    void blit_from(const TGAImage& src, int dst_x, int dst_y);

    int width() const { return w; }
    int height() const { return h; }

private:
    int w = 0, h = 0, bpp = 0;
    std::vector<uint8_t> data;
};

#endif
