// Merged GL2/GLES2 renderer (source/backend/gl.c).
//
// Built on the glad loader generated with `--merge --api='gl:core=2.1,gles2=2.0'`
// (vendor/opengl/glad/src/gl.c): a single glad_gl* pointer table is shared by
// both API flavors, so gl.c never branches on GLES vs desktop GL — it only
// calls the subset of GL2/GLES2 that both share (no VAOs, no glBegin,
// glClearDepthf/glDepthRangef instead of the desktop-only double variants).
#ifndef STARTSHIPPER_GL_H
#define STARTSHIPPER_GL_H

#include <stdbool.h>

#include "math4.h"

// Matches retro_hw_get_proc_address_t / GLADloadfunc exactly (a function
// returning a function pointer, not void*) so no pointer-type punning is
// needed when handing libretro's callback straight to gladLoadGL/GLES2.
typedef void (*gl_generic_proc)(void);
typedef gl_generic_proc (*gl_proc_address_fn)(const char *sym);

typedef struct {
    float x, y, z;
    float r, g, b, a;
} GlVertex;

typedef struct {
    unsigned int vbo;
    int vertex_count;
    int capacity;
} GlMesh;

// context_type: pass true for a GLES2 context, false for desktop GL.
// get_proc_address must be the pointer libretro's retro_hw_render_callback
// handed back after RETRO_ENVIRONMENT_SET_HW_RENDER.
bool gl_backend_init(gl_proc_address_fn get_proc_address, bool is_gles);
void gl_backend_shutdown(void);

// Binds `fbo` (0 = default), sets the viewport and clears color+depth.
void gl_begin_frame(unsigned int fbo, int width, int height, float clear_r, float clear_g, float clear_b);
void gl_set_viewport(int x, int y, int width, int height);

// view/proj are combined once per frame; gl_mesh_draw multiplies by model.
void gl_set_camera(Mat4 view, Mat4 proj);

GlMesh gl_mesh_create(const GlVertex *vertices, int count);
void   gl_mesh_update(GlMesh *mesh, const GlVertex *vertices, int count);
void   gl_mesh_destroy(GlMesh *mesh);
void   gl_mesh_draw(const GlMesh *mesh, Mat4 model);

// Immediate 2D HUD helpers: pixel-space coordinates, origin top-left,
// depth test disabled, alpha blended. screen_w/screen_h size the ortho proj.
void gl_draw_quad2d(float x, float y, float w, float h, float r, float g, float b, float a, int screen_w, int screen_h);
void gl_draw_triangle2d(float x0, float y0, float x1, float y1, float x2, float y2,
                         float r, float g, float b, float a, int screen_w, int screen_h);

// One draw call for an arbitrary batch of pre-built 2D triangles (pixel
// space, same convention as gl_draw_quad2d) — for callers that want to
// submit many quads/triangles without one upload per shape.
void gl_draw_batch2d(const GlVertex *verts, int count, int screen_w, int screen_h);

#endif
