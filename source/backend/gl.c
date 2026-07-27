#include "gl.h"
#include "glad/gl.h"

#include <stddef.h>
#include <string.h>

// GLSL sources live in source/shaders/*.{vert,frag} and get baked into these
// arrays by tools/xxd.c at build time (see CMakeLists.txt) — no #version
// pragma, precision guarded by GL_ES, so they compile unmodified under
// desktop GL2.1 and GLES2.
#include "gecnd/shader_basic_vert.h"
#include "gecnd/shader_basic_frag.h"

static struct {
    unsigned int program;
    int u_mvp;
    Mat4 view_proj;
    unsigned int dynamic_vbo;
    bool initialized;
} g_gl;

static unsigned int compile_shader(unsigned int type, const char *src, int len) {
    unsigned int shader = glCreateShader(type);
    glShaderSource(shader, 1, &src, &len);
    glCompileShader(shader);
    int ok = 0;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        glDeleteShader(shader);
        return 0;
    }
    return shader;
}

bool gl_backend_init(gl_proc_address_fn get_proc_address, bool is_gles) {
    int loaded = is_gles ? gladLoadGLES2((GLADloadfunc)get_proc_address)
                         : gladLoadGL((GLADloadfunc)get_proc_address);
    if (!loaded) return false;

    unsigned int vs = compile_shader(GL_VERTEX_SHADER, (const char *)SHADER_BASIC_VERT_SRC, (int)SHADER_BASIC_VERT_SRC_len);
    unsigned int fs = compile_shader(GL_FRAGMENT_SHADER, (const char *)SHADER_BASIC_FRAG_SRC, (int)SHADER_BASIC_FRAG_SRC_len);
    if (!vs || !fs) return false;

    unsigned int program = glCreateProgram();
    glAttachShader(program, vs);
    glAttachShader(program, fs);
    glBindAttribLocation(program, 0, "aPosition");
    glBindAttribLocation(program, 1, "aColor");
    glLinkProgram(program);

    int linked = 0;
    glGetProgramiv(program, GL_LINK_STATUS, &linked);
    glDeleteShader(vs);
    glDeleteShader(fs);
    if (!linked) {
        glDeleteProgram(program);
        return false;
    }

    g_gl.program = program;
    g_gl.u_mvp = glGetUniformLocation(program, "uMVP");
    g_gl.view_proj = mat4_identity();
    glGenBuffers(1, &g_gl.dynamic_vbo);

    g_gl.initialized = true;
    return true;
}

void gl_backend_shutdown(void) {
    if (g_gl.dynamic_vbo) glDeleteBuffers(1, &g_gl.dynamic_vbo);
    if (g_gl.program) glDeleteProgram(g_gl.program);
    memset(&g_gl, 0, sizeof(g_gl));
}

// Re-asserts blend/depth state every frame rather than relying on it
// surviving from context_reset: the frontend's own GL usage between our
// retro_run() calls (menu, overlay, its own HW-render bookkeeping) is free
// to leave blending disabled, which otherwise silently turns every alpha-
// tested draw (HUD quads, text) into a fully opaque one.
void gl_begin_frame(unsigned int fbo, int width, int height, float clear_r, float clear_g, float clear_b) {
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    glViewport(0, 0, width, height);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDepthMask(GL_TRUE);
    glClearColor(clear_r, clear_g, clear_b, 1.0f);
    glClearDepthf(1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glDepthFunc(GL_LEQUAL);
}

void gl_set_camera(Mat4 view, Mat4 proj) {
    g_gl.view_proj = mat4_multiply(proj, view);
}

GlMesh gl_mesh_create(const GlVertex *vertices, int count) {
    GlMesh mesh;
    memset(&mesh, 0, sizeof(mesh));
    glGenBuffers(1, &mesh.vbo);
    glBindBuffer(GL_ARRAY_BUFFER, mesh.vbo);
    glBufferData(GL_ARRAY_BUFFER, (long)(count * (int)sizeof(GlVertex)), vertices, GL_STATIC_DRAW);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    mesh.vertex_count = count;
    mesh.capacity = count;
    return mesh;
}

void gl_mesh_update(GlMesh *mesh, const GlVertex *vertices, int count) {
    glBindBuffer(GL_ARRAY_BUFFER, mesh->vbo);
    if (count > mesh->capacity) {
        glBufferData(GL_ARRAY_BUFFER, (long)(count * (int)sizeof(GlVertex)), vertices, GL_DYNAMIC_DRAW);
        mesh->capacity = count;
    } else {
        glBufferSubData(GL_ARRAY_BUFFER, 0, (long)(count * (int)sizeof(GlVertex)), vertices);
    }
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    mesh->vertex_count = count;
}

void gl_mesh_destroy(GlMesh *mesh) {
    if (mesh->vbo) glDeleteBuffers(1, &mesh->vbo);
    mesh->vbo = 0;
    mesh->vertex_count = 0;
    mesh->capacity = 0;
}

static void bind_vertex_layout(void) {
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(GlVertex), (const void *)offsetof(GlVertex, x));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, sizeof(GlVertex), (const void *)offsetof(GlVertex, r));
}

static void unbind_vertex_layout(void) {
    glDisableVertexAttribArray(0);
    glDisableVertexAttribArray(1);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
}

void gl_mesh_draw(const GlMesh *mesh, Mat4 model) {
    if (!mesh->vbo || mesh->vertex_count <= 0) return;
    glEnable(GL_DEPTH_TEST);
    glDepthMask(GL_TRUE);
    glUseProgram(g_gl.program);
    Mat4 mvp = mat4_multiply(g_gl.view_proj, model);
    glUniformMatrix4fv(g_gl.u_mvp, 1, GL_FALSE, mvp.m);
    glBindBuffer(GL_ARRAY_BUFFER, mesh->vbo);
    bind_vertex_layout();
    glDrawArrays(GL_TRIANGLES, 0, mesh->vertex_count);
    unbind_vertex_layout();
}

void gl_draw_batch2d(const GlVertex *verts, int count, int screen_w, int screen_h) {
    if (count <= 0) return;
    glDisable(GL_DEPTH_TEST);
    glUseProgram(g_gl.program);
    Mat4 proj = mat4_ortho(0.0f, (float)screen_w, (float)screen_h, 0.0f);
    glUniformMatrix4fv(g_gl.u_mvp, 1, GL_FALSE, proj.m);
    glBindBuffer(GL_ARRAY_BUFFER, g_gl.dynamic_vbo);
    glBufferData(GL_ARRAY_BUFFER, (long)(count * (int)sizeof(GlVertex)), verts, GL_DYNAMIC_DRAW);
    bind_vertex_layout();
    glDrawArrays(GL_TRIANGLES, 0, count);
    unbind_vertex_layout();
}

void gl_draw_quad2d(float x, float y, float w, float h, float r, float g, float b, float a, int screen_w, int screen_h) {
    GlVertex v[6] = {
        {x,     y,     0, r, g, b, a},
        {x + w, y,     0, r, g, b, a},
        {x + w, y + h, 0, r, g, b, a},
        {x,     y,     0, r, g, b, a},
        {x + w, y + h, 0, r, g, b, a},
        {x,     y + h, 0, r, g, b, a},
    };
    gl_draw_batch2d(v, 6, screen_w, screen_h);
}

void gl_draw_triangle2d(float x0, float y0, float x1, float y1, float x2, float y2,
                         float r, float g, float b, float a, int screen_w, int screen_h) {
    GlVertex v[3] = {
        {x0, y0, 0, r, g, b, a},
        {x1, y1, 0, r, g, b, a},
        {x2, y2, 0, r, g, b, a},
    };
    gl_draw_batch2d(v, 3, screen_w, screen_h);
}
