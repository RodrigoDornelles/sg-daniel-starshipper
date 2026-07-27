// HUD/menu text: fontstash rasterizes the embedded Roboto TTF into an alpha
// atlas; this file only implements the four GL callbacks fontstash needs
// (create/update/draw/delete the atlas texture) plus a small dedicated
// textured-quad shader — independent of gl.c's solid-color mesh pipeline.
#include "text.h"
#include "glad/gl.h"
#include "math4.h"

#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define FONTSTASH_IMPLEMENTATION
#include "fontstash.h"

#include "gecnd/roboto_regular.h"
#include "gecnd/shader_text_vert.h"
#include "gecnd/shader_text_frag.h"

typedef struct {
    float x, y, u, v, r, g, b, a;
} TextVertex;

#define TEXT_BATCH_CAP 1024 // matches fontstash's own FONS_VERTEX_COUNT

static struct {
    unsigned int program;
    int u_mvp;
    int u_tex;
    unsigned int vbo;
    unsigned int atlas_tex;
    int atlas_w, atlas_h;
    unsigned char *scratch;
    size_t scratch_cap;
    FONScontext *fs;
    int font;
    int screen_w, screen_h; // set right before fonsDrawText so renderDraw can build the ortho proj
    bool ready;
} g_text;

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

static int fons_render_create(void *uptr, int width, int height) {
    (void)uptr;
    g_text.atlas_w = width;
    g_text.atlas_h = height;
    glGenTextures(1, &g_text.atlas_tex);
    glBindTexture(GL_TEXTURE_2D, g_text.atlas_tex);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_ALPHA, width, height, 0, GL_ALPHA, GL_UNSIGNED_BYTE, NULL);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
    glBindTexture(GL_TEXTURE_2D, 0);
    return 1;
}

static void fons_render_update(void *uptr, int *rect, const unsigned char *data) {
    (void)uptr;
    int x = rect[0], y = rect[1];
    int w = rect[2] - rect[0], h = rect[3] - rect[1];
    if (w <= 0 || h <= 0) return;

    size_t needed = (size_t)w * (size_t)h;
    if (g_text.scratch_cap < needed) {
        g_text.scratch = (unsigned char *)realloc(g_text.scratch, needed);
        g_text.scratch_cap = needed;
    }
    for (int row = 0; row < h; row++) {
        const unsigned char *src = data + (size_t)(y + row) * (size_t)g_text.atlas_w + (size_t)x;
        memcpy(g_text.scratch + (size_t)row * (size_t)w, src, (size_t)w);
    }

    glBindTexture(GL_TEXTURE_2D, g_text.atlas_tex);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexSubImage2D(GL_TEXTURE_2D, 0, x, y, w, h, GL_ALPHA, GL_UNSIGNED_BYTE, g_text.scratch);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
    glBindTexture(GL_TEXTURE_2D, 0);
}

static void fons_render_draw(void *uptr, const float *verts, const float *tcoords, const unsigned int *colors, int nverts) {
    (void)uptr;
    if (nverts <= 0) return;
    if (nverts > TEXT_BATCH_CAP) nverts = TEXT_BATCH_CAP; // fontstash never actually exceeds FONS_VERTEX_COUNT per flush

    static TextVertex buf[TEXT_BATCH_CAP];
    for (int i = 0; i < nverts; i++) {
        unsigned int c = colors[i];
        buf[i].x = verts[i * 2 + 0];
        buf[i].y = verts[i * 2 + 1];
        buf[i].u = tcoords[i * 2 + 0];
        buf[i].v = tcoords[i * 2 + 1];
        buf[i].r = (float)(c & 0xFFu) / 255.0f;
        buf[i].g = (float)((c >> 8) & 0xFFu) / 255.0f;
        buf[i].b = (float)((c >> 16) & 0xFFu) / 255.0f;
        buf[i].a = (float)((c >> 24) & 0xFFu) / 255.0f;
    }

    glDisable(GL_DEPTH_TEST);
    glUseProgram(g_text.program);
    Mat4 proj = mat4_ortho(0.0f, (float)g_text.screen_w, (float)g_text.screen_h, 0.0f);
    glUniformMatrix4fv(g_text.u_mvp, 1, GL_FALSE, proj.m);
    glUniform1i(g_text.u_tex, 0);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, g_text.atlas_tex);

    glBindBuffer(GL_ARRAY_BUFFER, g_text.vbo);
    glBufferData(GL_ARRAY_BUFFER, (long)(nverts * (int)sizeof(TextVertex)), buf, GL_DYNAMIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(TextVertex), (const void *)offsetof(TextVertex, x));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(TextVertex), (const void *)offsetof(TextVertex, u));
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 4, GL_FLOAT, GL_FALSE, sizeof(TextVertex), (const void *)offsetof(TextVertex, r));
    glDrawArrays(GL_TRIANGLES, 0, nverts);
    glDisableVertexAttribArray(0);
    glDisableVertexAttribArray(1);
    glDisableVertexAttribArray(2);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindTexture(GL_TEXTURE_2D, 0);
}

static void fons_render_delete(void *uptr) {
    (void)uptr;
    if (g_text.scratch) {
        free(g_text.scratch);
        g_text.scratch = NULL;
        g_text.scratch_cap = 0;
    }
}

bool text_backend_init(void) {
    unsigned int vs = compile_shader(GL_VERTEX_SHADER, (const char *)SHADER_TEXT_VERT_SRC, (int)SHADER_TEXT_VERT_SRC_len);
    unsigned int fs = compile_shader(GL_FRAGMENT_SHADER, (const char *)SHADER_TEXT_FRAG_SRC, (int)SHADER_TEXT_FRAG_SRC_len);
    if (!vs || !fs) return false;

    unsigned int program = glCreateProgram();
    glAttachShader(program, vs);
    glAttachShader(program, fs);
    glBindAttribLocation(program, 0, "aPosition");
    glBindAttribLocation(program, 1, "aUV");
    glBindAttribLocation(program, 2, "aColor");
    glLinkProgram(program);

    int linked = 0;
    glGetProgramiv(program, GL_LINK_STATUS, &linked);
    glDeleteShader(vs);
    glDeleteShader(fs);
    if (!linked) {
        glDeleteProgram(program);
        return false;
    }

    memset(&g_text, 0, sizeof(g_text));
    g_text.program = program;
    g_text.u_mvp = glGetUniformLocation(program, "uMVP");
    g_text.u_tex = glGetUniformLocation(program, "uTex");
    glGenBuffers(1, &g_text.vbo);

    FONSparams params;
    memset(&params, 0, sizeof(params));
    params.width = 512;
    params.height = 512;
    params.flags = FONS_ZERO_TOPLEFT;
    params.renderCreate = fons_render_create;
    params.renderUpdate = fons_render_update;
    params.renderDraw = fons_render_draw;
    params.renderDelete = fons_render_delete;
    params.userPtr = NULL;

    g_text.fs = fonsCreateInternal(&params);
    if (!g_text.fs) {
        glDeleteProgram(program);
        return false;
    }

    g_text.font = fonsAddFontMem(g_text.fs, "default", (unsigned char *)GAME_FONT_ROBOTO_REGULAR,
                                  (int)GAME_FONT_ROBOTO_REGULAR_len, 0);
    if (g_text.font == FONS_INVALID) {
        fonsDeleteInternal(g_text.fs);
        glDeleteProgram(program);
        return false;
    }

    g_text.ready = true;
    return true;
}

void text_backend_shutdown(void) {
    if (!g_text.ready) return;
    fonsDeleteInternal(g_text.fs);
    if (g_text.atlas_tex) glDeleteTextures(1, &g_text.atlas_tex);
    if (g_text.vbo) glDeleteBuffers(1, &g_text.vbo);
    if (g_text.program) glDeleteProgram(g_text.program);
    memset(&g_text, 0, sizeof(g_text));
}

void text_draw(float x, float y, float size, float r, float g, float b, float a,
               const char *str, int screen_w, int screen_h) {
    if (!g_text.ready || !str) return;
    g_text.screen_w = screen_w;
    g_text.screen_h = screen_h;
    unsigned int color = ((unsigned int)(r * 255.0f)) |
                          ((unsigned int)(g * 255.0f) << 8) |
                          ((unsigned int)(b * 255.0f) << 16) |
                          ((unsigned int)(a * 255.0f) << 24);
    fonsSetSize(g_text.fs, size);
    fonsSetFont(g_text.fs, g_text.font);
    fonsSetColor(g_text.fs, color);
    fonsSetAlign(g_text.fs, FONS_ALIGN_LEFT | FONS_ALIGN_TOP);
    fonsDrawText(g_text.fs, x, y, str, NULL);
}

float text_width(const char *str, float size) {
    if (!g_text.ready || !str) return 0.0f;
    fonsSetSize(g_text.fs, size);
    fonsSetFont(g_text.fs, g_text.font);
    return fonsTextBounds(g_text.fs, 0.0f, 0.0f, str, NULL, NULL);
}

float text_line_height(float size) {
    if (!g_text.ready) return size;
    fonsSetSize(g_text.fs, size);
    fonsSetFont(g_text.fs, g_text.font);
    float ascender, descender, lineh;
    fonsVertMetrics(g_text.fs, &ascender, &descender, &lineh);
    return lineh;
}
