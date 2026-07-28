// Shared glyph struct for the headers tools/fonts.c generates from BDF font
// sources at build time (see CMakeLists.txt) — one struct definition here so
// multiple generated font headers (regular/bold) can be included together
// without redefining it.
#ifndef STARTSHIPPER_BITFONT_H
#define STARTSHIPPER_BITFONT_H

// First codepoint and glyph count every generated *_GLYPHS table covers:
// ASCII 0x20..0x7F (space through DEL), indexed as codepoint - 0x20.
#define BITFONT_FIRST_CODEPOINT 0x20
#define BITFONT_GLYPH_COUNT 96

typedef struct {
    int width, height;        // ink bitmap size in pixels (0x0 for space/missing glyphs)
    int bearing_x, bearing_y; // bitmap origin relative to the pen position:
                                // bearing_x = left edge, bearing_y = distance
                                // from baseline up to the bitmap's top row
    int advance;               // horizontal pen advance in pixels
    int pixel_offset;          // byte offset into the font's *_PIXELS array
                                // (width*height bytes, one per pixel, 0 or 255)
} BitFontGlyph;

#endif
