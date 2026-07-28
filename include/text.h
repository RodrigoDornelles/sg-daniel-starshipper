// HUD/menu text via a hand-rolled OTB bitmap rasterizer (source/backend/text.c)
// — Tamzen5x9 regular+bold, embedded at build time (see CMakeLists.txt), no
// fontstash/stb_truetype outline rendering involved.
//
// NOTE: the OTB parsing + glyph-quad rendering itself isn't implemented yet —
// text_draw/text_width/text_line_height are currently no-ops so the rest of
// the HUD code compiles and runs against a stable API while the rasterizer
// is built out.
#ifndef STARTSHIPPER_TEXT_H
#define STARTSHIPPER_TEXT_H

#include <stdbool.h>

typedef enum {
    TEXT_FONT_REGULAR,
    TEXT_FONT_BOLD
} TextFont;

// Call once a GL context is live (alongside gl_backend_init), and tear down
// before the context goes away (alongside gl_backend_shutdown).
bool text_backend_init(void);
void text_backend_shutdown(void);

// Selects which embedded Tamzen5x9 face subsequent text_draw/text_width/
// text_line_height calls use. Defaults to TEXT_FONT_REGULAR.
void text_set_font(TextFont font);

// Pixel-space coordinates, origin top-left, depth test disabled, alpha
// blended — same convention as gl_draw_quad2d/gl_draw_batch2d.
void  text_draw(float x, float y, float size, float r, float g, float b, float a,
                 const char *str, int screen_w, int screen_h);
float text_width(const char *str, float size);
float text_line_height(float size);

#endif
