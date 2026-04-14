# SDL2 Setup Guide for Phase 10

## Quick Start for Windows (MSYS2/MinGW)

### Option 1: MSYS2 Package Manager (Recommended)

If you're using MSYS2, install SDL2 with:

```bash
# For MinGW 64-bit
pacman -S mingw-w64-x86_64-SDL2

# For MinGW 32-bit
pacman -S mingw-w64-i686-SDL2
```

Then build with:
```bash
bash build_app.sh
```

### Option 2: Manual Download

1. **Download SDL2 Development Libraries:**
   - Visit: https://github.com/libsdl-org/SDL/releases/latest
   - Download: `SDL2-devel-2.x.x-mingw.tar.gz`

2. **Extract to Project:**
   ```bash
   mkdir -p external
   cd external
   # Extract the downloaded SDL2-devel-x.x.x-mingw.tar.gz here
   # You should have: external/SDL2-x.x.x/
   ```

3. **Build:**
   ```bash
   bash build_app.sh
   ```

### Option 3: System-Wide SDL2 (Linux/Mac)

**Ubuntu/Debian:**
```bash
sudo apt install libsdl2-dev
bash build_app.sh
```

**Fedora:**
```bash
sudo dnf install SDL2-devel
bash build_app.sh
```

**macOS (Homebrew):**
```bash
brew install sdl2
bash build_app.sh
```

## Testing the Application

Once built, run:
```bash
./pixel_app.exe  # Windows
./pixel_app      # Linux/Mac
```

## Controls

- **WASD** - Move camera
- **Mouse** - Look around
- **1-2** - Switch render modes (Wireframe, Flat)
- **F1** - Toggle CRT effects
- **F2** - Toggle theme (Green/Amber)
- **F3** - Toggle orbit/free camera mode
- **L** - Toggle rotating light
- **R** - Reset camera
- **ESC** - Quit

## Troubleshooting

### "SDL2 not found" Error

Make sure SDL2 is either:
1. Installed system-wide (Option 1 or 3)
2. Or extracted to `external/` directory (Option 2)

### Missing DLL on Windows

If you get "SDL2.dll not found" when running:
1. Copy `SDL2.dll` from SDL2 bin directory to project root
2. Or add SDL2 bin directory to PATH

### Compilation Errors

Check that you have:
- C++11 compatible compiler (g++ 4.9+)
- SDL2 headers accessible
- All Phase 1-9 code compiled successfully
