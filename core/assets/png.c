#include "png.h"
#include <stdlib.h>
#include <string.h>
#include <zlib.h>

static uint32_t be32(const uint8_t *p) {
    return (uint32_t)p[0] << 24 | (uint32_t)p[1] << 16 | (uint32_t)p[2] << 8 | p[3];
}

static int paeth(int a, int b, int c) {
    int p = a + b - c, pa = abs(p - a), pb = abs(p - b), pc = abs(p - c);
    if (pa <= pb && pa <= pc) return a;
    return pb <= pc ? b : c;
}

uint8_t *fern_png_decode(const uint8_t *data, size_t len, int *w, int *h) {
    static const uint8_t sig[8] = {137, 80, 78, 71, 13, 10, 26, 10};
    uint8_t palette[256][4];
    uint8_t *idat = NULL, *raw = NULL, *out = NULL;
    size_t idat_len = 0, pos = 8;
    int width = 0, height = 0, depth = 0, type = 0, channels, bpp, stride, x, y, i;
    uLongf raw_len;
    memset(palette, 255, sizeof(palette));
    if (len < 8 || memcmp(data, sig, 8) != 0) return NULL;
    while (pos + 12 <= len) {
        uint32_t clen = be32(data + pos);
        const uint8_t *ctype = data + pos + 4, *c = data + pos + 8;
        if (pos + 12 + clen > len) break;
        if (!memcmp(ctype, "IHDR", 4)) {
            width = (int)be32(c);
            height = (int)be32(c + 4);
            depth = c[8];
            type = c[9];
            if (c[12] != 0) goto fail; // interlace
        } else if (!memcmp(ctype, "PLTE", 4)) {
            for (i = 0; i < (int)clen / 3 && i < 256; i++) {
                palette[i][0] = c[i * 3];
                palette[i][1] = c[i * 3 + 1];
                palette[i][2] = c[i * 3 + 2];
            }
        } else if (!memcmp(ctype, "tRNS", 4) && type == 3) {
            for (i = 0; i < (int)clen && i < 256; i++) palette[i][3] = c[i];
        } else if (!memcmp(ctype, "IDAT", 4)) {
            idat = realloc(idat, idat_len + clen);
            memcpy(idat + idat_len, c, clen);
            idat_len += clen;
        } else if (!memcmp(ctype, "IEND", 4)) {
            break;
        }
        pos += 12 + clen;
    }
    if (width <= 0 || height <= 0 || width > 4096 || height > 4096 || !idat) goto fail;
    channels = type == 0 ? 1 : type == 2 ? 3 : type == 3 ? 1 : type == 4 ? 2 : type == 6 ? 4 : 0;
    if (!channels || (type != 3 && depth != 8) || (type == 3 && depth > 8)) goto fail;
    bpp = (channels * depth + 7) / 8;
    stride = (width * channels * depth + 7) / 8;
    raw_len = (uLongf)(stride + 1) * (uLongf)height;
    raw = malloc(raw_len);
    if (uncompress(raw, &raw_len, idat, (uLong)idat_len) != Z_OK || raw_len != (uLongf)(stride + 1) * (uLongf)height)
        goto fail;
    // Undo the filters, row by row, in place.
    for (y = 0; y < height; y++) {
        uint8_t *row = raw + (size_t)y * (stride + 1);
        uint8_t *line = row + 1;
        uint8_t *prev = y ? raw + (size_t)(y - 1) * (stride + 1) + 1 : NULL;
        for (x = 0; x < stride; x++) {
            int a = x >= bpp ? line[x - bpp] : 0, b = prev ? prev[x] : 0, c = prev && x >= bpp ? prev[x - bpp] : 0;
            switch (row[0]) {
            case 0: break;
            case 1: line[x] = (uint8_t)(line[x] + a); break;
            case 2: line[x] = (uint8_t)(line[x] + b); break;
            case 3: line[x] = (uint8_t)(line[x] + ((a + b) >> 1)); break;
            case 4: line[x] = (uint8_t)(line[x] + paeth(a, b, c)); break;
            default: goto fail;
            }
        }
    }
    out = malloc((size_t)width * height * 4);
    for (y = 0; y < height; y++) {
        const uint8_t *line = raw + (size_t)y * (stride + 1) + 1;
        uint8_t *o = out + (size_t)y * width * 4;
        for (x = 0; x < width; x++, o += 4) {
            switch (type) {
            case 0: o[0] = o[1] = o[2] = line[x]; o[3] = 255; break;
            case 2: o[0] = line[x * 3]; o[1] = line[x * 3 + 1]; o[2] = line[x * 3 + 2]; o[3] = 255; break;
            case 4: o[0] = o[1] = o[2] = line[x * 2]; o[3] = line[x * 2 + 1]; break;
            case 6: memcpy(o, line + x * 4, 4); break;
            default: {
                int bit = x * depth, idx = (line[bit >> 3] >> (8 - depth - (bit & 7))) & ((1 << depth) - 1);
                memcpy(o, palette[idx], 4);
            }
            }
        }
    }
    free(idat);
    free(raw);
    *w = width;
    *h = height;
    return out;
fail:
    free(idat);
    free(raw);
    free(out);
    return NULL;
}
