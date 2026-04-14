#ifndef VEC_H
#define VEC_H

#include <cmath>
#include <cassert>
#include <iostream>

// Generic n-dimensional vector
template<int n> struct vec {
    double data[n] = {0};

    double& operator[](const int i) {
        assert(i >= 0 && i < n);
        return data[i];
    }
    double operator[](const int i) const {
        assert(i >= 0 && i < n);
        return data[i];
    }
    double norm2() const { return (*this) * (*this); }
    double norm() const { return std::sqrt(norm2()); }
};

// Dot product
template<int n>
double operator*(const vec<n>& lhs, const vec<n>& rhs) {
    double ret = 0;
    for (int i = 0; i < n; i++) ret += lhs[i] * rhs[i];
    return ret;
}

// Scalar operations
template<int n> vec<n> operator*(const double& rhs, const vec<n>& lhs) {
    vec<n> ret;
    for (int i = 0; i < n; i++) ret[i] = lhs[i] * rhs;
    return ret;
}
template<int n> vec<n> operator*(const vec<n>& lhs, const double& rhs) { return rhs * lhs; }
template<int n> vec<n> operator/(const vec<n>& lhs, const double& rhs) {
    vec<n> ret;
    for (int i = 0; i < n; i++) ret[i] = lhs[i] / rhs;
    return ret;
}

// Vector operations
template<int n> vec<n> operator+(const vec<n>& lhs, const vec<n>& rhs) {
    vec<n> ret;
    for (int i = 0; i < n; i++) ret[i] = lhs[i] + rhs[i];
    return ret;
}
template<int n> vec<n> operator-(const vec<n>& lhs, const vec<n>& rhs) {
    vec<n> ret;
    for (int i = 0; i < n; i++) ret[i] = lhs[i] - rhs[i];
    return ret;
}
template<int n> vec<n> operator-(const vec<n>& lhs) { return lhs * (-1.0); }

template<int n> vec<n> normalized(const vec<n>& v) { return v / v.norm(); }

// Specialized Vec2
template<> struct vec<2> {
    double x = 0, y = 0;
    vec() = default;
    vec(double x, double y) : x(x), y(y) {}
    double& operator[](const int i) { return i == 0 ? x : y; }
    double operator[](const int i) const { return i == 0 ? x : y; }
    double norm2() const { return x*x + y*y; }
    double norm() const { return std::sqrt(norm2()); }
};

// Specialized Vec3
template<> struct vec<3> {
    double x = 0, y = 0, z = 0;
    vec() = default;
    vec(double x, double y, double z) : x(x), y(y), z(z) {}
    double& operator[](const int i) {
        if (i == 0) return x; if (i == 1) return y; return z;
    }
    double operator[](const int i) const {
        if (i == 0) return x; if (i == 1) return y; return z;
    }
    double norm2() const { return x*x + y*y + z*z; }
    double norm() const { return std::sqrt(norm2()); }
};

// Specialized Vec4 (homogeneous coordinates)
template<> struct vec<4> {
    double x = 0, y = 0, z = 0, w = 0;
    vec() = default;
    vec(double x, double y, double z, double w = 1) : x(x), y(y), z(z), w(w) {}
    double& operator[](const int i) {
        if (i == 0) return x; if (i == 1) return y; if (i == 2) return z; return w;
    }
    double operator[](const int i) const {
        if (i == 0) return x; if (i == 1) return y; if (i == 2) return z; return w;
    }
    double norm2() const { return x*x + y*y + z*z + w*w; }
    double norm() const { return std::sqrt(norm2()); }
};

// Cross product (3D only) - gives perpendicular vector
inline vec<3> cross(const vec<3>& v1, const vec<3>& v2) {
    return vec<3>(
        v1.y * v2.z - v1.z * v2.y,
        v1.z * v2.x - v1.x * v2.z,
        v1.x * v2.y - v1.y * v2.x
    );
}

// Homogeneous coordinate conversions
inline vec<4> embed(const vec<3>& v, double fill = 1) {
    return vec<4>(v.x, v.y, v.z, fill);
}
inline vec<3> proj(const vec<4>& v) {
    return vec<3>(v.x / v.w, v.y / v.w, v.z / v.w);
}

// Type aliases
typedef vec<2> Vec2;
typedef vec<3> Vec3;
typedef vec<4> Vec4;

// Integer versions for screen coordinates
struct Vec2i { int x = 0, y = 0; Vec2i() = default; Vec2i(int x, int y) : x(x), y(y) {} };
struct Vec3i { int x = 0, y = 0, z = 0; Vec3i() = default; Vec3i(int x, int y, int z) : x(x), y(y), z(z) {} };

#endif
