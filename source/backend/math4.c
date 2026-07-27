// mat4_multiply is the hot path (one call per mesh draw: mvp = view_proj *
// model), so it gets a SIMD implementation — SSE on x86/x86_64, NEON on
// ARM/AArch64 — with a plain scalar fallback for anything else. Every other
// Mat4 builder here runs at most once or twice per frame per object and
// stays scalar; vectorizing them wouldn't move the needle.
#include "math4.h"

#include <math.h>
#include <string.h>

#if defined(__x86_64__) || defined(_M_X64) || defined(__SSE__) || (defined(_M_IX86_FP) && _M_IX86_FP >= 1)
#define MATH4_SSE 1
#include <xmmintrin.h>
#elif defined(__ARM_NEON) || defined(__ARM_NEON__) || defined(_M_ARM64) || defined(_M_ARM)
#define MATH4_NEON 1
#include <arm_neon.h>
#endif

Mat4 mat4_identity(void) {
    Mat4 r;
    memset(r.m, 0, sizeof(r.m));
    r.m[0] = r.m[5] = r.m[10] = r.m[15] = 1.0f;
    return r;
}

#if defined(MATH4_SSE)

Mat4 mat4_multiply(Mat4 a, Mat4 b) {
    Mat4 r;
    __m128 a0 = _mm_loadu_ps(&a.m[0]);
    __m128 a1 = _mm_loadu_ps(&a.m[4]);
    __m128 a2 = _mm_loadu_ps(&a.m[8]);
    __m128 a3 = _mm_loadu_ps(&a.m[12]);

    for (int col = 0; col < 4; col++) {
        __m128 b0 = _mm_set1_ps(b.m[col * 4 + 0]);
        __m128 b1 = _mm_set1_ps(b.m[col * 4 + 1]);
        __m128 b2 = _mm_set1_ps(b.m[col * 4 + 2]);
        __m128 b3 = _mm_set1_ps(b.m[col * 4 + 3]);
        __m128 out = _mm_add_ps(_mm_add_ps(_mm_mul_ps(a0, b0), _mm_mul_ps(a1, b1)),
                                 _mm_add_ps(_mm_mul_ps(a2, b2), _mm_mul_ps(a3, b3)));
        _mm_storeu_ps(&r.m[col * 4], out);
    }
    return r;
}

#elif defined(MATH4_NEON)

Mat4 mat4_multiply(Mat4 a, Mat4 b) {
    Mat4 r;
    float32x4_t a0 = vld1q_f32(&a.m[0]);
    float32x4_t a1 = vld1q_f32(&a.m[4]);
    float32x4_t a2 = vld1q_f32(&a.m[8]);
    float32x4_t a3 = vld1q_f32(&a.m[12]);

    for (int col = 0; col < 4; col++) {
        float32x4_t out = vmulq_n_f32(a0, b.m[col * 4 + 0]);
        out = vmlaq_n_f32(out, a1, b.m[col * 4 + 1]);
        out = vmlaq_n_f32(out, a2, b.m[col * 4 + 2]);
        out = vmlaq_n_f32(out, a3, b.m[col * 4 + 3]);
        vst1q_f32(&r.m[col * 4], out);
    }
    return r;
}

#else

Mat4 mat4_multiply(Mat4 a, Mat4 b) {
    Mat4 r;
    for (int col = 0; col < 4; col++) {
        for (int row = 0; row < 4; row++) {
            float sum = 0.0f;
            for (int k = 0; k < 4; k++) sum += a.m[k * 4 + row] * b.m[col * 4 + k];
            r.m[col * 4 + row] = sum;
        }
    }
    return r;
}

#endif

Mat4 mat4_translate(float x, float y, float z) {
    Mat4 r = mat4_identity();
    r.m[12] = x;
    r.m[13] = y;
    r.m[14] = z;
    return r;
}

Mat4 mat4_scale(float x, float y, float z) {
    Mat4 r = mat4_identity();
    r.m[0] = x;
    r.m[5] = y;
    r.m[10] = z;
    return r;
}

static Mat4 mat4_rotate_x(float a) {
    Mat4 r = mat4_identity();
    float c = cosf(a), s = sinf(a);
    r.m[5] = c;  r.m[6] = s;
    r.m[9] = -s; r.m[10] = c;
    return r;
}

static Mat4 mat4_rotate_y(float a) {
    Mat4 r = mat4_identity();
    float c = cosf(a), s = sinf(a);
    r.m[0] = c;  r.m[2] = -s;
    r.m[8] = s;  r.m[10] = c;
    return r;
}

static Mat4 mat4_rotate_z(float a) {
    Mat4 r = mat4_identity();
    float c = cosf(a), s = sinf(a);
    r.m[0] = c;  r.m[1] = s;
    r.m[4] = -s; r.m[5] = c;
    return r;
}

// Applies X, then Y, then Z (R = Rz * Ry * Rx).
Mat4 mat4_rotate_xyz(float rx, float ry, float rz) {
    Mat4 r = mat4_multiply(mat4_rotate_y(ry), mat4_rotate_x(rx));
    return mat4_multiply(mat4_rotate_z(rz), r);
}

Mat4 mat4_perspective(float fov_deg, float aspect, float near_z, float far_z) {
    Mat4 r;
    memset(r.m, 0, sizeof(r.m));
    float f = 1.0f / tanf(fov_deg * (float)M_PI / 360.0f);
    r.m[0] = f / aspect;
    r.m[5] = f;
    r.m[10] = (far_z + near_z) / (near_z - far_z);
    r.m[11] = -1.0f;
    r.m[14] = (2.0f * far_z * near_z) / (near_z - far_z);
    return r;
}

Mat4 mat4_look_at(float ex, float ey, float ez, float tx, float ty, float tz, float ux, float uy, float uz) {
    float fx = tx - ex, fy = ty - ey, fz = tz - ez;
    float flen = sqrtf(fx * fx + fy * fy + fz * fz);
    if (flen > 0.0001f) { fx /= flen; fy /= flen; fz /= flen; }

    float sx = fy * uz - fz * uy;
    float sy = fz * ux - fx * uz;
    float sz = fx * uy - fy * ux;
    float slen = sqrtf(sx * sx + sy * sy + sz * sz);
    if (slen > 0.0001f) { sx /= slen; sy /= slen; sz /= slen; }

    float ux2 = sy * fz - sz * fy;
    float uy2 = sz * fx - sx * fz;
    float uz2 = sx * fy - sy * fx;

    Mat4 r = mat4_identity();
    r.m[0] = sx;  r.m[4] = sy;  r.m[8] = sz;
    r.m[1] = ux2; r.m[5] = uy2; r.m[9] = uz2;
    r.m[2] = -fx; r.m[6] = -fy; r.m[10] = -fz;
    r.m[12] = -(sx * ex + sy * ey + sz * ez);
    r.m[13] = -(ux2 * ex + uy2 * ey + uz2 * ez);
    r.m[14] = fx * ex + fy * ey + fz * ez;
    return r;
}

// near=-1/far=1 fixed: enough for the 2D HUD pass, which only needs x/y.
Mat4 mat4_ortho(float left, float right, float bottom, float top) {
    Mat4 r = mat4_identity();
    r.m[0] = 2.0f / (right - left);
    r.m[5] = 2.0f / (top - bottom);
    r.m[10] = -1.0f;
    r.m[12] = -(right + left) / (right - left);
    r.m[13] = -(top + bottom) / (top - bottom);
    return r;
}
