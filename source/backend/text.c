// HUD/menu text: Tamzen5x9 regular+bold, baked at *build* time from BDF
// sources into plain glyph-bitmap tables (tools/fonts.c -> include/bitfont.h
// + the generated gecnd/tamzen_5x9_*.h headers — see CMakeLists.txt). This
// file's only job at runtime is: blit those precomputed glyphs into one GL
// alpha atlas once at init, then draw textured quads from a UV table. No
// font-format parsing here at all (no fontstash, no stb_truetype, no BDF).
#include "text.h"
#include "glad/gl.h"
#include "math4.h"

#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#include "gecnd/tamzen_5x9_regular.h"
#include "gecnd/tamzen_5x9_bold.h"
#include "gecnd/shader_text_vert.h"
#include "gecnd/shader_text_frag.h"

typedef struct {
    float x, y, u, v, r, g, b, a;
} TextVertex;

#define TEXT_BATCH_CAP 1024 // vertices per text_draw() flush (6 per glyph)

// Atlas layout: 16 columns x 6 rows (96 glyphs) per face, the two faces
// stacked vertically, each cell padded by 1px to avoid bilinear/mip bleed.
#define GLYPH_COLS 16
#define GLYPH_ROWS ((BITFONT_GLYPH_COUNT + GLYPH_COLS - 1) / GLYPH_COLS)
#define CELL_PAD 1

typedef struct {
    float u0, v0, u1, v1;
    int advance;
} GlyphUV;

static struct {
    unsigned int program;
    int u_mvp;
    int u_tex;
    unsigned int vbo;
    unsigned int atlas_tex;
    int cell_w, cell_h; // native (unscaled) glyph cell size, from the regular face
    GlyphUV glyphs[2][BITFONT_GLYPH_COUNT]; // indexed by TextFont
    TextFont current_font;
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

// Blits one already-decoded glyph bitmap (BitFontGlyph + its *_PIXELS slice)
// into its atlas cell at (cell_x, cell_y), placing ink via bearing_x/y
// against `ascent` — same baseline math BDF/OTB bitmap fonts always use.
static void blit_glyph(unsigned char *atlas, int atlas_w, int atlas_h,
                       const BitFontGlyph *g, const unsigned char *pixels, int ascent,
                       int cell_x, int cell_y, int cell_w, int cell_h) {
    int top = ascent - g->bearing_y;
    int left = g->bearing_x;
    for (int y = 0; y < g->height; y++) {
        int yy = top + y;
        if (yy < 0 || yy >= cell_h) continue;
        for (int x = 0; x < g->width; x++) {
            int xx = left + x;
            if (xx < 0 || xx >= cell_w) continue;
            int ax = cell_x + xx, ay = cell_y + yy;
            if (ax < 0 || ax >= atlas_w || ay < 0 || ay >= atlas_h) continue;
            atlas[ay * atlas_w + ax] = pixels[y * g->width + x];
        }
    }
}

static void build_atlas(void) {
    g_text.cell_w = TAMZEN_5X9_REGULAR_CELL_W;
    g_text.cell_h = TAMZEN_5X9_REGULAR_CELL_H;
    int cell_w = g_text.cell_w, cell_h = g_text.cell_h;
    int padded_w = cell_w + CELL_PAD * 2;
    int padded_h = cell_h + CELL_PAD * 2;
    int atlas_w = GLYPH_COLS * padded_w;
    int atlas_h = GLYPH_ROWS * padded_h * 2; // *2: regular stacked over bold

    unsigned char *atlas = (unsigned char *)calloc((size_t)atlas_w * (size_t)atlas_h, 1);

    for (int face = 0; face < 2; face++) {
        const BitFontGlyph *glyphs = (face == TEXT_FONT_REGULAR) ? TAMZEN_5X9_REGULAR_GLYPHS : TAMZEN_5X9_BOLD_GLYPHS;
        const unsigned char *pixels = (face == TEXT_FONT_REGULAR) ? TAMZEN_5X9_REGULAR_PIXELS : TAMZEN_5X9_BOLD_PIXELS;
        int ascent = (face == TEXT_FONT_REGULAR) ? TAMZEN_5X9_REGULAR_ASCENT : TAMZEN_5X9_BOLD_ASCENT;

        for (int i = 0; i < BITFONT_GLYPH_COUNT; i++) {
            int col = i % GLYPH_COLS;
            int row = i / GLYPH_COLS;
            int cell_x = col * padded_w + CELL_PAD;
            int cell_y = (face * GLYPH_ROWS + row) * padded_h + CELL_PAD;

            const BitFontGlyph *g = &glyphs[i];
            if (g->width > 0 && g->height > 0) {
                blit_glyph(atlas, atlas_w, atlas_h, g, pixels + g->pixel_offset, ascent,
                           cell_x, cell_y, cell_w, cell_h);
            }

            GlyphUV *uv = &g_text.glyphs[face][i];
            uv->u0 = (float)cell_x / (float)atlas_w;
            uv->v0 = (float)cell_y / (float)atlas_h;
            uv->u1 = (float)(cell_x + cell_w) / (float)atlas_w;
            uv->v1 = (float)(cell_y + cell_h) / (float)atlas_h;
            uv->advance = g->advance;
        }
    }

    glGenTextures(1, &g_text.atlas_tex);
    glBindTexture(GL_TEXTURE_2D, g_text.atlas_tex);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST); // keep the pixel-font look when scaled
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_ALPHA, atlas_w, atlas_h, 0, GL_ALPHA, GL_UNSIGNED_BYTE, atlas);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
    glBindTexture(GL_TEXTURE_2D, 0);

    free(atlas);
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
    g_text.current_font = TEXT_FONT_REGULAR;

    build_atlas();

    g_text.ready = true;
    return true;
}

void text_backend_shutdown(void) {
    if (!g_text.ready) return;
    if (g_text.atlas_tex) glDeleteTextures(1, &g_text.atlas_tex);
    if (g_text.vbo) glDeleteBuffers(1, &g_text.vbo);
    if (g_text.program) glDeleteProgram(g_text.program);
    memset(&g_text, 0, sizeof(g_text));
}

void text_set_font(TextFont font) {
    g_text.current_font = font;
}

void text_draw(float x, float y, float size, float r, float g, float b, float a,
               const char *str, int screen_w, int screen_h) {
    if (!g_text.ready || !str || !*str) return;

    float scale = size / (float)g_text.cell_h;
    const GlyphUV *table = g_text.glyphs[g_text.current_font];

    static TextVertex buf[TEXT_BATCH_CAP];
    int nverts = 0;
    float cursor_x = x, cursor_y = y;

    for (const unsigned char *c = (const unsigned char *)str; *c && nverts + 6 <= TEXT_BATCH_CAP; c++) {
        if (*c == '\n') {
            cursor_x = x;
            cursor_y += (float)g_text.cell_h * scale;
            continue;
        }
        if (*c < BITFONT_FIRST_CODEPOINT || *c >= BITFONT_FIRST_CODEPOINT + BITFONT_GLYPH_COUNT) {
            cursor_x += (float)g_text.cell_w * scale;
            continue;
        }

        const GlyphUV *uv = &table[*c - BITFONT_FIRST_CODEPOINT];
        float x0 = cursor_x, y0 = cursor_y;
        float x1 = x0 + (float)g_text.cell_w * scale;
        float y1 = y0 + (float)g_text.cell_h * scale;

        TextVertex quad[6] = {
            {x0, y0, uv->u0, uv->v0, r, g, b, a},
            {x1, y0, uv->u1, uv->v0, r, g, b, a},
            {x1, y1, uv->u1, uv->v1, r, g, b, a},
            {x0, y0, uv->u0, uv->v0, r, g, b, a},
            {x1, y1, uv->u1, uv->v1, r, g, b, a},
            {x0, y1, uv->u0, uv->v1, r, g, b, a},
        };
        memcpy(&buf[nverts], quad, sizeof(quad));
        nverts += 6;

        cursor_x += (float)uv->advance * scale;
    }
    if (nverts <= 0) return;

    glDisable(GL_DEPTH_TEST); // blending is already enabled per-frame (see gl.c)
    glUseProgram(g_text.program);
    Mat4 proj = mat4_ortho(0.0f, (float)screen_w, (float)screen_h, 0.0f);
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

float text_width(const char *str, float size) {
    if (!g_text.ready || !str) return 0.0f;
    float scale = size / (float)g_text.cell_h;
    const GlyphUV *table = g_text.glyphs[g_text.current_font];

    float width = 0.0f, max_width = 0.0f;
    for (const unsigned char *c = (const unsigned char *)str; *c; c++) {
        if (*c == '\n') { if (width > max_width) max_width = width; width = 0.0f; continue; }
        if (*c < BITFONT_FIRST_CODEPOINT || *c >= BITFONT_FIRST_CODEPOINT + BITFONT_GLYPH_COUNT) {
            width += (float)g_text.cell_w * scale;
            continue;
        }
        width += (float)table[*c - BITFONT_FIRST_CODEPOINT].advance * scale;
    }
    return width > max_width ? width : max_width;
}

float text_line_height(float size) {
    if (!g_text.ready) return size;
    return (float)g_text.cell_h * (size / (float)g_text.cell_h); // == size; symbolic for clarity
}
