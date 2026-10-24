#include "fx.h"
#include <math.h>
#include <string.h>

void fern_fx_init(FernFx *fx, int lava, uint32_t seed) {
    memset(fx, 0, sizeof(*fx));
    fx->lava = lava;
    fx->seed = seed ? seed : 1;
}

// A small random generator, so that a test gives the same frames.
static float rnd(FernFx *fx) {
    fx->seed = fx->seed * 1664525u + 1013904223u;
    return (fx->seed >> 8) / 16777216.0f;
}

static void water_tick(FernFx *fx) {
    int x, y, i;
    for (x = 0; x < 16; x++)
        for (y = 0; y < 16; y++) {
            float f = 0;
            for (i = x - 1; i <= x + 1; i++) f += fx->a[(i & 15) + (y & 15) * 16];
            fx->b[x + y * 16] = f / 3.3f + fx->c[x + y * 16] * 0.8f;
        }
    for (i = 0; i < 256; i++) {
        fx->c[i] += fx->d[i] * 0.05f;
        if (fx->c[i] < 0) fx->c[i] = 0;
        fx->d[i] -= 0.1f;
        if (rnd(fx) < 0.05f) fx->d[i] = 0.5f;
    }
}

static void lava_tick(FernFx *fx) {
    int x, y, i, j;
    for (x = 0; x < 16; x++)
        for (y = 0; y < 16; y++) {
            float f = 0;
            int l = (int)(sinf(y * 3.14159265f * 2 / 16) * 1.2f), m = (int)(sinf(x * 3.14159265f * 2 / 16) * 1.2f);
            for (i = x - 1; i <= x + 1; i++)
                for (j = y - 1; j <= y + 1; j++) f += fx->a[((i + l) & 15) + ((j + m) & 15) * 16];
            fx->b[x + y * 16] = f / 10.0f + (fx->c[(x & 15) + (y & 15) * 16] + fx->c[((x + 1) & 15) + (y & 15) * 16] +
                                             fx->c[((x + 1) & 15) + ((y + 1) & 15) * 16] +
                                             fx->c[(x & 15) + ((y + 1) & 15) * 16]) / 4.0f * 0.8f;
            fx->c[x + y * 16] += fx->d[x + y * 16] * 0.01f;
            if (fx->c[x + y * 16] < 0) fx->c[x + y * 16] = 0;
            fx->d[x + y * 16] -= 0.06f;
            if (rnd(fx) < 0.005f) fx->d[x + y * 16] = 1.5f;
        }
}

void fern_fx_step(FernFx *fx, uint8_t out[16 * 16 * 4]) {
    float tmp[256];
    int i;
    if (fx->lava) lava_tick(fx); else water_tick(fx);
    memcpy(tmp, fx->a, sizeof(tmp));
    memcpy(fx->a, fx->b, sizeof(tmp));
    memcpy(fx->b, tmp, sizeof(tmp));
    for (i = 0; i < 256; i++) {
        float f = fx->a[i];
        if (fx->lava) {
            f *= 2;
            if (f > 1) f = 1;
            if (f < 0) f = 0;
            out[i * 4] = (uint8_t)(f * 100 + 155);
            out[i * 4 + 1] = (uint8_t)(f * f * 255);
            out[i * 4 + 2] = (uint8_t)(f * f * f * f * 128);
            out[i * 4 + 3] = 255;
        } else {
            float f2;
            if (f > 1) f = 1;
            if (f < 0) f = 0;
            f2 = f * f;
            out[i * 4] = (uint8_t)(32 + f2 * 32);
            out[i * 4 + 1] = (uint8_t)(50 + f2 * 64);
            out[i * 4 + 2] = 255;
            out[i * 4 + 3] = (uint8_t)(146 + f2 * 50);
        }
    }
}
