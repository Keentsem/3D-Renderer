#ifndef TRANSFORM_H
#define TRANSFORM_H

#include "../core/mat.h"
#include "../core/vec.h"
#include <cmath>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// ============================================================
// Viewport Transformation
// Maps NDC [-1,1]³ to screen coordinates
// ============================================================

Mat4 viewport(int x, int y, int w, int h, int depth = 255) {
    Mat4 m = Mat4::identity();

    // Scale
    m[0][0] = w / 2.0;
    m[1][1] = h / 2.0;
    m[2][2] = depth / 2.0;

    // Translate
    m[0][3] = x + w / 2.0;
    m[1][3] = y + h / 2.0;
    m[2][3] = depth / 2.0;

    return m;
}

// ============================================================
// Projection Matrices
// ============================================================

// Simple perspective: c = -1/camera_distance
// Camera at (0, 0, camera_distance) looking at origin
Mat4 perspective(double c) {
    Mat4 m = Mat4::identity();
    m[3][2] = c;  // This sets w' = 1 + z*c
    return m;
}

// Standard perspective with field of view
Mat4 perspective_fov(double fov_radians, double aspect, double near, double far) {
    Mat4 m;  // All zeros

    double tanHalfFov = std::tan(fov_radians / 2.0);

    m[0][0] = 1.0 / (aspect * tanHalfFov);
    m[1][1] = 1.0 / tanHalfFov;
    m[2][2] = -(far + near) / (far - near);
    m[2][3] = -(2.0 * far * near) / (far - near);
    m[3][2] = -1.0;
    // m[3][3] = 0  (implicit)

    return m;
}

// Orthographic projection (no perspective)
Mat4 orthographic(double left, double right, double bottom, double top,
                  double near, double far) {
    Mat4 m = Mat4::identity();

    m[0][0] = 2.0 / (right - left);
    m[1][1] = 2.0 / (top - bottom);
    m[2][2] = -2.0 / (far - near);

    m[0][3] = -(right + left) / (right - left);
    m[1][3] = -(top + bottom) / (top - bottom);
    m[2][3] = -(far + near) / (far - near);

    return m;
}

// ============================================================
// View Matrix (Look-At)
// ============================================================

Mat4 lookat(const Vec3& eye, const Vec3& center, const Vec3& up) {
    // Camera basis vectors
    Vec3 z = normalized(eye - center);     // Camera looks down -z
    Vec3 x = normalized(cross(up, z));     // Camera right
    Vec3 y = normalized(cross(z, x));      // Camera up (orthogonalized)

    // Build rotation matrix (inverse = transpose for orthonormal)
    Mat4 Minv = Mat4::identity();
    for (int i = 0; i < 3; i++) {
        Minv[0][i] = x[i];
        Minv[1][i] = y[i];
        Minv[2][i] = z[i];
    }

    // Translation (move world opposite to camera)
    Mat4 Tr = Mat4::identity();
    Tr[0][3] = -eye.x;
    Tr[1][3] = -eye.y;
    Tr[2][3] = -eye.z;

    return Minv * Tr;
}

// ============================================================
// Model Transformations
// ============================================================

Mat4 translate(double x, double y, double z) {
    Mat4 m = Mat4::identity();
    m[0][3] = x;
    m[1][3] = y;
    m[2][3] = z;
    return m;
}

Mat4 translate(const Vec3& v) {
    return translate(v.x, v.y, v.z);
}

Mat4 scale(double sx, double sy, double sz) {
    Mat4 m = Mat4::identity();
    m[0][0] = sx;
    m[1][1] = sy;
    m[2][2] = sz;
    return m;
}

Mat4 scale(double s) {
    return scale(s, s, s);
}

Mat4 rotate_x(double angle) {
    Mat4 m = Mat4::identity();
    double c = std::cos(angle);
    double s = std::sin(angle);
    m[1][1] = c;  m[1][2] = -s;
    m[2][1] = s;  m[2][2] = c;
    return m;
}

Mat4 rotate_y(double angle) {
    Mat4 m = Mat4::identity();
    double c = std::cos(angle);
    double s = std::sin(angle);
    m[0][0] = c;  m[0][2] = s;
    m[2][0] = -s; m[2][2] = c;
    return m;
}

Mat4 rotate_z(double angle) {
    Mat4 m = Mat4::identity();
    double c = std::cos(angle);
    double s = std::sin(angle);
    m[0][0] = c;  m[0][1] = -s;
    m[1][0] = s;  m[1][1] = c;
    return m;
}

// Rotation around arbitrary axis (Rodrigues' formula)
Mat4 rotate(const Vec3& axis, double angle) {
    Vec3 n = normalized(axis);
    double c = std::cos(angle);
    double s = std::sin(angle);
    double t = 1.0 - c;

    Mat4 m = Mat4::identity();

    m[0][0] = t * n.x * n.x + c;
    m[0][1] = t * n.x * n.y - s * n.z;
    m[0][2] = t * n.x * n.z + s * n.y;

    m[1][0] = t * n.x * n.y + s * n.z;
    m[1][1] = t * n.y * n.y + c;
    m[1][2] = t * n.y * n.z - s * n.x;

    m[2][0] = t * n.x * n.z - s * n.y;
    m[2][1] = t * n.y * n.z + s * n.x;
    m[2][2] = t * n.z * n.z + c;

    return m;
}

// ============================================================
// Utilities
// ============================================================

inline double radians(double degrees) {
    return degrees * M_PI / 180.0;
}

inline double degrees(double radians_val) {
    return radians_val * 180.0 / M_PI;
}

#endif
