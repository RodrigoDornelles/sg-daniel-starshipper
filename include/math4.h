// 4x4 matrix math. mat4_multiply (the hot path — one call per mesh draw to
// fold model into the frame's view*proj) has a SIMD implementation in
// source/backend/math4.c: SSE on x86, NEON on ARM, plain scalar elsewhere.
#ifndef STARTSHIPPER_MATH4_H
#define STARTSHIPPER_MATH4_H

typedef struct {
    float m[16]; // column-major, OpenGL convention
} Mat4;

Mat4 mat4_identity(void);
Mat4 mat4_multiply(Mat4 a, Mat4 b);
Mat4 mat4_translate(float x, float y, float z);
Mat4 mat4_scale(float x, float y, float z);
Mat4 mat4_rotate_xyz(float rx, float ry, float rz);
Mat4 mat4_perspective(float fov_deg, float aspect, float near_z, float far_z);
Mat4 mat4_look_at(float ex, float ey, float ez, float tx, float ty, float tz, float ux, float uy, float uz);
Mat4 mat4_ortho(float left, float right, float bottom, float top);

#endif
