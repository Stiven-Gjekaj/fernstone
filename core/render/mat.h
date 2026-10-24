// mat.h: 4 x 4 matrices in column order, as GLES2 reads them.
#ifndef FERN_MAT_H
#define FERN_MAT_H

#include <math.h>
#include <string.h>

typedef struct {
    float m[16];
} FernMat;

static inline FernMat fern_mat_identity(void) {
    FernMat r;
    memset(&r, 0, sizeof(r));
    r.m[0] = r.m[5] = r.m[10] = r.m[15] = 1;
    return r;
}

static inline FernMat fern_mat_mul(FernMat a, FernMat b) {
    FernMat r;
    int i, j, k;
    for (i = 0; i < 4; i++)
        for (j = 0; j < 4; j++) {
            float s = 0;
            for (k = 0; k < 4; k++) s += a.m[k * 4 + j] * b.m[i * 4 + k];
            r.m[i * 4 + j] = s;
        }
    return r;
}

static inline FernMat fern_mat_perspective(float fov_deg, float aspect, float n, float f) {
    FernMat r;
    float t = 1.0f / tanf(fov_deg * (float)M_PI / 360.0f);
    memset(&r, 0, sizeof(r));
    r.m[0] = t / aspect;
    r.m[5] = t;
    r.m[10] = (f + n) / (n - f);
    r.m[11] = -1;
    r.m[14] = 2 * f * n / (n - f);
    return r;
}

static inline FernMat fern_mat_ortho(float l, float r_, float b, float t, float n, float f) {
    FernMat r = fern_mat_identity();
    r.m[0] = 2 / (r_ - l);
    r.m[5] = 2 / (t - b);
    r.m[10] = -2 / (f - n);
    r.m[12] = -(r_ + l) / (r_ - l);
    r.m[13] = -(t + b) / (t - b);
    r.m[14] = -(f + n) / (f - n);
    return r;
}

static inline FernMat fern_mat_rotate(float deg, float x, float y, float z) {
    FernMat r = fern_mat_identity();
    float a = deg * (float)M_PI / 180.0f, c = cosf(a), s = sinf(a), t = 1 - c;
    r.m[0] = t * x * x + c;     r.m[4] = t * x * y - s * z; r.m[8] = t * x * z + s * y;
    r.m[1] = t * x * y + s * z; r.m[5] = t * y * y + c;     r.m[9] = t * y * z - s * x;
    r.m[2] = t * x * z - s * y; r.m[6] = t * y * z + s * x; r.m[10] = t * z * z + c;
    return r;
}

static inline FernMat fern_mat_translate(float x, float y, float z) {
    FernMat r = fern_mat_identity();
    r.m[12] = x;
    r.m[13] = y;
    r.m[14] = z;
    return r;
}

static inline FernMat fern_mat_scale(float x, float y, float z) {
    FernMat r = fern_mat_identity();
    r.m[0] = x;
    r.m[5] = y;
    r.m[10] = z;
    return r;
}

#endif
