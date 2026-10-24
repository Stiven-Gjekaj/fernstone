#include "fx.h"
#include "test.h"

TEST(fx_makes_moving_water) {
    FernFx fx;
    uint8_t a[1024], b[1024];
    int i, changed = 0;
    fern_fx_init(&fx, 0, 7);
    for (i = 0; i < 40; i++) fern_fx_step(&fx, a);
    fern_fx_step(&fx, b);
    for (i = 0; i < 1024; i++) changed += a[i] != b[i];
    CHECK(changed > 0);
    // Water is blue and a little transparent.
    for (i = 0; i < 256; i++) {
        CHECK_EQ(b[i * 4 + 2], 255);
        CHECK(b[i * 4 + 3] >= 146 && b[i * 4 + 3] <= 196);
    }
}

TEST(fx_makes_hot_lava) {
    FernFx fx;
    uint8_t px[1024];
    int i;
    fern_fx_init(&fx, 1, 7);
    for (i = 0; i < 100; i++) fern_fx_step(&fx, px);
    // Lava is red and opaque.
    for (i = 0; i < 256; i++) {
        CHECK(px[i * 4] >= 155);
        CHECK_EQ(px[i * 4 + 3], 255);
    }
}
