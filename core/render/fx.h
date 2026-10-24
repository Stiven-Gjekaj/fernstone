// fx.h: the moving textures of water and lava. Beta does not take them from
// terrain.png. It makes them with a small simulation in each tick
// (TextureWaterFX and TextureLavaFX). This is the same simulation.
#ifndef FERN_FX_H
#define FERN_FX_H

#include <stdint.h>

typedef struct {
    float a[256], b[256], c[256], d[256];
    uint32_t seed;
    int lava;
} FernFx;

void fern_fx_init(FernFx *fx, int lava, uint32_t seed);
// Run one tick and write the 16 x 16 RGBA tile.
void fern_fx_step(FernFx *fx, uint8_t out[16 * 16 * 4]);

#endif
