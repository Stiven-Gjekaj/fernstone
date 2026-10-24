// png.h: decode a PNG file to RGBA pixels. It reads 8-bit gray, RGB,
// palette (1 to 8 bits), gray with alpha, and RGBA images with no interlace.
#ifndef FERN_PNG_H
#define FERN_PNG_H

#include <stddef.h>
#include <stdint.h>

// Give a new buffer of w * h * 4 bytes, or NULL on an error.
uint8_t *fern_png_decode(const uint8_t *data, size_t len, int *w, int *h);

#endif
