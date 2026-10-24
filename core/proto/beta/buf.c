#include "buf.h"
#include <stdlib.h>
#include <string.h>

void fern_reader_init(FernReader *r, const uint8_t *data, size_t len) {
    r->data = data;
    r->len = len;
    r->pos = 0;
    r->short_read = 0;
    r->bad = 0;
}

int fern_reader_ok(const FernReader *r) {
    return !r->short_read && !r->bad;
}

static const uint8_t *take(FernReader *r, size_t n) {
    const uint8_t *p;
    if (r->short_read || r->bad || r->len - r->pos < n) {
        r->short_read = 1;
        return NULL;
    }
    p = r->data + r->pos;
    r->pos += n;
    return p;
}

uint8_t fern_read_u8(FernReader *r) {
    const uint8_t *p = take(r, 1);
    return p ? p[0] : 0;
}

int8_t fern_read_i8(FernReader *r) {
    return (int8_t)fern_read_u8(r);
}

uint16_t fern_read_u16(FernReader *r) {
    const uint8_t *p = take(r, 2);
    return p ? (uint16_t)(p[0] << 8 | p[1]) : 0;
}

int16_t fern_read_i16(FernReader *r) {
    return (int16_t)fern_read_u16(r);
}

int32_t fern_read_i32(FernReader *r) {
    const uint8_t *p = take(r, 4);
    if (!p) return 0;
    return (int32_t)((uint32_t)p[0] << 24 | (uint32_t)p[1] << 16 | (uint32_t)p[2] << 8 | p[3]);
}

int64_t fern_read_i64(FernReader *r) {
    uint64_t hi = (uint32_t)fern_read_i32(r);
    uint64_t lo = (uint32_t)fern_read_i32(r);
    return (int64_t)(hi << 32 | lo);
}

float fern_read_f32(FernReader *r) {
    uint32_t bits = (uint32_t)fern_read_i32(r);
    float v;
    memcpy(&v, &bits, 4);
    return v;
}

double fern_read_f64(FernReader *r) {
    uint64_t bits = (uint64_t)fern_read_i64(r);
    double v;
    memcpy(&v, &bits, 8);
    return v;
}

const uint8_t *fern_read_bytes(FernReader *r, size_t n) {
    return take(r, n);
}

// Append one code point to out as UTF-8. Keep space for the NUL.
static void put_utf8(char *out, size_t cap, size_t *n, uint32_t c) {
    char tmp[4];
    size_t k, len;
    if (c < 0x80) {
        tmp[0] = (char)c;
        len = 1;
    } else if (c < 0x800) {
        tmp[0] = (char)(0xC0 | c >> 6);
        tmp[1] = (char)(0x80 | (c & 0x3F));
        len = 2;
    } else if (c < 0x10000) {
        tmp[0] = (char)(0xE0 | c >> 12);
        tmp[1] = (char)(0x80 | (c >> 6 & 0x3F));
        tmp[2] = (char)(0x80 | (c & 0x3F));
        len = 3;
    } else {
        tmp[0] = (char)(0xF0 | c >> 18);
        tmp[1] = (char)(0x80 | (c >> 12 & 0x3F));
        tmp[2] = (char)(0x80 | (c >> 6 & 0x3F));
        tmp[3] = (char)(0x80 | (c & 0x3F));
        len = 4;
    }
    if (*n + len >= cap) return;
    for (k = 0; k < len; k++) out[(*n)++] = tmp[k];
}

void fern_read_str16(FernReader *r, char *out, size_t cap) {
    int16_t count = fern_read_i16(r);
    size_t n = 0;
    int i;
    if (cap) out[0] = 0;
    if (count < 0) {
        r->bad = 1;
        return;
    }
    for (i = 0; i < count; i++) {
        uint32_t c = fern_read_u16(r);
        if (c >= 0xD800 && c < 0xDC00 && i + 1 < count) {
            uint32_t lo = fern_read_u16(r);
            i++;
            c = 0x10000 + ((c - 0xD800) << 10) + (lo - 0xDC00);
        }
        if (cap) put_utf8(out, cap, &n, c);
    }
    if (cap) out[n] = 0;
}

void fern_read_str8(FernReader *r, char *out, size_t cap) {
    uint16_t count = fern_read_u16(r);
    const uint8_t *p = take(r, count);
    size_t n = 0, i;
    if (cap) out[0] = 0;
    if (!p || !cap) return;
    for (i = 0; i < count && n + 1 < cap; i++) {
        // Modified UTF-8 writes NUL as C0 80. Keep it out of a C string.
        if (p[i] == 0xC0 && i + 1 < count && p[i + 1] == 0x80) {
            i++;
            continue;
        }
        out[n++] = (char)p[i];
    }
    out[n] = 0;
}

void fern_writer_init(FernWriter *w) {
    w->data = NULL;
    w->len = 0;
    w->cap = 0;
}

void fern_writer_free(FernWriter *w) {
    free(w->data);
    fern_writer_init(w);
}

void fern_writer_reset(FernWriter *w) {
    w->len = 0;
}

void fern_write_bytes(FernWriter *w, const void *data, size_t n) {
    if (w->len + n > w->cap) {
        size_t cap = w->cap ? w->cap * 2 : 256;
        while (cap < w->len + n) cap *= 2;
        w->data = realloc(w->data, cap);
        w->cap = cap;
    }
    memcpy(w->data + w->len, data, n);
    w->len += n;
}

void fern_write_u8(FernWriter *w, uint8_t v) {
    fern_write_bytes(w, &v, 1);
}

void fern_write_i16(FernWriter *w, int16_t v) {
    uint8_t b[2] = {(uint8_t)((uint16_t)v >> 8), (uint8_t)v};
    fern_write_bytes(w, b, 2);
}

void fern_write_i32(FernWriter *w, int32_t v) {
    uint32_t u = (uint32_t)v;
    uint8_t b[4] = {(uint8_t)(u >> 24), (uint8_t)(u >> 16), (uint8_t)(u >> 8), (uint8_t)u};
    fern_write_bytes(w, b, 4);
}

void fern_write_i64(FernWriter *w, int64_t v) {
    fern_write_i32(w, (int32_t)((uint64_t)v >> 32));
    fern_write_i32(w, (int32_t)(uint32_t)v);
}

void fern_write_f32(FernWriter *w, float v) {
    uint32_t bits;
    memcpy(&bits, &v, 4);
    fern_write_i32(w, (int32_t)bits);
}

void fern_write_f64(FernWriter *w, double v) {
    uint64_t bits;
    memcpy(&bits, &v, 8);
    fern_write_i64(w, (int64_t)bits);
}

// Read one code point from UTF-8. Give U+FFFD for a bad byte.
static uint32_t next_utf8(const unsigned char **s) {
    const unsigned char *p = *s;
    uint32_t c = *p++;
    int extra = 0;
    if (c >= 0xF0) { c &= 0x07; extra = 3; }
    else if (c >= 0xE0) { c &= 0x0F; extra = 2; }
    else if (c >= 0xC0) { c &= 0x1F; extra = 1; }
    else if (c >= 0x80) { *s = p; return 0xFFFD; }
    while (extra-- > 0) {
        if ((*p & 0xC0) != 0x80) { *s = p; return 0xFFFD; }
        c = c << 6 | (*p++ & 0x3F);
    }
    *s = p;
    return c;
}

void fern_write_str16(FernWriter *w, const char *utf8) {
    const unsigned char *s = (const unsigned char *)utf8;
    size_t at = w->len;
    int16_t count = 0;
    fern_write_i16(w, 0);
    while (*s) {
        uint32_t c = next_utf8(&s);
        if (c >= 0x10000) {
            c -= 0x10000;
            fern_write_i16(w, (int16_t)(0xD800 + (c >> 10)));
            fern_write_i16(w, (int16_t)(0xDC00 + (c & 0x3FF)));
            count += 2;
        } else {
            fern_write_i16(w, (int16_t)c);
            count++;
        }
    }
    w->data[at] = (uint8_t)((uint16_t)count >> 8);
    w->data[at + 1] = (uint8_t)count;
}

void fern_write_str8(FernWriter *w, const char *utf8) {
    size_t n = strlen(utf8);
    if (n > 0xFFFF) n = 0xFFFF;
    fern_write_i16(w, (int16_t)n);
    fern_write_bytes(w, utf8, n);
}
