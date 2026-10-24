#include <stdlib.h>
#include <zlib.h>
#include "chunk.h"
#include "test.h"

// Build a full column the way a b1.7.3 server sends it: 16 x 128 x 16.
static FernPacket make_column(int cx, int cz, uint8_t **keep) {
    int n = 16 * 128 * 16, x, y, z;
    uint8_t *raw = calloc((size_t)n * 5 / 2, 1);
    uLongf size = compressBound((uLong)n * 5 / 2);
    uint8_t *zip = malloc(size);
    FernPacket p;
    for (x = 0; x < 16; x++)
        for (z = 0; z < 16; z++)
            for (y = 0; y < 128; y++) {
                int i = (x * 16 + z) * 128 + y;
                raw[i] = y < 60 ? 1 : y == 60 ? 2 : 0;
                if (y == 61 && x == 3 && z == 4) {
                    raw[i] = 35;
                    raw[n + i / 2] |= (uint8_t)(14 << ((i & 1) * 4));
                }
                // Sky light: 15 above the ground.
                raw[n * 2 + i / 2] |= (uint8_t)((y > 60 ? 15 : 0) << ((i & 1) * 4));
            }
    compress(zip, &size, raw, (uLong)n * 5 / 2);
    free(raw);
    p.id = 0x33;
    p.u.chunk.x = cx * 16;
    p.u.chunk.y = 0;
    p.u.chunk.z = cz * 16;
    p.u.chunk.sx = 16;
    p.u.chunk.sy = 128;
    p.u.chunk.sz = 16;
    p.u.chunk.size = (int32_t)size;
    p.u.chunk.data = zip;
    *keep = zip;
    return p;
}

TEST(chunk_writes_a_full_column) {
    FernWorld w;
    uint8_t *zip;
    FernPacket p = make_column(2, -1, &zip);
    fern_world_init(&w);
    CHECK(fern_chunk_apply(&w, fern_version_latest(), &p));
    CHECK_EQ(FERN_BLOCK(fern_world_get(&w, 32, 10, -16)), B_STONE);
    CHECK_EQ(FERN_BLOCK(fern_world_get(&w, 40, 60, -10)), B_GRASS);
    CHECK_EQ(fern_world_get(&w, 40, 61, -10), 0);
    CHECK_EQ(fern_world_get(&w, 35, 61, -12), FERN_STATE(B_WOOL, 14));
    CHECK_EQ(fern_world_sky(&w, 40, 61, -10), 15);
    CHECK_EQ(fern_world_sky(&w, 40, 59, -10), 0);
    // The empty sections at the top have no storage.
    CHECK(fern_world_column(&w, 2, -1)->sec[7] == NULL);
    free(zip);
    fern_world_clear(&w);
}

TEST(chunk_refuses_bad_data) {
    FernWorld w;
    FernPacket p;
    static const uint8_t junk[4] = {1, 2, 3, 4};
    fern_world_init(&w);
    p.u.chunk.x = 0;
    p.u.chunk.y = 0;
    p.u.chunk.z = 0;
    p.u.chunk.sx = p.u.chunk.sy = p.u.chunk.sz = 1;
    p.u.chunk.size = 4;
    p.u.chunk.data = junk;
    CHECK_EQ(fern_chunk_apply(&w, fern_version_latest(), &p), 0);
}

TEST(chunk_applies_a_multi_block_change) {
    FernWorld w;
    FernPacket p;
    static const uint8_t coords[4] = {0x12, 0x40, 0xF0, 0x7F}; // (1,2,64) and (15,0,127)
    static const uint8_t types[2] = {48, 20};
    static const uint8_t metas[2] = {0, 0};
    fern_world_init(&w);
    fern_world_column_make(&w, -1, 0);
    p.u.multi.cx = -1;
    p.u.multi.cz = 0;
    p.u.multi.count = 2;
    p.u.multi.coords = coords;
    p.u.multi.types = types;
    p.u.multi.metas = metas;
    fern_chunk_apply_multi(&w, fern_version_latest(), &p);
    CHECK_EQ(FERN_BLOCK(fern_world_get(&w, -15, 64, 2)), B_MOSSY_COBBLE);
    CHECK_EQ(FERN_BLOCK(fern_world_get(&w, -1, 127, 0)), B_GLASS);
    fern_world_clear(&w);
}
