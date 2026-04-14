#ifndef APP_BLUEPRINT_H
#define APP_BLUEPRINT_H

#include <SDL.h>
#include "../core/vec.h"
#include "../core/mat.h"
#include "../image/tgaimage.h"
#include "../rasterizer/zbuffer.h"
#include "../rasterizer/line.h"
#include "../transform/transform.h"
#include <string>
#include <chrono>
#include <cmath>
#include <vector>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// ============================================================
// Blueprint Technical Theme — switchable accent palettes
// ============================================================
// TGAColor(r, g, b) takes RGB in that order. Earlier comments in this
// file labeled the args "BGR" — that was wrong. Visual output kept,
// labels corrected, and a second palette added that cycles with [T].
// Call sites use Theme::ACCENT, Theme::BG_DARK, … as before. Under the
// hood these are references into a live palette that Theme::toggle()
// swaps in place — zero API changes for callers.
// ============================================================
namespace Theme {
    struct Palette {
        TGAColor bg_dark;     // window background
        TGAColor bg_panel;    // sidebar / topbar fill
        TGAColor grid;        // dividers + subtle lines
        TGAColor accent;      // primary highlight (titles, borders)
        TGAColor accent_alt;  // secondary tint (subtitles)
        TGAColor highlight;   // active / hover
        TGAColor warning;     // attention state
        TGAColor text;        // body text
        TGAColor text_dim;    // secondary text
        TGAColor success;     // good state
        TGAColor cube_hue;    // base tint for the cube faces
        const char* name;
    };

    // Mutable live palette — call sites reference these directly.
    inline Palette live = {
        TGAColor(15, 23, 42),    // bg_dark
        TGAColor(30, 41, 59),    // bg_panel
        TGAColor(51, 65, 85),    // grid
        TGAColor(34, 197, 94),   // accent
        TGAColor(22, 163, 74),   // accent_alt
        TGAColor(74, 222, 128),  // highlight
        TGAColor(251, 146, 60),  // warning
        TGAColor(248, 250, 252), // text
        TGAColor(148, 163, 184), // text_dim
        TGAColor(34, 197, 94),   // success
        TGAColor(34, 197, 94),   // cube_hue
        "PRO"
    };

    // Preset palettes
    inline const Palette kAmber = {
        TGAColor(10, 14, 18),    // deep ink
        TGAColor(20, 28, 38),    // panel
        TGAColor(38, 55, 70),    // grid
        TGAColor(255, 190, 40),  // warm gold
        TGAColor(190, 140, 60),  // bronze
        TGAColor(255, 230, 120), // bright gold
        TGAColor(255, 100, 40),  // orange
        TGAColor(225, 220, 205), // parchment
        TGAColor(120, 110, 95),  // dim warm grey
        TGAColor(110, 220, 130), // neon green
        TGAColor(255, 190, 40),  // cube = gold
        "AMBER"
    };

    inline const Palette kCyan = {
        TGAColor(6, 12, 18),
        TGAColor(14, 24, 34),
        TGAColor(40, 70, 90),
        TGAColor(80, 220, 230),  // bright cyan
        TGAColor(60, 150, 170),  // muted teal
        TGAColor(150, 255, 240), // ice
        TGAColor(255, 90, 130),  // magenta alert
        TGAColor(210, 230, 240), // cool white
        TGAColor(110, 135, 150), // cool grey
        TGAColor(120, 235, 140), // pistachio
        TGAColor(80, 220, 230),  // cube = cyan
        "CYAN"
    };

    inline const Palette kMagenta = {
        TGAColor(14, 8, 18),
        TGAColor(28, 18, 38),
        TGAColor(70, 40, 90),
        TGAColor(255, 80, 200),  // hot magenta
        TGAColor(180, 90, 150),  // dusk rose
        TGAColor(255, 180, 240), // pink glow
        TGAColor(255, 220, 60),  // lemon alert
        TGAColor(235, 220, 240),
        TGAColor(140, 110, 140),
        TGAColor(140, 255, 200), // mint
        TGAColor(255, 80, 200),  // cube = magenta
        "MAGENTA"
    };

    inline const Palette kPro = {
        TGAColor(15, 23, 42),    // bg_dark   #0F172A
        TGAColor(30, 41, 59),    // bg_panel  #1E293B
        TGAColor(51, 65, 85),    // grid      #334155
        TGAColor(34, 197, 94),   // accent    #22C55E
        TGAColor(22, 163, 74),   // accent_alt (darker green)
        TGAColor(74, 222, 128),  // highlight (lighter green)
        TGAColor(251, 146, 60),  // warning   orange
        TGAColor(248, 250, 252), // text      #F8FAFC
        TGAColor(148, 163, 184), // text_dim  slate-400
        TGAColor(34, 197, 94),   // success   = accent
        TGAColor(34, 197, 94),   // cube_hue  = accent
        "PRO"
    };

    inline void apply(const Palette& p) { live = p; }
    inline void cycle() {
        if      (live.name[0] == 'A') apply(kCyan);
        else if (live.name[0] == 'C') apply(kMagenta);
        else if (live.name[0] == 'M') apply(kPro);
        else                           apply(kAmber);
    }

    // Reference aliases — existing call sites keep working unchanged.
    inline const TGAColor& BG_DARK    = live.bg_dark;
    inline const TGAColor& BG_PANEL   = live.bg_panel;
    inline const TGAColor& GRID       = live.grid;
    inline const TGAColor& ACCENT     = live.accent;
    inline const TGAColor& ACCENT_ALT = live.accent_alt;
    inline const TGAColor& HIGHLIGHT  = live.highlight;
    inline const TGAColor& WARNING    = live.warning;
    inline const TGAColor& TEXT       = live.text;
    inline const TGAColor& TEXT_DIM   = live.text_dim;
    inline const TGAColor& SUCCESS    = live.success;
    inline const TGAColor& CUBE_HUE   = live.cube_hue;

    inline TGAColor grid_fade(double fade) {
        return TGAColor(
            static_cast<uint8_t>(GRID[2] * fade),  // [2]=R
            static_cast<uint8_t>(GRID[1] * fade),  // [1]=G
            static_cast<uint8_t>(GRID[0] * fade)   // [0]=B
        );
    }
}

// Forward declarations
namespace Font {
    void draw_char(TGAImage& img, int x, int y, char c, const TGAColor& color);
    void draw_string(TGAImage& img, int x, int y, const std::string& text,
                     const TGAColor& color, int scale = 1);
    void draw_centered(TGAImage& img, int x, int y, const std::string& text,
                      const TGAColor& color);
}

// ============================================================
// Modern PIXEL Ghost - Blueprint Edition
// ============================================================
namespace Mascot {
    enum State { IDLE, RENDERING, SUCCESS, ERROR, SLEEPING };

    inline int frame = 0;
    inline State current_state = IDLE;

    // Ghost body (16x16)
    const uint16_t GHOST_BODY[16] = {
        0b0000011111100000,
        0b0001111111111000,
        0b0011111111111100,
        0b0111111111111110,
        0b0111111111111110,
        0b0111111111111110,
        0b0111111111111110,
        0b0111111111111110,
        0b0111111111111110,
        0b0011111111111100,
        0b0001111111111000,
        0b0000111111110000,
        0b0000011111100000,
        0b0000010101010000,
        0b0000001010100000,
        0b0000000000000000
    };

    const uint8_t EYES_IDLE[2] =      {0b01100110, 0b01100110};
    const uint8_t EYES_RENDERING[2] = {0b01100110, 0b11111111};
    const uint8_t EYES_SUCCESS[2] =   {0b00000000, 0b01100110};
    const uint8_t EYES_ERROR[2] =     {0b10011001, 0b01100110};
    const uint8_t EYES_SLEEPING[2] =  {0b00000000, 0b01111110};

    const uint8_t MOUTH_IDLE[2] =      {0b00011000, 0b00000000};
    const uint8_t MOUTH_RENDERING[2] = {0b00111100, 0b00011000};
    const uint8_t MOUTH_SUCCESS[2] =   {0b00100100, 0b00011000};
    const uint8_t MOUTH_ERROR[2] =     {0b00011000, 0b00100100};
    const uint8_t MOUTH_SLEEPING[2] =  {0b00111100, 0b00000000};

    inline double float_offset() {
        // Smooth ease-in-out floating
        double t = (frame % 120) / 120.0;
        double eased = (std::cos(t * M_PI * 2) + 1) / 2.0;
        return (eased - 0.5) * 4.0;
    }

    // Helper: draw a filled block (for scaled pixel art)
    inline void fill_block(TGAImage& img, int x, int y, int s, const TGAColor& c) {
        for (int dy = 0; dy < s; dy++)
            for (int dx = 0; dx < s; dx++)
                img.set(x + dx, y + dy, c);
    }
    inline void add_block(TGAImage& img, int x, int y, int s, const TGAColor& c) {
        for (int dy = 0; dy < s; dy++)
            for (int dx = 0; dx < s; dx++) {
                TGAColor e = img.get(x + dx, y + dy);
                img.set(x + dx, y + dy, TGAColor(
                    (uint8_t)std::min(255, e[0] + c[0]),
                    (uint8_t)std::min(255, e[1] + c[1]),
                    (uint8_t)std::min(255, e[2] + c[2])));
            }
    }

    // Draw ghost with glow effect. scale = pixel size (1 = 16x16, 2 = 32x32, 3 = 48x48).
    void draw_with_glow(TGAImage& img, int x, int y, State state,
                        const TGAColor& color, int scale = 1) {
        current_state = state;
        frame++;
        int s = std::max(1, scale);
        int float_y = y + static_cast<int>(float_offset() * s);

        const uint8_t* eyes;
        const uint8_t* mouth;
        switch (state) {
            case RENDERING: eyes = EYES_RENDERING; mouth = MOUTH_RENDERING; break;
            case SUCCESS:   eyes = EYES_SUCCESS;   mouth = MOUTH_SUCCESS;   break;
            case ERROR:     eyes = EYES_ERROR;     mouth = MOUTH_ERROR;     break;
            case SLEEPING:  eyes = EYES_SLEEPING;  mouth = MOUTH_SLEEPING;  break;
            default:        eyes = EYES_IDLE;      mouth = MOUTH_IDLE;      break;
        }

        // Glow layers (reduce passes at higher scale for perf)
        int glow_max = (s > 1) ? 2 : 3;
        for (int glow_r = glow_max; glow_r >= 1; glow_r--) {
            double glow_intensity = 0.08 / glow_r;
            TGAColor gc = Theme::ACCENT * glow_intensity;
            for (int row = 0; row < 16; row++) {
                uint16_t bits = GHOST_BODY[row];
                for (int col = 0; col < 16; col++) {
                    if (!(bits & (0x8000 >> col))) continue;
                    int cx = x + col * s + s / 2;
                    int cy = float_y + row * s + s / 2;
                    int gr = glow_r * s;
                    for (int dy = -gr; dy <= gr; dy += s)
                        for (int dx = -gr; dx <= gr; dx += s)
                            if (dx*dx + dy*dy <= gr*gr)
                                add_block(img, cx + dx - s/2, cy + dy - s/2, s, gc);
                }
            }
        }

        // Main body
        for (int row = 0; row < 16; row++) {
            uint16_t bits = GHOST_BODY[row];
            for (int col = 0; col < 16; col++)
                if (bits & (0x8000 >> col))
                    fill_block(img, x + col * s, float_y + row * s, s, color);
        }

        // Eyes
        for (int row = 0; row < 2; row++) {
            for (int col = 0; col < 8; col++)
                fill_block(img, x + (4 + col) * s, float_y + (4 + row) * s, s, Theme::BG_DARK);
            uint8_t eb = eyes[row];
            for (int col = 0; col < 8; col++)
                if (eb & (0x80 >> col))
                    fill_block(img, x + (4 + col) * s, float_y + (4 + row) * s, s, Theme::ACCENT);
        }

        // Mouth
        for (int row = 0; row < 2; row++) {
            uint8_t mb = mouth[row];
            for (int col = 0; col < 8; col++)
                if (mb & (0x80 >> col))
                    fill_block(img, x + (4 + col) * s, float_y + (7 + row) * s, s, Theme::BG_DARK);
        }

        // Zzz for sleeping
        if (state == SLEEPING) {
            int zzz_phase = (frame / 20) % 3;
            const char* zzz[] = {"z", "zZ", "zZz"};
            Font::draw_string(img, x + 14 * s, float_y - 4 * s - zzz_phase * 3,
                             zzz[zzz_phase], Theme::ACCENT * 0.7, s);
        }
    }

    const char* get_message(State state) {
        switch (state) {
            case IDLE:      return "READY";
            case RENDERING: return "RENDERING...";
            case SUCCESS:   return "COMPLETE";
            case ERROR:     return "ERROR";
            case SLEEPING:  return "IDLE";
        }
        return "";
    }
}

// ============================================================
// Refined Bitmap Font (8x8, cleaner design)
// ============================================================
namespace Font {
    // Same character set but cleaner rendering
    const unsigned char CHARS[96][8] = {
        {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00}, // Space
        {0x18,0x18,0x18,0x18,0x18,0x00,0x18,0x00}, // !
        {0x6C,0x6C,0x24,0x00,0x00,0x00,0x00,0x00}, // "
        {0x6C,0xFE,0x6C,0x6C,0xFE,0x6C,0x00,0x00}, // #
        {0x18,0x7E,0xC0,0x7C,0x06,0xFC,0x18,0x00}, // $
        {0xC6,0xCC,0x18,0x30,0x66,0xC6,0x00,0x00}, // %
        {0x38,0x6C,0x38,0x76,0xDC,0xCC,0x76,0x00}, // &
        {0x18,0x18,0x30,0x00,0x00,0x00,0x00,0x00}, // '
        {0x0C,0x18,0x30,0x30,0x30,0x18,0x0C,0x00}, // (
        {0x30,0x18,0x0C,0x0C,0x0C,0x18,0x30,0x00}, // )
        {0x00,0x66,0x3C,0xFF,0x3C,0x66,0x00,0x00}, // *
        {0x00,0x18,0x18,0x7E,0x18,0x18,0x00,0x00}, // +
        {0x00,0x00,0x00,0x00,0x00,0x18,0x18,0x30}, // ,
        {0x00,0x00,0x00,0x7E,0x00,0x00,0x00,0x00}, // -
        {0x00,0x00,0x00,0x00,0x00,0x18,0x18,0x00}, // .
        {0x06,0x0C,0x18,0x30,0x60,0xC0,0x00,0x00}, // /
        {0x7C,0xC6,0xCE,0xD6,0xE6,0xC6,0x7C,0x00}, // 0
        {0x18,0x38,0x18,0x18,0x18,0x18,0x7E,0x00}, // 1
        {0x7C,0xC6,0x0C,0x18,0x30,0x60,0xFE,0x00}, // 2
        {0x7C,0xC6,0x06,0x3C,0x06,0xC6,0x7C,0x00}, // 3
        {0x0C,0x1C,0x3C,0x6C,0xFE,0x0C,0x0C,0x00}, // 4
        {0xFE,0xC0,0xFC,0x06,0x06,0xC6,0x7C,0x00}, // 5
        {0x3C,0x60,0xC0,0xFC,0xC6,0xC6,0x7C,0x00}, // 6
        {0xFE,0x06,0x0C,0x18,0x30,0x30,0x30,0x00}, // 7
        {0x7C,0xC6,0xC6,0x7C,0xC6,0xC6,0x7C,0x00}, // 8
        {0x7C,0xC6,0xC6,0x7E,0x06,0x0C,0x78,0x00}, // 9
        {0x00,0x18,0x18,0x00,0x18,0x18,0x00,0x00}, // :
        {0x00,0x18,0x18,0x00,0x18,0x18,0x30,0x00}, // ;
        {0x0C,0x18,0x30,0x60,0x30,0x18,0x0C,0x00}, // <
        {0x00,0x00,0x7E,0x00,0x7E,0x00,0x00,0x00}, // =
        {0x30,0x18,0x0C,0x06,0x0C,0x18,0x30,0x00}, // >
        {0x7C,0xC6,0x0C,0x18,0x18,0x00,0x18,0x00}, // ?
        {0x7C,0xC6,0xDE,0xDE,0xDE,0xC0,0x7C,0x00}, // @
        {0x38,0x6C,0xC6,0xFE,0xC6,0xC6,0xC6,0x00}, // A
        {0xFC,0xC6,0xC6,0xFC,0xC6,0xC6,0xFC,0x00}, // B
        {0x7C,0xC6,0xC0,0xC0,0xC0,0xC6,0x7C,0x00}, // C
        {0xF8,0xCC,0xC6,0xC6,0xC6,0xCC,0xF8,0x00}, // D
        {0xFE,0xC0,0xC0,0xFC,0xC0,0xC0,0xFE,0x00}, // E
        {0xFE,0xC0,0xC0,0xFC,0xC0,0xC0,0xC0,0x00}, // F
        {0x7C,0xC6,0xC0,0xCE,0xC6,0xC6,0x7E,0x00}, // G
        {0xC6,0xC6,0xC6,0xFE,0xC6,0xC6,0xC6,0x00}, // H
        {0x7E,0x18,0x18,0x18,0x18,0x18,0x7E,0x00}, // I
        {0x06,0x06,0x06,0x06,0xC6,0xC6,0x7C,0x00}, // J
        {0xC6,0xCC,0xD8,0xF0,0xD8,0xCC,0xC6,0x00}, // K
        {0xC0,0xC0,0xC0,0xC0,0xC0,0xC0,0xFE,0x00}, // L
        {0xC6,0xEE,0xFE,0xD6,0xC6,0xC6,0xC6,0x00}, // M
        {0xC6,0xE6,0xF6,0xDE,0xCE,0xC6,0xC6,0x00}, // N
        {0x7C,0xC6,0xC6,0xC6,0xC6,0xC6,0x7C,0x00}, // O
        {0xFC,0xC6,0xC6,0xFC,0xC0,0xC0,0xC0,0x00}, // P
        {0x7C,0xC6,0xC6,0xC6,0xD6,0xDE,0x7C,0x06}, // Q
        {0xFC,0xC6,0xC6,0xFC,0xD8,0xCC,0xC6,0x00}, // R
        {0x7C,0xC6,0xC0,0x7C,0x06,0xC6,0x7C,0x00}, // S
        {0xFF,0x18,0x18,0x18,0x18,0x18,0x18,0x00}, // T
        {0xC6,0xC6,0xC6,0xC6,0xC6,0xC6,0x7C,0x00}, // U
        {0xC6,0xC6,0xC6,0xC6,0x6C,0x38,0x10,0x00}, // V
        {0xC6,0xC6,0xC6,0xD6,0xFE,0xEE,0xC6,0x00}, // W
        {0xC6,0x6C,0x38,0x38,0x6C,0xC6,0xC6,0x00}, // X
        {0xC3,0x66,0x3C,0x18,0x18,0x18,0x18,0x00}, // Y
        {0xFE,0x0C,0x18,0x30,0x60,0xC0,0xFE,0x00}, // Z
        {0x3C,0x30,0x30,0x30,0x30,0x30,0x3C,0x00}, // [
        {0xC0,0x60,0x30,0x18,0x0C,0x06,0x00,0x00}, // backslash
        {0x3C,0x0C,0x0C,0x0C,0x0C,0x0C,0x3C,0x00}, // ]
        {0x10,0x38,0x6C,0xC6,0x00,0x00,0x00,0x00}, // ^
        {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0xFF}, // _
        {0x30,0x18,0x0C,0x00,0x00,0x00,0x00,0x00}, // `
        {0x00,0x00,0x7C,0x06,0x7E,0xC6,0x7E,0x00}, // a
        {0xC0,0xC0,0xFC,0xC6,0xC6,0xC6,0xFC,0x00}, // b
        {0x00,0x00,0x7C,0xC6,0xC0,0xC6,0x7C,0x00}, // c
        {0x06,0x06,0x7E,0xC6,0xC6,0xC6,0x7E,0x00}, // d
        {0x00,0x00,0x7C,0xC6,0xFE,0xC0,0x7C,0x00}, // e
        {0x1C,0x30,0x7C,0x30,0x30,0x30,0x30,0x00}, // f
        {0x00,0x00,0x7E,0xC6,0xC6,0x7E,0x06,0x7C}, // g
        {0xC0,0xC0,0xFC,0xC6,0xC6,0xC6,0xC6,0x00}, // h
        {0x18,0x00,0x38,0x18,0x18,0x18,0x3C,0x00}, // i
        {0x0C,0x00,0x1C,0x0C,0x0C,0x0C,0xCC,0x78}, // j
        {0xC0,0xC0,0xCC,0xD8,0xF0,0xD8,0xCC,0x00}, // k
        {0x38,0x18,0x18,0x18,0x18,0x18,0x3C,0x00}, // l
        {0x00,0x00,0xEC,0xFE,0xD6,0xD6,0xC6,0x00}, // m
        {0x00,0x00,0xFC,0xC6,0xC6,0xC6,0xC6,0x00}, // n
        {0x00,0x00,0x7C,0xC6,0xC6,0xC6,0x7C,0x00}, // o
        {0x00,0x00,0xFC,0xC6,0xC6,0xFC,0xC0,0xC0}, // p
        {0x00,0x00,0x7E,0xC6,0xC6,0x7E,0x06,0x06}, // q
        {0x00,0x00,0xDC,0xE0,0xC0,0xC0,0xC0,0x00}, // r
        {0x00,0x00,0x7E,0xC0,0x7C,0x06,0xFC,0x00}, // s
        {0x30,0x30,0x7C,0x30,0x30,0x30,0x1C,0x00}, // t
        {0x00,0x00,0xC6,0xC6,0xC6,0xC6,0x7E,0x00}, // u
        {0x00,0x00,0xC6,0xC6,0xC6,0x6C,0x38,0x00}, // v
        {0x00,0x00,0xC6,0xD6,0xD6,0xFE,0x6C,0x00}, // w
        {0x00,0x00,0xC6,0x6C,0x38,0x6C,0xC6,0x00}, // x
        {0x00,0x00,0xC6,0xC6,0xC6,0x7E,0x06,0x7C}, // y
        {0x00,0x00,0xFE,0x0C,0x38,0x60,0xFE,0x00}, // z
        {0x0E,0x18,0x18,0x70,0x18,0x18,0x0E,0x00}, // {
        {0x18,0x18,0x18,0x18,0x18,0x18,0x18,0x00}, // |
        {0x70,0x18,0x18,0x0E,0x18,0x18,0x70,0x00}, // }
        {0x72,0x9C,0x00,0x00,0x00,0x00,0x00,0x00}, // ~
        {0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF}  // DEL
    };

    void draw_char(TGAImage& img, int x, int y, char c, const TGAColor& color) {
        if (c < 32 || c > 127) c = '?';
        int idx = c - 32;
        for (int row = 0; row < 8; row++) {
            unsigned char bits = CHARS[idx][row];
            for (int col = 0; col < 8; col++) {
                if (bits & (0x80 >> col)) {
                    img.set(x + col, y + row, color);
                }
            }
        }
    }

    void draw_string(TGAImage& img, int x, int y, const std::string& text,
                     const TGAColor& color, int scale) {
        int cx = x;
        for (char c : text) {
            if (c == '\n') { cx = x; y += 10 * scale; continue; }
            if (scale == 1) {
                draw_char(img, cx, y, c, color);
            } else {
                int idx = (c < 32 || c > 127) ? 31 : c - 32;
                for (int row = 0; row < 8; row++) {
                    unsigned char bits = CHARS[idx][row];
                    for (int col = 0; col < 8; col++) {
                        if (bits & (0x80 >> col)) {
                            for (int sy = 0; sy < scale; sy++)
                                for (int sx = 0; sx < scale; sx++)
                                    img.set(cx + col*scale + sx, y + row*scale + sy, color);
                        }
                    }
                }
            }
            cx += 8 * scale;
        }
    }

    void draw_centered(TGAImage& img, int cx, int cy, const std::string& text,
                      const TGAColor& color) {
        int width = text.length() * 8;
        draw_string(img, cx - width/2, cy - 4, text, color);
    }

    int text_width(const std::string& text) {
        return text.length() * 8;
    }
}

// ============================================================
// Modern UI Components
// ============================================================
namespace UI {
    // Grid rendering with fade effect
    void draw_grid(TGAImage& img, int ox, int oy, int w, int h) {
        int cx = ox + w/2;
        int cy = oy + h/2;
        double max_dist = std::sqrt((w/2)*(w/2) + (h/2)*(h/2));

        // Minor grid lines (every 10px)
        for (int x = 0; x < w; x += 10) {
            for (int y = 0; y < h; y++) {
                double dx = (ox + x) - cx;
                double dy = (oy + y) - cy;
                double dist = std::sqrt(dx*dx + dy*dy);
                double fade = 1.0 - (dist / max_dist) * 0.6;
                if (fade > 0) {
                    img.set(ox + x, oy + y, Theme::grid_fade(0.2 * fade));
                }
            }
        }

        // Major grid lines (every 50px)
        for (int x = 0; x < w; x += 50) {
            for (int y = 0; y < h; y++) {
                double dx = (ox + x) - cx;
                double dy = (oy + y) - cy;
                double dist = std::sqrt(dx*dx + dy*dy);
                double fade = 1.0 - (dist / max_dist) * 0.6;
                if (fade > 0) {
                    img.set(ox + x, oy + y, Theme::grid_fade(0.5 * fade));
                }
            }
        }

        for (int y = 0; y < h; y += 50) {
            for (int x = 0; x < w; x++) {
                double dx = (ox + x) - cx;
                double dy = (oy + y) - cy;
                double dist = std::sqrt(dx*dx + dy*dy);
                double fade = 1.0 - (dist / max_dist) * 0.6;
                if (fade > 0) {
                    img.set(ox + x, oy + y, Theme::grid_fade(0.5 * fade));
                }
            }
        }
    }

    // Draw panel with gradient and optional rounded corners
    void draw_panel(TGAImage& img, int x, int y, int w, int h,
                    const TGAColor& bg, const TGAColor& border, int radius = 0) {
        // Fill with subtle gradient (top to bottom)
        for (int py = 0; py < h; py++) {
            double gradient = 1.0 - (double)py / h * 0.12;
            TGAColor row_color(
                static_cast<uint8_t>(bg[0] * gradient),
                static_cast<uint8_t>(bg[1] * gradient),
                static_cast<uint8_t>(bg[2] * gradient)
            );

            for (int px = 0; px < w; px++) {
                // Simple corner rounding
                if (radius > 0) {
                    bool in_corner = false;
                    if ((px < radius && py < radius && (px*px + py*py < radius*radius)) ||
                        (px >= w-radius && py < radius && ((w-1-px)*(w-1-px) + py*py < radius*radius)) ||
                        (px < radius && py >= h-radius && (px*px + (h-1-py)*(h-1-py) < radius*radius)) ||
                        (px >= w-radius && py >= h-radius && ((w-1-px)*(w-1-px) + (h-1-py)*(h-1-py) < radius*radius))) {
                        continue; // Skip rounded corners
                    }
                }
                img.set(x + px, y + py, row_color);
            }
        }

        // Draw border
        for (int px = 0; px < w; px++) {
            img.set(x + px, y, border);
            img.set(x + px, y + h - 1, border);
        }
        for (int py = 0; py < h; py++) {
            img.set(x, y + py, border);
            img.set(x + w - 1, y + py, border);
        }
    }

    // Button component
    struct Button {
        int x, y, w, h;
        std::string label;
        bool hovered = false;
        bool active = false;

        void draw(TGAImage& img, double pulse_phase = 0.0) {
            // Active buttons get a subtle pulsing accent border so the
            // user's eye keeps finding the current render mode.
            double pulse = active
                ? 0.75 + 0.25 * std::sin(pulse_phase * 2.0 * M_PI)
                : 1.0;

            TGAColor bg = active ? Theme::ACCENT * pulse :
                         hovered ? Theme::BG_PANEL * 1.15 : Theme::BG_PANEL;
            TGAColor text = active ? Theme::BG_DARK : Theme::TEXT;
            TGAColor border = active ? Theme::HIGHLIGHT : Theme::GRID;

            draw_panel(img, x, y, w, h, bg, border, 3);

            // Extra glow band under active buttons.
            if (active) {
                for (int px = 2; px < w - 2; px++) {
                    img.set(x + px, y + h - 2, Theme::HIGHLIGHT * pulse);
                    img.set(x + px, y + h - 1, Theme::HIGHLIGHT * (pulse * 0.5));
                }
            }

            Font::draw_centered(img, x + w/2, y + h/2, label, text);
        }

        bool contains(int mx, int my) const {
            return mx >= x && mx < x + w && my >= y && my < y + h;
        }
    };
}

// ============================================================
// Camera Controller
// ============================================================
class CameraController {
public:
    Vec3 position{0, 0, 3}, target{0, 0, 0}, up{0, 1, 0};
    double yaw = -90.0, pitch = 0.0, distance = 3.0;
    double move_speed = 0.1, look_speed = 0.3;
    bool orbit_mode = true;

    void update() {
        double rad_yaw = yaw * M_PI / 180.0;
        double rad_pitch = pitch * M_PI / 180.0;
        if (orbit_mode) {
            position.x = target.x + distance * cos(rad_pitch) * cos(rad_yaw);
            position.y = target.y + distance * sin(rad_pitch);
            position.z = target.z + distance * cos(rad_pitch) * sin(rad_yaw);
        } else {
            Vec3 front{cos(rad_pitch)*cos(rad_yaw), sin(rad_pitch), cos(rad_pitch)*sin(rad_yaw)};
            target = position + front;
        }
    }

    void process_keyboard(const uint8_t* keys) {
        Vec3 front = normalized(target - position);
        Vec3 right = normalized(cross(front, up));
        if (keys[SDL_SCANCODE_W]) { if (orbit_mode) distance -= move_speed; else position = position + front * move_speed; }
        if (keys[SDL_SCANCODE_S]) { if (orbit_mode) distance += move_speed; else position = position - front * move_speed; }
        if (keys[SDL_SCANCODE_A]) { if (orbit_mode) yaw -= look_speed * 3; else position = position - right * move_speed; }
        if (keys[SDL_SCANCODE_D]) { if (orbit_mode) yaw += look_speed * 3; else position = position + right * move_speed; }
        if (keys[SDL_SCANCODE_Q]) position = position + up * move_speed;
        if (keys[SDL_SCANCODE_E]) position = position - up * move_speed;
        distance = std::max(0.5, std::min(20.0, distance));
        update();
    }

    void process_mouse(int dx, int dy) {
        yaw += dx * look_speed;
        pitch = std::max(-89.0, std::min(89.0, pitch - dy * look_speed));
        update();
    }

    Mat4 get_view_matrix() const { return lookat(position, target, up); }
};

// ============================================================
// Application State
// ============================================================
struct AppState {
    // Window layout — wider canvas so the new inspector panel has room.
    int width = 1280, height = 800;
    int viewport_x = 270, viewport_y = 62, viewport_w = 800, viewport_h = 696;
    CameraController camera;
    double fps = 0, frame_time = 0, idle_timer = 0;

    // Rolling frame-time history (seconds) for the inspector histogram.
    static constexpr int FT_HIST = 90;
    double frame_hist[FT_HIST] = {0};
    int    frame_hist_i = 0;
    void push_frame_time(double ft) {
        frame_hist[frame_hist_i] = ft;
        frame_hist_i = (frame_hist_i + 1) % FT_HIST;
    }

    enum RenderMode { WIREFRAME, FLAT, GOURAUD, TEXTURED, RAYTRACE };
    RenderMode render_mode = FLAT;
    bool rt_dirty = true;          // true = need to re-render raytrace
    int  rt_samples = 0;           // progressive sample count

    // Edit state — Blender-ish two-level mode.
    enum EditMode { OBJECT_MODE, EDIT_MODE };
    enum SelectMode { SEL_VERT, SEL_FACE };
    EditMode   edit_mode   = OBJECT_MODE;
    SelectMode select_mode = SEL_VERT;
    int selected_vertex = -1;
    int selected_face   = -1;
    // Pulse phase for the active-mode button / selection glow.
    double ui_phase = 0.0;

    Vec3 light_dir = normalized(Vec3(1, 1, 1));
    bool light_rotate = false;
    double light_angle = 0;

    // UI Buttons
    std::vector<UI::Button> mode_buttons;
    int mouse_x = 0, mouse_y = 0;

    AppState() {
        // Render mode pill bar — centered in the top bar.
        const int bx = 470, by = 14, bw = 84, bh = 26, gap = 6;
        mode_buttons.push_back({bx + 0*(bw+gap), by, bw, bh, "WIREFRAME", false, false});
        mode_buttons.push_back({bx + 1*(bw+gap), by, bw, bh, "FLAT",      false, true});
        mode_buttons.push_back({bx + 2*(bw+gap), by, bw, bh, "GOURAUD",   false, false});
        mode_buttons.push_back({bx + 3*(bw+gap), by, bw, bh, "TEXTURED",  false, false});
        mode_buttons.push_back({bx + 4*(bw+gap), by, bw, bh, "RAYTRACE",  false, false});
    }

    void update_buttons() {
        for (size_t i = 0; i < mode_buttons.size(); i++) {
            mode_buttons[i].active = (i == (size_t)render_mode);
            mode_buttons[i].hovered = mode_buttons[i].contains(mouse_x, mouse_y);
        }
    }

    Mascot::State get_ghost_state() const {
        if (idle_timer > 10.0) return Mascot::SLEEPING;
        return Mascot::IDLE;
    }

    const char* mode_name() const {
        switch (render_mode) {
            case WIREFRAME: return "WIREFRAME";
            case FLAT: return "FLAT";
            case GOURAUD: return "GOURAUD";
            case TEXTURED: return "TEXTURED";
            case RAYTRACE: return "RAYTRACE";
        }
        return "?";
    }
};

#endif
