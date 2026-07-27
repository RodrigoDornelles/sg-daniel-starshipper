// HUD/menu text via fontstash (source/backend/text.c) — a real TTF (Roboto,
// embedded at build time, see CMakeLists.txt) rasterized into an alpha
// texture atlas, not a hand-rolled bitmap font.
#ifndef STARTSHIPPER_TEXT_H
#define STARTSHIPPER_TEXT_H

#include <stdbool.h>

// Call once a GL context is live (alongside gl_backend_init), and tear down
// before the context goes away (alongside gl_backend_shutdown).
bool text_backend_init(void);
void text_backend_shutdown(void);

// Pixel-space coordinates, origin top-left, depth test disabled, alpha
// blended — same convention as gl_draw_quad2d/gl_draw_batch2d.
void  text_draw(float x, float y, float size, float r, float g, float b, float a,
                 const char *str, int screen_w, int screen_h);
float text_width(const char *str, float size);
float text_line_height(float size);

#endif
