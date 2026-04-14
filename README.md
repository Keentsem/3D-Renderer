# 3D Renderer - From Scratch

A complete 3D renderer implementation in C++, supporting both rasterization and ray tracing.

## Project Status

**Phase 1: Project Foundations** ✓ Complete

- Directory structure created
- Vector math library (Vec2, Vec3, Vec4)
- Matrix math library (Mat3, Mat4)
- TGA image I/O with RLE compression
- Build system (CMake & Makefile)

## Requirements

### C++ Compiler ✓ INSTALLED
MinGW-w64 (GCC 14.2.0) is installed at `C:\mingw64\mingw64\bin\`

The build scripts automatically add this to your PATH.

## Building

### Easiest Method
```bash
./build.sh      # Build the project
./renderer.exe  # Run the renderer
```

Or use the batch files in Windows Command Prompt:
```bash
build.bat       # Build the project
run.bat         # Run the renderer
```

### Manual Build (if needed)
```bash
# Make sure MinGW is in your PATH first:
set PATH=C:\mingw64\mingw64\bin;%PATH%

# Then compile:
g++ -std=c++17 -O3 -Wall -Isrc -c src/main.cpp -o src/main.o
g++ -std=c++17 -O3 -Wall -Isrc -c src/image/tgaimage.cpp -o src/image/tgaimage.o
g++ -std=c++17 -O3 -o renderer.exe src/main.o src/image/tgaimage.o
```

### Using Make (if available)
```bash
make            # Build the project
make run        # Build and run
make clean      # Clean build artifacts
```

## Testing Phase 1

After building, run:
```bash
./renderer  # or renderer.exe on Windows
```

This will:
1. Test vector operations (dot product, cross product, normalization)
2. Test matrix operations (multiplication, identity)
3. Generate `output/test.tga` with a red cross and colored corners

Expected output:
```
=== Phase 1: Testing Project Foundations ===

--- Testing Vectors ---
Dot product a * b: 32
Cross product a x b: (-3, 6, -3)
Normalized a: (0.267261, 0.534522, 0.801784)
Length of normalized a: 1.0

--- Testing Matrices ---
Identity * v: (1, 2, 3, 1)
Scaled v: (2, 6, 12, 1)

--- Testing TGA Image ---
✓ Image saved successfully to output/test.tga!
  Image size: 100x100

=== Phase 1 Complete ===
All foundation components are working correctly.
Ready to proceed to Phase 2: Line Drawing.
```

## Project Structure

```
3d_renderer/
├── src/
│   ├── core/           # vec.h, mat.h (math foundations)
│   ├── image/          # tgaimage.h, tgaimage.cpp (image I/O)
│   ├── rasterizer/     # line.h, triangle.h, zbuffer.h, shader.h (Phase 2+)
│   ├── raytracer/      # raytracer.h (Phase 4+)
│   ├── scene/          # model.h (Phase 2+)
│   ├── transform/      # transform.h (Phase 3+)
│   └── main.cpp
├── obj/                # 3D model files (.obj)
├── textures/           # Texture images (.tga)
├── output/             # Rendered output images
├── CMakeLists.txt
├── Makefile
└── README.md
```

## Key Features Implemented (Phase 1)

### Vector Mathematics (`src/core/vec.h`)
- Generic n-dimensional vectors
- Specialized Vec2, Vec3, Vec4
- Dot product, cross product
- Scalar operations (multiply, divide)
- Vector operations (add, subtract)
- Normalization
- Homogeneous coordinate conversions

### Matrix Mathematics (`src/core/mat.h`)
- Generic m×n matrices
- Matrix-vector multiplication
- Matrix-matrix multiplication
- Transpose
- 4×4 inverse (for normal transformations)
- Identity matrix generation

### TGA Image I/O (`src/image/tgaimage.h`, `tgaimage.cpp`)
- Read/write TGA files
- RLE compression support
- Horizontal/vertical flipping
- GRAYSCALE, RGB, RGBA formats
- Predefined colors (WHITE, BLACK, RED, GREEN, BLUE)

## Next Steps

Proceed to **Phase 2: Line Drawing** to implement:
- Bresenham's line algorithm
- Wireframe rendering
- OBJ file loading
- Your first 3D wireframe model

## License

Educational project - feel free to use and modify.
