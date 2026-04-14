#include "image/tgaimage.h"
#include <fstream>
#include <cstring>
#include <iostream>

TGAImage::TGAImage(int w, int h, int bpp) : w(w), h(h), bpp(bpp) {
    data.resize(w * h * bpp, 0);
}

TGAColor TGAImage::get(int x, int y) const {
    if (!data.size() || x < 0 || y < 0 || x >= w || y >= h) {
        return TGAColor();
    }
    TGAColor c;
    c.bytespp = bpp;
    std::memcpy(c.bgra, &data[(x + y * w) * bpp], bpp);
    return c;
}

void TGAImage::set(int x, int y, const TGAColor& c) {
    if (!data.size() || x < 0 || y < 0 || x >= w || y >= h) {
        return;
    }
    std::memcpy(&data[(x + y * w) * bpp], c.bgra, bpp);
}

void TGAImage::fill(const TGAColor& c) {
    if (data.empty() || bpp <= 0) return;
    const int total = w * h;
    uint8_t* base = data.data();
    // Write the first pixel, then memcpy-double outwards (log N passes).
    std::memcpy(base, c.bgra, bpp);
    int filled = 1;
    while (filled < total) {
        int chunk = std::min(filled, total - filled);
        std::memcpy(base + filled * bpp, base, chunk * bpp);
        filled += chunk;
    }
}

void TGAImage::blit_from(const TGAImage& src, int dst_x, int dst_y) {
    if (data.empty() || src.data.empty() || bpp != src.bpp) return;
    const int sw = src.w, sh = src.h;
    // Compute clipped rectangle in destination space.
    int x0 = std::max(0, dst_x);
    int y0 = std::max(0, dst_y);
    int x1 = std::min(w,  dst_x + sw);
    int y1 = std::min(h,  dst_y + sh);
    if (x0 >= x1 || y0 >= y1) return;
    const int row_bytes = (x1 - x0) * bpp;
    for (int y = y0; y < y1; y++) {
        const uint8_t* s = &src.data[((y - dst_y) * sw + (x0 - dst_x)) * bpp];
        uint8_t*       d = &data    [(y * w + x0) * bpp];
        std::memcpy(d, s, row_bytes);
    }
}

void TGAImage::flip_horizontally() {
    int half = w >> 1;
    for (int y = 0; y < h; y++) {
        for (int x = 0; x < half; x++) {
            TGAColor c1 = get(x, y);
            TGAColor c2 = get(w - 1 - x, y);
            set(x, y, c2);
            set(w - 1 - x, y, c1);
        }
    }
}

void TGAImage::flip_vertically() {
    int half = h >> 1;
    for (int y = 0; y < half; y++) {
        for (int x = 0; x < w; x++) {
            TGAColor c1 = get(x, y);
            TGAColor c2 = get(x, h - 1 - y);
            set(x, y, c2);
            set(x, h - 1 - y, c1);
        }
    }
}

bool TGAImage::read_tga_file(const std::string& filename) {
    std::ifstream in(filename, std::ios::binary);
    if (!in.is_open()) {
        std::cerr << "Error: cannot open file " << filename << std::endl;
        return false;
    }

    TGAHeader header;
    in.read(reinterpret_cast<char*>(&header), sizeof(header));
    if (!in.good()) {
        std::cerr << "Error: reading header" << std::endl;
        return false;
    }

    w = header.width;
    h = header.height;
    bpp = header.bitsperpixel >> 3;

    if (w <= 0 || h <= 0 || (bpp != 1 && bpp != 3 && bpp != 4)) {
        std::cerr << "Error: invalid bpp or size" << std::endl;
        return false;
    }

    size_t nbytes = w * h * bpp;
    data.resize(nbytes);

    if (header.datatypecode == 2 || header.datatypecode == 3) {
        // Uncompressed
        in.read(reinterpret_cast<char*>(data.data()), nbytes);
        if (!in.good()) {
            std::cerr << "Error: reading image data" << std::endl;
            return false;
        }
    } else if (header.datatypecode == 10 || header.datatypecode == 11) {
        // RLE compressed
        size_t pixelcount = w * h;
        size_t currentpixel = 0;
        TGAColor color;
        color.bytespp = bpp;

        while (currentpixel < pixelcount) {
            uint8_t chunkheader = 0;
            in.read(reinterpret_cast<char*>(&chunkheader), sizeof(chunkheader));
            if (!in.good()) {
                std::cerr << "Error: reading RLE chunk" << std::endl;
                return false;
            }

            if (chunkheader < 128) {
                // Raw chunk
                chunkheader++;
                for (int i = 0; i < chunkheader; i++) {
                    in.read(reinterpret_cast<char*>(color.bgra), bpp);
                    if (!in.good()) {
                        std::cerr << "Error: reading raw pixel" << std::endl;
                        return false;
                    }
                    std::memcpy(&data[currentpixel * bpp], color.bgra, bpp);
                    currentpixel++;
                }
            } else {
                // RLE chunk
                chunkheader -= 127;
                in.read(reinterpret_cast<char*>(color.bgra), bpp);
                if (!in.good()) {
                    std::cerr << "Error: reading RLE pixel" << std::endl;
                    return false;
                }
                for (int i = 0; i < chunkheader; i++) {
                    std::memcpy(&data[currentpixel * bpp], color.bgra, bpp);
                    currentpixel++;
                }
            }
        }
    } else {
        std::cerr << "Error: unknown image format" << std::endl;
        return false;
    }

    if (!(header.imagedescriptor & 0x20)) {
        flip_vertically();
    }
    if (header.imagedescriptor & 0x10) {
        flip_horizontally();
    }

    return true;
}

bool TGAImage::write_tga_file(const std::string& filename, bool vflip, bool rle) const {
    static const uint8_t developer_area_ref[4] = {0, 0, 0, 0};
    static const uint8_t extension_area_ref[4] = {0, 0, 0, 0};
    static const uint8_t footer[18] = {'T','R','U','E','V','I','S','I','O','N','-','X','F','I','L','E','.','\0'};

    FILE* out = fopen(filename.c_str(), "wb");
    if (!out) {
        std::cerr << "Error: cannot open file " << filename << std::endl;
        return false;
    }

    TGAHeader header;
    header.bitsperpixel = bpp << 3;
    header.width = w;
    header.height = h;
    header.datatypecode = (bpp == 1 ? (rle ? 11 : 3) : (rle ? 10 : 2));
    header.imagedescriptor = vflip ? 0x00 : 0x20;

    if (fwrite(&header, 1, sizeof(header), out) != sizeof(header)) {
        std::cerr << "Error: writing header" << std::endl;
        fclose(out);
        return false;
    }

    if (!rle) {
        // Uncompressed
        size_t nbytes = w * h * bpp;
        if (fwrite(data.data(), 1, nbytes, out) != nbytes) {
            std::cerr << "Error: writing image data" << std::endl;
            fclose(out);
            return false;
        }
    } else {
        // RLE compression
        const int max_chunk_length = 128;
        size_t npixels = w * h;
        size_t curpix = 0;

        while (curpix < npixels) {
            size_t chunkstart = curpix * bpp;
            size_t curbyte = curpix * bpp;
            uint8_t run_length = 1;
            bool raw = true;

            while (curpix + run_length < npixels && run_length < max_chunk_length) {
                bool succ_eq = true;
                for (int t = 0; succ_eq && t < bpp; t++) {
                    succ_eq = (data[curbyte + t] == data[curbyte + t + bpp]);
                }
                curbyte += bpp;
                if (1 == run_length) {
                    raw = !succ_eq;
                }
                if (raw && succ_eq) {
                    run_length--;
                    break;
                }
                if (!raw && !succ_eq) {
                    break;
                }
                run_length++;
            }

            curpix += run_length;
            uint8_t chunk_header = raw ? (run_length - 1) : (run_length + 127);
            if (fwrite(&chunk_header, 1, 1, out) != 1) {
                std::cerr << "Error: writing RLE chunk header" << std::endl;
                fclose(out);
                return false;
            }

            size_t chunk_size = raw ? (run_length * bpp) : bpp;
            if (fwrite(&data[chunkstart], 1, chunk_size, out) != chunk_size) {
                std::cerr << "Error: writing RLE chunk data" << std::endl;
                fclose(out);
                return false;
            }
        }
    }

    fwrite(developer_area_ref, 1, sizeof(developer_area_ref), out);
    fwrite(extension_area_ref, 1, sizeof(extension_area_ref), out);
    fwrite(footer, 1, sizeof(footer), out);

    fclose(out);
    return true;
}
