# PIXEL the Wireframe Ghost - Visual Reference

## ASCII Art Preview

```
    ░░░░░░░░░░░░
  ░░████████████░░
 ░██░░░░░░░░░░░░██░
░██░░██░░░░░░██░░██░     "PIXEL"
░██░░██░░░░░░██░░██░      The Wireframe Ghost
░██░░░░░░░░░░░░░░██░
░██░░░░████████░░██░      > READY TO RENDER_
 ░██░░░░░░░░░░░░██░
  ░░██░░░░░░░░██░░
    ░░██░░░░██░░
      ░░████░░
        ░░░░
```

## Emotional States

### IDLE (Default State)
```
   ◠    ◠     Happy eyes
      ‿        Small smile

  "READY TO RENDER"
```

### RENDERING (During active rendering)
```
   ◉    ◉     Wide eyes
      ○        Open mouth

  "PROCESSING..."
```

### SUCCESS (Render completed)
```
   ◡    ◡     Happy closed eyes
      ◡        Big smile

  "RENDER COMPLETE!"
```

### ERROR (Something went wrong)
```
   ✖    ✖     X eyes
      △        Triangle frown
    ░░░        Glitch trail effect

  "SOMETHING WENT WRONG"
```

### SLEEPING (Idle >10 seconds)
```
   —    —     Sleeping eyes
      ○        Small mouth
    zZz        Sleep bubbles float up

  "IDLE MODE"
```

## Size Variations

### Large (32x32) - Boot Sequence
- Used in initialization screen
- 2x scaled version of 16x16 sprite
- Appears with title "PIXEL" and subtitle "THE WIREFRAME GHOST"

### Normal (16x16) - UI Panel Header
- Animated floating (sine wave up/down)
- State changes based on app activity
- Located top-left of UI panel

### Mini (8x8) - Status Bar
- Static icon
- Simplified silhouette
- Left side of bottom status bar

## Animation Details

### Floating Motion
```
Frame 0:   y = base + 2.0  (top of float)
Frame 15:  y = base + 0.0  (middle)
Frame 31:  y = base - 2.0  (bottom)
Frame 47:  y = base + 0.0  (middle)
```
Motion: Smooth sine wave, period ≈ 6 seconds

### Glitch Effect (ERROR state only)
```
Row 6-7: Shifts left/right by 1 pixel
Pattern: Changes every 5 frames
Trail: 3 pixels with decreasing opacity
```

### Sleep Animation (SLEEPING state only)
```
Phase 0: "z"      at (14, -4)
Phase 1: "zZ"     at (14, -7)
Phase 2: "zZz"    at (14, -10)
```
Cycles every 20 frames

## Color Schemes

### Green Phosphor (Default)
```
Primary:  RGB(51, 255, 51)   - #33FF33 - Main ghost color
Dim:      RGB(26, 128, 26)   - #1A801A - Dimmed text
Bright:   RGB(102, 255, 102) - #66FF66 - Highlights
```

### Amber (F2 to toggle)
```
Primary:  RGB(255, 176, 0)   - #FFB000 - Main ghost color
Dim:      RGB(128, 88, 0)    - #805800 - Dimmed text
Bright:   RGB(255, 204, 51)  - #FFCC33 - Highlights
```

Note: TGAColor uses BGR byte order internally!

## Sprite Data Structure

### Body (16x16, 1-bit per pixel)
```cpp
const uint16_t GHOST_BODY[16] = {
    0b0000011111100000,  // ░░░░████████░░░░
    0b0001111111111000,  // ░░██████████████░░
    // ... 16 rows total
};
```

### Eyes (8x8, placed at row 4-5, col 4-11)
```cpp
const uint8_t EYES_IDLE[2] = {
    0b01100110,  // ░██░░██░
    0b01100110,  // ░██░░██░
};
```

### Mouth (8x8, placed at row 7-8, col 4-11)
```cpp
const uint8_t MOUTH_IDLE[2] = {
    0b00011000,  // ░░░██░░░
    0b00000000,  // ░░░░░░░░
};
```

## Implementation Notes

### Drawing Order
1. Draw body from sprite data
2. Cut out eye sockets (fill with black)
3. Draw eyes based on state
4. Draw mouth based on state (uses black pixels)
5. Add special effects (glitch trail, sleep bubbles)

### State Triggers
```
User Input → IDLE (immediately)
No Input for 10s → SLEEPING
Active Rendering → RENDERING (if implemented)
Error Detected → ERROR (if implemented)
Render Done → SUCCESS (briefly, then IDLE)
```

### Collision with UI
- Ghost is drawn after background fill
- No collision detection needed
- Floats in safe area of UI panel
- Never overlaps with text or boxes

## Easter Eggs

1. **Sleep Detection**: Leave app idle for 10+ seconds
2. **Wake Up**: Any input wakes the ghost instantly
3. **Theme Switch**: Ghost changes color with theme (F2)
4. **Persistence**: Ghost state persists across render mode changes

## Fun Facts

- PIXEL has 5 distinct emotional states
- Composed of ~400 pixels in normal size
- Floats at 0.1 Hz frequency
- Name inspired by being made of pixels AND haunting your renderer
- First appearance: Phase 10 boot sequence
- Favorite activity: Watching your beautiful renders!

---

**Pro Tip**: Watch PIXEL's face carefully - it reflects the app's state!

```
          ░░░░░░░
        ░░██████░░
       ░██░◠░░◠░██░
       ░██░░‿░░░██░
        ░██████░██░
         ░░░░░░░░

      "Happy Rendering!"
```
