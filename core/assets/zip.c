#include "zip.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <zlib.h>

struct FernZip {
    FILE *f;
    uint8_t *dir;       // The central directory.
    size_t dir_len;
    int entries;
};

static uint16_t u16(const uint8_t *p) { return (uint16_t)(p[0] | p[1] << 8); }
static uint32_t u32(const uint8_t *p) { return (uint32_t)p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24; }

FernZip *fern_zip_open(const char *path) {
    FernZip *z;
    uint8_t tail[65557];
    long size, start;
    size_t n;
    int i;
    FILE *f = fopen(path, "rb");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END);
    size = ftell(f);
    start = size > (long)sizeof(tail) ? size - (long)sizeof(tail) : 0;
    fseek(f, start, SEEK_SET);
    n = fread(tail, 1, (size_t)(size - start), f);
    // Find the end of the central directory from the back.
    for (i = (int)n - 22; i >= 0; i--) {
        if (u32(tail + i) == 0x06054b50) {
            uint32_t len = u32(tail + i + 12), off = u32(tail + i + 16);
            z = calloc(1, sizeof(*z));
            z->f = f;
            z->entries = u16(tail + i + 10);
            z->dir = malloc(len);
            z->dir_len = len;
            fseek(f, (long)off, SEEK_SET);
            if (fread(z->dir, 1, len, f) != len) break;
            return z;
        }
    }
    fclose(f);
    return NULL;
}

uint8_t *fern_zip_read(FernZip *z, const char *name, size_t *len) {
    size_t pos = 0, nlen = strlen(name);
    int i;
    for (i = 0; i < z->entries && pos + 46 <= z->dir_len; i++) {
        const uint8_t *e = z->dir + pos;
        uint16_t method = u16(e + 10), fn = u16(e + 28), extra = u16(e + 30), comment = u16(e + 32);
        uint32_t csize = u32(e + 20), usize = u32(e + 24), off = u32(e + 42);
        if (u32(e) != 0x02014b50) return NULL;
        if (fn == nlen && memcmp(e + 46, name, nlen) == 0) {
            uint8_t local[30], *src, *out;
            fseek(z->f, (long)off, SEEK_SET);
            if (fread(local, 1, 30, z->f) != 30 || u32(local) != 0x04034b50) return NULL;
            fseek(z->f, (long)(off + 30 + u16(local + 26) + u16(local + 28)), SEEK_SET);
            src = malloc(csize ? csize : 1);
            if (fread(src, 1, csize, z->f) != csize) {
                free(src);
                return NULL;
            }
            if (method == 0) {
                *len = csize;
                return src;
            }
            out = malloc(usize ? usize : 1);
            {
                z_stream s;
                int rc;
                memset(&s, 0, sizeof(s));
                inflateInit2(&s, -MAX_WBITS);
                s.next_in = src;
                s.avail_in = csize;
                s.next_out = out;
                s.avail_out = usize;
                rc = inflate(&s, Z_FINISH);
                inflateEnd(&s);
                free(src);
                if (rc != Z_STREAM_END) {
                    free(out);
                    return NULL;
                }
            }
            *len = usize;
            return out;
        }
        pos += 46 + fn + extra + comment;
    }
    return NULL;
}

void fern_zip_close(FernZip *z) {
    if (!z) return;
    fclose(z->f);
    free(z->dir);
    free(z);
}
