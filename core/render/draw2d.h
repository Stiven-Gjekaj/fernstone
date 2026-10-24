// draw2d.h: textured and colored rectangles and text in screen pixels.
// The calls fill a batch. The batch draws when the texture changes or at
// the end of the frame.
#ifndef FERN_DRAW2D_H
#define FERN_DRAW2D_H

#include <stdint.h>
#include "gl.h"

typedef struct FernDraw2D FernDraw2D;

FernDraw2D *fern_d2_new(void);
// Start a frame of w x h pixels. The origin is the top left.
void fern_d2_begin(FernDraw2D *d, int w, int h);
// A rectangle with a part of a texture: u0, v0, u1, v1 in 0 to 1.
// tex 0 draws a plain color.
void fern_d2_image(FernDraw2D *d, GLuint tex, float x, float y, float w, float h, float u0, float v0, float u1,
                   float v1, uint32_t rgba);
void fern_d2_rect(FernDraw2D *d, float x, float y, float w, float h, uint32_t rgba);
void fern_d2_flush(FernDraw2D *d);

// The font: the 16 x 16 grid of font/default.png from the jar, or the
// built-in font if rgba is NULL.
void fern_d2_set_font(FernDraw2D *d, const uint8_t *rgba, int w, int h);
// Draw text at a scale. The section sign (U+00A7) and a hex digit set the
// color, as in Beta. Give the width in pixels.
float fern_d2_text(FernDraw2D *d, const char *utf8, float x, float y, float scale, uint32_t rgba, int shadow);
float fern_d2_text_width(FernDraw2D *d, const char *utf8, float scale);

// Turn and scale each later rectangle around a point (in pixels), for
// example for a splash text. fern_d2_reset_transform stops it.
void fern_d2_transform(FernDraw2D *d, float ox, float oy, float angle_deg, float scale);
void fern_d2_reset_transform(FernDraw2D *d);
// The 5 x 7 rows of a built-in glyph (bit 4 is the left column). Give 0 for
// a character that the built-in font does not have.
int fern_d2_glyph(char c, uint8_t rows[7]);

#define FERN_RGBA(r, g, b, a) ((uint32_t)(r) << 24 | (uint32_t)(g) << 16 | (uint32_t)(b) << 8 | (uint32_t)(a))

#endif
