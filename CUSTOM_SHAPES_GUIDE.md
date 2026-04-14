# How to Create Custom Shapes in the 3D Renderer

## Overview

This guide shows you how to add your own 3D shapes to the renderer instead of the default cube. You can create pyramids, octahedrons, or any custom polyhedron!

## Understanding the Shape Definition

### 1. Vertices (Points in 3D Space)

Vertices are defined as `Vec3` with (X, Y, Z) coordinates:

```cpp
Vec3 cube[8] = {
    {-0.5, -0.5, -0.5},  // Vertex 0: back-bottom-left
    { 0.5, -0.5, -0.5},  // Vertex 1: back-bottom-right
    { 0.5,  0.5, -0.5},  // Vertex 2: back-top-right
    {-0.5,  0.5, -0.5},  // Vertex 3: back-top-left
    {-0.5, -0.5,  0.5},  // Vertex 4: front-bottom-left
    { 0.5, -0.5,  0.5},  // Vertex 5: front-bottom-right
    { 0.5,  0.5,  0.5},  // Vertex 6: front-top-right
    {-0.5,  0.5,  0.5}   // Vertex 7: front-top-left
};
```

**Coordinate System**:
- **X**: Left (-) to Right (+)
- **Y**: Down (-) to Up (+)
- **Z**: Back (-) to Front (+)

### 2. Faces (Triangles)

Faces connect vertices into triangles. Each face is defined by 3 vertex indices:

```cpp
int faces[12][3] = {
    {0, 1, 2},  // Back face triangle 1
    {0, 2, 3},  // Back face triangle 2
    {4, 6, 5},  // Front face triangle 1
    {4, 7, 6},  // Front face triangle 2
    // ... more faces
};
```

**Important**: Vertices must be in **counter-clockwise order** when viewed from outside!

## Where to Add Your Shape

Edit `src/app/main_app_blueprint.cpp`, find the `render_scene_blueprint` function around line 165:

```cpp
void render_scene_blueprint(TGAImage& vp, ZBuffer& zb, AppState& state) {
    // ... grid drawing code ...

    // THIS IS WHERE YOU DEFINE YOUR SHAPE:
    Vec3 cube[8] = { /* ... */ };
    int faces[12][3] = { /* ... */ };

    // ... rendering code ...
}
```

## Example Custom Shapes

### Example 1: Pyramid (Tetrahedron)

A simple 4-sided pyramid with a triangular base:

```cpp
// Replace the cube definition with this:

// 4 vertices
Vec3 pyramid[4] = {
    { 0.0,  0.7,  0.0},  // 0: Top apex
    {-0.5, -0.5, -0.5},  // 1: Base corner 1
    { 0.5, -0.5, -0.5},  // 2: Base corner 2
    { 0.0, -0.5,  0.5}   // 3: Base corner 3
};

// 4 faces (triangles)
int faces[4][3] = {
    {0, 2, 1},  // Right face
    {0, 3, 2},  // Back-right face
    {0, 1, 3},  // Left face
    {1, 2, 3}   // Base
};

// Update loop to use 4 faces instead of 12:
for (int f = 0; f < 4; f++) {  // Changed from 12 to 4
    Vec3 scr[3], wld[3];
    for (int v = 0; v < 3; v++) {
        wld[v] = pyramid[faces[f][v]];  // Changed from cube to pyramid
        // ... rest of code
    }
}
```

### Example 2: Octahedron (8-sided)

A diamond shape with 6 vertices and 8 triangular faces:

```cpp
// 6 vertices
Vec3 octahedron[6] = {
    { 0.0,  0.7,  0.0},  // 0: Top
    { 0.0, -0.7,  0.0},  // 1: Bottom
    { 0.7,  0.0,  0.0},  // 2: Right
    {-0.7,  0.0,  0.0},  // 3: Left
    { 0.0,  0.0,  0.7},  // 4: Front
    { 0.0,  0.0, -0.7}   // 5: Back
};

// 8 faces
int faces[8][3] = {
    {0, 2, 4},  // Top-right-front
    {0, 4, 3},  // Top-front-left
    {0, 3, 5},  // Top-left-back
    {0, 5, 2},  // Top-back-right
    {1, 4, 2},  // Bottom-front-right
    {1, 3, 4},  // Bottom-left-front
    {1, 5, 3},  // Bottom-back-left
    {1, 2, 5}   // Bottom-right-back
};

// Update loop:
for (int f = 0; f < 8; f++) {  // Changed to 8
    Vec3 scr[3], wld[3];
    for (int v = 0; v < 3; v++) {
        wld[v] = octahedron[faces[f][v]];  // Changed to octahedron
        // ... rest of code
    }
}
```

### Example 3: Flat Rectangle/Plane

A simple flat square (2 triangles):

```cpp
// 4 vertices
Vec3 plane[4] = {
    {-0.7,  0.0, -0.7},  // 0: Back-left
    { 0.7,  0.0, -0.7},  // 1: Back-right
    { 0.7,  0.0,  0.7},  // 2: Front-right
    {-0.7,  0.0,  0.7}   // 3: Front-left
};

// 2 faces
int faces[2][3] = {
    {0, 1, 2},  // First triangle
    {0, 2, 3}   // Second triangle
};

// Update loop:
for (int f = 0; f < 2; f++) {
    Vec3 scr[3], wld[3];
    for (int v = 0; v < 3; v++) {
        wld[v] = plane[faces[f][v]];
        // ... rest of code
    }
}
```

### Example 4: Prism (Triangular)

A triangular prism with 6 vertices and 8 faces:

```cpp
// 6 vertices (2 triangular ends)
Vec3 prism[6] = {
    // Front triangle
    { 0.0,  0.5,  0.5},  // 0: Top
    {-0.5, -0.5,  0.5},  // 1: Bottom-left
    { 0.5, -0.5,  0.5},  // 2: Bottom-right
    // Back triangle
    { 0.0,  0.5, -0.5},  // 3: Top
    {-0.5, -0.5, -0.5},  // 4: Bottom-left
    { 0.5, -0.5, -0.5}   // 5: Bottom-right
};

// 8 faces
int faces[8][3] = {
    {0, 1, 2},  // Front triangle
    {3, 5, 4},  // Back triangle
    {0, 3, 1},  // Top-left side
    {1, 3, 4},  // Top-left side 2
    {0, 2, 3},  // Top-right side
    {2, 5, 3},  // Top-right side 2
    {1, 4, 2},  // Bottom side
    {2, 4, 5}   // Bottom side 2
};

// Update loop:
for (int f = 0; f < 8; f++) {
    Vec3 scr[3], wld[3];
    for (int v = 0; v < 3; v++) {
        wld[v] = prism[faces[f][v]];
        // ... rest of code
    }
}
```

## Step-by-Step: Adding a Pyramid

Let's walk through adding a pyramid step-by-step:

### Step 1: Find the Shape Definition

Open `src/app/main_app_blueprint.cpp` and search for this code (around line 165):

```cpp
// Cube vertices
Vec3 cube[8] = {{-0.5,-0.5,-0.5},{0.5,-0.5,-0.5}, ...};
int faces[12][3] = {{0,1,2},{0,2,3}, ...};
```

### Step 2: Replace with Pyramid

Change it to:

```cpp
// Pyramid vertices
Vec3 pyramid[4] = {
    { 0.0,  0.7,  0.0},  // Top apex
    {-0.5, -0.5, -0.5},  // Base corner 1
    { 0.5, -0.5, -0.5},  // Base corner 2
    { 0.0, -0.5,  0.5}   // Base corner 3
};

// Pyramid faces
int faces[4][3] = {
    {0, 2, 1},  // Right face
    {0, 3, 2},  // Back face
    {0, 1, 3},  // Left face
    {1, 2, 3}   // Base
};
```

### Step 3: Update the Rendering Loop

Find this line:
```cpp
for (int f = 0; f < 12; f++) {
```

Change to:
```cpp
for (int f = 0; f < 4; f++) {  // 4 faces in pyramid
```

### Step 4: Update Vertex References

Find this line:
```cpp
wld[v] = cube[faces[f][v]];
```

Change to:
```cpp
wld[v] = pyramid[faces[f][v]];
```

### Step 5: Rebuild and Run

```bash
bash build_blueprint.sh
./renderer_blueprint.exe
```

You should now see a pyramid instead of a cube!

## Face Orientation (Winding Order)

**Critical Rule**: Triangles must have vertices in **counter-clockwise order** when viewed from outside.

```
    Looking at a triangle from outside:

         v0
        /  \
       /    \
      v1----v2

    Correct order: {v0, v1, v2} (counter-clockwise)
    Wrong order: {v0, v2, v1} (clockwise - will be culled!)
```

If a face disappears, try reversing the vertex order:
- Change `{0, 1, 2}` to `{0, 2, 1}`

## Troubleshooting

### Problem: Shape doesn't appear
**Solution**: Check vertex coordinates - they might be outside the view:
```cpp
// Make sure coordinates are between -1.0 and 1.0
Vec3 vertex = {0.0, 0.0, 0.0};  // This is at the origin
```

### Problem: Some faces are missing
**Solution**: Wrong winding order. Reverse the vertex indices:
```cpp
// If this doesn't show:
{0, 1, 2}
// Try this:
{0, 2, 1}
```

### Problem: Shape is too small/large
**Solution**: Scale the coordinates:
```cpp
// Too small? Multiply coordinates:
Vec3 bigger[4] = {
    { 0.0,  1.4,  0.0},  // 2x larger
    {-1.0, -1.0, -1.0},
    // ...
};

// Too large? Divide coordinates:
Vec3 smaller[4] = {
    { 0.0,  0.35,  0.0},  // 2x smaller
    {-0.25, -0.25, -0.25},
    // ...
};
```

### Problem: Shape is off-center
**Solution**: Adjust all coordinates equally:
```cpp
// Move up by 0.5:
Vec3 moved[4] = {
    { 0.0,  1.2,  0.0},  // Added 0.5 to Y
    {-0.5,  0.0, -0.5},  // Added 0.5 to Y
    // ...
};
```

## Creating Complex Shapes

### Tips for Complex Shapes:

1. **Start Simple**: Begin with basic shapes and add detail
2. **Draw on Paper**: Sketch your shape and number the vertices
3. **Build Incrementally**: Add one face at a time, test after each
4. **Use Symmetry**: Mirror vertices across axes for symmetric shapes

### Example: Star Shape (Work in Progress)

```cpp
// 10 vertices: 5 outer points, 5 inner points
Vec3 star[10] = {
    // Outer points (radius 0.7)
    { 0.0,   0.7,  0.0},   // 0: Top
    { 0.665, 0.217, 0.0},  // 1: Right-top
    { 0.411,-0.567, 0.0},  // 2: Right-bottom
    {-0.411,-0.567, 0.0},  // 3: Left-bottom
    {-0.665, 0.217, 0.0},  // 4: Left-top
    // Inner points (radius 0.3)
    { 0.0,   0.3,  0.0},   // 5: Center-top
    { 0.286, 0.093, 0.0},  // 6: Center-right-top
    { 0.176,-0.243, 0.0},  // 7: Center-right-bottom
    {-0.176,-0.243, 0.0},  // 8: Center-left-bottom
    {-0.286, 0.093, 0.0}   // 9: Center-left-top
};

// Connect points to create triangles
int faces[10][3] = {
    {5, 0, 6},  {6, 0, 1},
    {6, 1, 7},  {7, 1, 2},
    {7, 2, 8},  {8, 2, 3},
    {8, 3, 9},  {9, 3, 4},
    {9, 4, 5},  {5, 4, 0}
};
```

## Loading Custom Models

For complex shapes, consider loading OBJ files:

```cpp
// In render_scene_blueprint function:
Model myModel("models/my_shape.obj");

if (myModel.nfaces() > 0) {
    for (int f = 0; f < myModel.nfaces(); f++) {
        std::vector<int> face = myModel.face(f);
        Vec3 scr[3], wld[3];
        for (int v = 0; v < 3; v++) {
            wld[v] = myModel.vert(face[v]);
            scr[v] = proj(MVP * embed(wld[v]));
        }
        // ... render face
    }
}
```

## Quick Reference

### Common Shapes Cheat Sheet

| Shape | Vertices | Faces | Complexity |
|-------|----------|-------|------------|
| Tetrahedron (Pyramid) | 4 | 4 | ⭐ Easy |
| Cube | 8 | 12 | ⭐ Easy |
| Octahedron | 6 | 8 | ⭐⭐ Medium |
| Prism (Triangular) | 6 | 8 | ⭐⭐ Medium |
| Icosahedron | 12 | 20 | ⭐⭐⭐ Hard |
| Sphere (approx) | 20+ | 40+ | ⭐⭐⭐ Hard |

### Vertex Count Formula

For regular polyhedrons:
- **Faces**: Number of triangular surfaces
- **Vertices**: Corner points
- **Edges**: Lines connecting vertices

**Euler's Formula**: V - E + F = 2
- V = Vertices
- E = Edges
- F = Faces

## Next Steps

1. **Try the pyramid example** above
2. **Experiment with vertex positions** to see how shape changes
3. **Create your own unique shape**
4. **Load complex models** from OBJ files
5. **Add multiple shapes** to the scene (render multiple times with different positions)

Happy modeling! 🎨✨
