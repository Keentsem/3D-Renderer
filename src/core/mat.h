#ifndef MAT_H
#define MAT_H

#include "vec.h"

template<int nrows, int ncols> struct mat {
    vec<ncols> rows[nrows] = {{}};

    vec<ncols>& operator[](const int i) { return rows[i]; }
    const vec<ncols>& operator[](const int i) const { return rows[i]; }

    vec<nrows> col(const int i) const {
        vec<nrows> ret;
        for (int j = 0; j < nrows; j++) ret[j] = rows[j][i];
        return ret;
    }

    void set_col(const int i, const vec<nrows>& v) {
        for (int j = 0; j < nrows; j++) rows[j][i] = v[j];
    }

    static mat<nrows, ncols> identity() {
        mat<nrows, ncols> ret;
        for (int i = 0; i < nrows && i < ncols; i++) ret[i][i] = 1;
        return ret;
    }
};

// Matrix-vector multiplication
template<int nrows, int ncols>
vec<nrows> operator*(const mat<nrows, ncols>& lhs, const vec<ncols>& rhs) {
    vec<nrows> ret;
    for (int i = 0; i < nrows; i++) ret[i] = lhs[i] * rhs;
    return ret;
}

// Matrix-matrix multiplication
template<int R1, int C1, int C2>
mat<R1, C2> operator*(const mat<R1, C1>& lhs, const mat<C1, C2>& rhs) {
    mat<R1, C2> ret;
    for (int i = 0; i < R1; i++)
        for (int j = 0; j < C2; j++)
            ret[i][j] = lhs[i] * rhs.col(j);
    return ret;
}

// Transpose
template<int nrows, int ncols>
mat<ncols, nrows> transpose(const mat<nrows, ncols>& m) {
    mat<ncols, nrows> ret;
    for (int i = 0; i < nrows; i++)
        for (int j = 0; j < ncols; j++)
            ret[j][i] = m[i][j];
    return ret;
}

// 4x4 inverse (needed for normal transformation)
inline mat<4, 4> inverse(const mat<4, 4>& m) {
    mat<4, 4> ret;
    // Cofactor expansion
    double A2323 = m[2][2] * m[3][3] - m[2][3] * m[3][2];
    double A1323 = m[2][1] * m[3][3] - m[2][3] * m[3][1];
    double A1223 = m[2][1] * m[3][2] - m[2][2] * m[3][1];
    double A0323 = m[2][0] * m[3][3] - m[2][3] * m[3][0];
    double A0223 = m[2][0] * m[3][2] - m[2][2] * m[3][0];
    double A0123 = m[2][0] * m[3][1] - m[2][1] * m[3][0];
    double A2313 = m[1][2] * m[3][3] - m[1][3] * m[3][2];
    double A1313 = m[1][1] * m[3][3] - m[1][3] * m[3][1];
    double A1213 = m[1][1] * m[3][2] - m[1][2] * m[3][1];
    double A2312 = m[1][2] * m[2][3] - m[1][3] * m[2][2];
    double A1312 = m[1][1] * m[2][3] - m[1][3] * m[2][1];
    double A1212 = m[1][1] * m[2][2] - m[1][2] * m[2][1];
    double A0313 = m[1][0] * m[3][3] - m[1][3] * m[3][0];
    double A0213 = m[1][0] * m[3][2] - m[1][2] * m[3][0];
    double A0312 = m[1][0] * m[2][3] - m[1][3] * m[2][0];
    double A0212 = m[1][0] * m[2][2] - m[1][2] * m[2][0];
    double A0113 = m[1][0] * m[3][1] - m[1][1] * m[3][0];
    double A0112 = m[1][0] * m[2][1] - m[1][1] * m[2][0];

    double det = m[0][0] * (m[1][1] * A2323 - m[1][2] * A1323 + m[1][3] * A1223)
               - m[0][1] * (m[1][0] * A2323 - m[1][2] * A0323 + m[1][3] * A0223)
               + m[0][2] * (m[1][0] * A1323 - m[1][1] * A0323 + m[1][3] * A0123)
               - m[0][3] * (m[1][0] * A1223 - m[1][1] * A0223 + m[1][2] * A0123);

    double invdet = 1.0 / det;

    ret[0][0] = invdet *  (m[1][1] * A2323 - m[1][2] * A1323 + m[1][3] * A1223);
    ret[0][1] = invdet * -(m[0][1] * A2323 - m[0][2] * A1323 + m[0][3] * A1223);
    ret[0][2] = invdet *  (m[0][1] * A2313 - m[0][2] * A1313 + m[0][3] * A1213);
    ret[0][3] = invdet * -(m[0][1] * A2312 - m[0][2] * A1312 + m[0][3] * A1212);
    ret[1][0] = invdet * -(m[1][0] * A2323 - m[1][2] * A0323 + m[1][3] * A0223);
    ret[1][1] = invdet *  (m[0][0] * A2323 - m[0][2] * A0323 + m[0][3] * A0223);
    ret[1][2] = invdet * -(m[0][0] * A2313 - m[0][2] * A0313 + m[0][3] * A0213);
    ret[1][3] = invdet *  (m[0][0] * A2312 - m[0][2] * A0312 + m[0][3] * A0212);
    ret[2][0] = invdet *  (m[1][0] * A1323 - m[1][1] * A0323 + m[1][3] * A0123);
    ret[2][1] = invdet * -(m[0][0] * A1323 - m[0][1] * A0323 + m[0][3] * A0123);
    ret[2][2] = invdet *  (m[0][0] * A1313 - m[0][1] * A0313 + m[0][3] * A0113);
    ret[2][3] = invdet * -(m[0][0] * A1312 - m[0][1] * A0312 + m[0][3] * A0112);
    ret[3][0] = invdet * -(m[1][0] * A1223 - m[1][1] * A0223 + m[1][2] * A0123);
    ret[3][1] = invdet *  (m[0][0] * A1223 - m[0][1] * A0223 + m[0][2] * A0123);
    ret[3][2] = invdet * -(m[0][0] * A1213 - m[0][1] * A0213 + m[0][2] * A0113);
    ret[3][3] = invdet *  (m[0][0] * A1212 - m[0][1] * A0212 + m[0][2] * A0112);

    return ret;
}

typedef mat<3, 3> Mat3;
typedef mat<4, 4> Mat4;

#endif
