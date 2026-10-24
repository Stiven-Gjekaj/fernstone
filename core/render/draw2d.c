#include "draw2d.h"
#include <stdlib.h>
#include <string.h>
#include "gfx.h"
#include "mat.h"

#define MAX_QUADS 4096

typedef struct {
    float x, y, u, v;
    uint8_t c[4];
} V2;

struct FernDraw2D {
    GLuint program, vbo, ibo, white, font;
    GLint u_mvp, u_tex;
    V2 v[MAX_QUADS * 4];
    int quads;
    GLuint tex;
    int w, h;
    uint8_t widths[256];
    float font_cell;      // The size of one glyph cell, in pixels of the font image.
    int builtin;
};

static const char *vs =
    "attribute vec2 a_pos;\n"
    "attribute vec2 a_uv;\n"
    "attribute vec4 a_color;\n"
    "uniform mat4 u_mvp;\n"
    "varying vec2 v_uv;\n"
    "varying vec4 v_color;\n"
    "void main() { gl_Position = u_mvp * vec4(a_pos, 0.0, 1.0); v_uv = a_uv; v_color = a_color; }\n";

static const char *fs =
    "precision mediump float;\n"
    "uniform sampler2D u_tex;\n"
    "varying vec2 v_uv;\n"
    "varying vec4 v_color;\n"
    "void main() {\n"
    "    vec4 t = texture2D(u_tex, v_uv) * v_color;\n"
    "    if (t.a < 0.01) discard;\n"
    "    gl_FragColor = t;\n"
    "}\n";

// The built-in font: 5 x 7 glyphs drawn for Fernstone. Row 0 is the top,
// and bit 4 is the left column. Small letters use the capital glyphs.
static const struct {
    char c;
    uint8_t row[7];
} glyphs[] = {
    {'!', {4, 4, 4, 4, 4, 0, 4}}, {'"', {10, 10, 0, 0, 0, 0, 0}}, {'#', {10, 31, 10, 10, 31, 10, 0}},
    {'%', {24, 25, 2, 4, 8, 19, 3}}, {'\'', {4, 4, 8, 0, 0, 0, 0}}, {'(', {2, 4, 8, 8, 8, 4, 2}},
    {')', {8, 4, 2, 2, 2, 4, 8}}, {'+', {0, 4, 4, 31, 4, 4, 0}}, {',', {0, 0, 0, 0, 12, 4, 8}},
    {'-', {0, 0, 0, 31, 0, 0, 0}}, {'.', {0, 0, 0, 0, 0, 12, 12}}, {'/', {1, 1, 2, 4, 8, 16, 16}},
    {'0', {14, 17, 19, 21, 25, 17, 14}}, {'1', {4, 12, 4, 4, 4, 4, 14}}, {'2', {14, 17, 1, 2, 4, 8, 31}},
    {'3', {31, 2, 4, 2, 1, 17, 14}}, {'4', {2, 6, 10, 18, 31, 2, 2}}, {'5', {31, 16, 30, 1, 1, 17, 14}},
    {'6', {6, 8, 16, 30, 17, 17, 14}}, {'7', {31, 1, 2, 4, 8, 8, 8}}, {'8', {14, 17, 17, 14, 17, 17, 14}},
    {'9', {14, 17, 17, 15, 1, 2, 12}}, {':', {0, 12, 12, 0, 12, 12, 0}}, {';', {0, 12, 12, 0, 12, 4, 8}},
    {'<', {2, 4, 8, 16, 8, 4, 2}}, {'=', {0, 0, 31, 0, 31, 0, 0}}, {'>', {8, 4, 2, 1, 2, 4, 8}},
    {'?', {14, 17, 1, 2, 4, 0, 4}}, {'A', {14, 17, 17, 31, 17, 17, 17}}, {'B', {30, 17, 17, 30, 17, 17, 30}},
    {'C', {14, 17, 16, 16, 16, 17, 14}}, {'D', {28, 18, 17, 17, 17, 18, 28}}, {'E', {31, 16, 16, 30, 16, 16, 31}},
    {'F', {31, 16, 16, 30, 16, 16, 16}}, {'G', {14, 17, 16, 23, 17, 17, 15}}, {'H', {17, 17, 17, 31, 17, 17, 17}},
    {'I', {14, 4, 4, 4, 4, 4, 14}}, {'J', {7, 2, 2, 2, 2, 18, 12}}, {'K', {17, 18, 20, 24, 20, 18, 17}},
    {'L', {16, 16, 16, 16, 16, 16, 31}}, {'M', {17, 27, 21, 21, 17, 17, 17}}, {'N', {17, 17, 25, 21, 19, 17, 17}},
    {'O', {14, 17, 17, 17, 17, 17, 14}}, {'P', {30, 17, 17, 30, 16, 16, 16}}, {'Q', {14, 17, 17, 17, 21, 18, 13}},
    {'R', {30, 17, 17, 30, 20, 18, 17}}, {'S', {15, 16, 16, 14, 1, 1, 30}}, {'T', {31, 4, 4, 4, 4, 4, 4}},
    {'U', {17, 17, 17, 17, 17, 17, 14}}, {'V', {17, 17, 17, 17, 17, 10, 4}}, {'W', {17, 17, 17, 21, 21, 21, 10}},
    {'X', {17, 17, 10, 4, 10, 17, 17}}, {'Y', {17, 17, 17, 10, 4, 4, 4}}, {'Z', {31, 1, 2, 4, 8, 16, 31}},
    {'[', {14, 8, 8, 8, 8, 8, 14}}, {']', {14, 2, 2, 2, 2, 2, 14}}, {'_', {0, 0, 0, 0, 0, 0, 31}},
};

FernDraw2D *fern_d2_new(void) {
    static const char *attribs[] = {"a_pos", "a_uv", "a_color", NULL};
    static const uint8_t white[4] = {255, 255, 255, 255};
    FernDraw2D *d = calloc(1, sizeof(*d));
    GLushort *idx = malloc(sizeof(GLushort) * MAX_QUADS * 6);
    int i;
    d->program = fern_gl_program(vs, fs, attribs);
    d->u_mvp = glGetUniformLocation(d->program, "u_mvp");
    d->u_tex = glGetUniformLocation(d->program, "u_tex");
    for (i = 0; i < MAX_QUADS; i++) {
        idx[i * 6] = (GLushort)(i * 4);
        idx[i * 6 + 1] = (GLushort)(i * 4 + 1);
        idx[i * 6 + 2] = (GLushort)(i * 4 + 2);
        idx[i * 6 + 3] = (GLushort)(i * 4);
        idx[i * 6 + 4] = (GLushort)(i * 4 + 2);
        idx[i * 6 + 5] = (GLushort)(i * 4 + 3);
    }
    glGenBuffers(1, &d->ibo);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, d->ibo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(GLushort) * MAX_QUADS * 6, idx, GL_STATIC_DRAW);
    free(idx);
    glGenBuffers(1, &d->vbo);
    d->white = fern_gl_texture(white, 1, 1);
    fern_d2_set_font(d, NULL, 0, 0);
    return d;
}

void fern_d2_begin(FernDraw2D *d, int w, int h) {
    d->w = w;
    d->h = h;
    d->quads = 0;
    d->tex = 0;
}

void fern_d2_flush(FernDraw2D *d) {
    FernMat m;
    if (!d->quads) return;
    m = fern_mat_ortho(0, (float)d->w, (float)d->h, 0, -1, 1);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glUseProgram(d->program);
    glUniformMatrix4fv(d->u_mvp, 1, GL_FALSE, m.m);
    glUniform1i(d->u_tex, 0);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, d->tex ? d->tex : d->white);
    glBindBuffer(GL_ARRAY_BUFFER, d->vbo);
    glBufferData(GL_ARRAY_BUFFER, (GLsizeiptr)(sizeof(V2) * 4 * (size_t)d->quads), d->v, GL_STREAM_DRAW);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, d->ibo);
    glEnableVertexAttribArray(0);
    glEnableVertexAttribArray(1);
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(V2), (void *)0);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(V2), (void *)8);
    glVertexAttribPointer(2, 4, GL_UNSIGNED_BYTE, GL_TRUE, sizeof(V2), (void *)16);
    glDrawElements(GL_TRIANGLES, d->quads * 6, GL_UNSIGNED_SHORT, 0);
    glDisableVertexAttribArray(2);
    d->quads = 0;
}

void fern_d2_image(FernDraw2D *d, GLuint tex, float x, float y, float w, float h, float u0, float v0, float u1,
                   float v1, uint32_t rgba) {
    V2 *v;
    int i;
    if (tex != d->tex || d->quads >= MAX_QUADS) {
        fern_d2_flush(d);
        d->tex = tex;
    }
    v = d->v + d->quads * 4;
    v[0].x = x;     v[0].y = y;     v[0].u = u0; v[0].v = v0;
    v[1].x = x + w; v[1].y = y;     v[1].u = u1; v[1].v = v0;
    v[2].x = x + w; v[2].y = y + h; v[2].u = u1; v[2].v = v1;
    v[3].x = x;     v[3].y = y + h; v[3].u = u0; v[3].v = v1;
    for (i = 0; i < 4; i++) {
        v[i].c[0] = (uint8_t)(rgba >> 24);
        v[i].c[1] = (uint8_t)(rgba >> 16);
        v[i].c[2] = (uint8_t)(rgba >> 8);
        v[i].c[3] = (uint8_t)rgba;
    }
    d->quads++;
}

void fern_d2_rect(FernDraw2D *d, float x, float y, float w, float h, uint32_t rgba) {
    fern_d2_image(d, 0, x, y, w, h, 0, 0, 1, 1, rgba);
}

void fern_d2_set_font(FernDraw2D *d, const uint8_t *rgba, int w, int h) {
    static uint8_t built[128 * 128 * 4];
    unsigned i;
    int ch, x, y;
    if (d->font) glDeleteTextures(1, &d->font);
    if (!rgba) {
        // Draw the built-in glyphs into a grid like the one of the jar.
        memset(built, 0, sizeof(built));
        for (i = 0; i < sizeof(glyphs) / sizeof(glyphs[0]); i++) {
            int c = (unsigned char)glyphs[i].c;
            for (y = 0; y < 7; y++)
                for (x = 0; x < 5; x++)
                    if (glyphs[i].row[y] & (16 >> x)) {
                        uint8_t *p = built + (((c >> 4) * 8 + y) * 128 + (c & 15) * 8 + x) * 4;
                        p[0] = p[1] = p[2] = p[3] = 255;
                    }
        }
        rgba = built;
        w = h = 128;
        d->builtin = 1;
    } else {
        d->builtin = 0;
    }
    d->font = fern_gl_texture(rgba, w, h);
    d->font_cell = (float)w / 16.0f;
    // The width of each glyph: the last column with a pixel, plus one.
    for (ch = 0; ch < 256; ch++) {
        int cell = w / 16, last = -1;
        for (x = 0; x < cell; x++)
            for (y = 0; y < cell; y++)
                if (rgba[(((ch >> 4) * cell + y) * w + (ch & 15) * cell + x) * 4 + 3] > 128) last = x;
        d->widths[ch] = (uint8_t)(last < 0 ? (ch == ' ' ? cell / 2 : 0) : (last + 2) * 8 / cell);
        if (ch == ' ') d->widths[ch] = 4;
    }
}

// The 16 colors of the chat codes, as in Beta.
static uint32_t code_color(int c, uint32_t alpha) {
    int j = (c >> 3 & 1) * 85;
    int r = (c >> 2 & 1) * 170 + j, g = (c >> 1 & 1) * 170 + j, b = (c & 1) * 170 + j;
    if (c == 6) r += 85;
    return FERN_RGBA(r, g, b, alpha);
}

// Read one character as a Latin-1 code (the font has 256 glyphs).
static int next_char(const unsigned char **s) {
    const unsigned char *p = *s;
    int c = *p++;
    if (c >= 0xC0 && (*p & 0xC0) == 0x80) {
        int cp = (c & 0x1F) << 6 | (*p++ & 0x3F);
        if (c >= 0xE0) {
            while ((*p & 0xC0) == 0x80) p++;
            cp = '?';
        }
        c = cp < 256 ? cp : '?';
    }
    *s = p;
    return c;
}

static float draw_run(FernDraw2D *d, const char *utf8, float x, float y, float scale, uint32_t rgba, int draw) {
    const unsigned char *s = (const unsigned char *)utf8;
    float start = x, cell = d->font_cell;
    uint32_t color = rgba;
    while (*s) {
        int c = next_char(&s), g;
        if (c == 0xA7 && *s) {
            int k = next_char(&s);
            const char *hex = "0123456789abcdef";
            const char *f = strchr(hex, k | 0x20);
            if (f && k) color = code_color((int)(f - hex), rgba & 255);
            continue;
        }
        g = c;
        if (d->builtin && c >= 'a' && c <= 'z') g = c - 32;
        if (draw && g != ' ') {
            float u = (g & 15) * cell / (16 * cell), v = (g >> 4) * cell / (16 * cell);
            fern_d2_image(d, d->font, x, y, 8 * scale, 8 * scale, u, v, u + 1.0f / 16, v + 1.0f / 16, color);
        }
        x += d->widths[g] * scale;
    }
    return x - start;
}

float fern_d2_text(FernDraw2D *d, const char *utf8, float x, float y, float scale, uint32_t rgba, int shadow) {
    if (shadow) {
        // The shadow keeps the color codes dark, as in Beta.
        char plain[512];
        const char *s = utf8;
        size_t n = 0;
        while (*s && n + 1 < sizeof(plain)) {
            if ((unsigned char)s[0] == 0xC2 && (unsigned char)s[1] == 0xA7 && s[2]) {
                s += 3;
                continue;
            }
            plain[n++] = *s++;
        }
        plain[n] = 0;
        draw_run(d, plain, x + scale, y + scale, scale, FERN_RGBA(40, 40, 40, rgba & 255), 1);
    }
    return draw_run(d, utf8, x, y, scale, rgba, 1);
}

float fern_d2_text_width(FernDraw2D *d, const char *utf8, float scale) {
    return draw_run(d, utf8, 0, 0, scale, 0, 0);
}
