// world.h: the internal world model. The renderer reads only this.
// A column is 16 x 16 blocks and holds sections of 16 x 16 x 16 blocks. An
// empty section has no storage. The protocol adapter writes the blocks and
// the light that the server sends.
#ifndef FERN_WORLD_H
#define FERN_WORLD_H

#include <stddef.h>
#include <stdint.h>
#include "blocks.h"

#define FERN_SECTIONS 8          // 128 blocks high in Alpha and Beta.
#define FERN_HEIGHT (FERN_SECTIONS * 16)

typedef struct {
    FernState state[4096];       // index: y << 8 | z << 4 | x
    uint8_t light[4096];         // high nibble: sky light, low nibble: block light
    uint16_t solid;              // The count of blocks that are not air.
} FernSection;

typedef struct FernColumn {
    int32_t x, z;
    FernSection *sec[FERN_SECTIONS];
    uint32_t dirty;              // A bit for each section that needs a new mesh.
    uint32_t version;            // It changes with each edit of the column.
    struct FernColumn *next;     // The next column in the same hash bucket.
} FernColumn;

#define FERN_WORLD_BUCKETS 1024

typedef struct {
    FernColumn *bucket[FERN_WORLD_BUCKETS];
    int columns;
    int64_t time;                // The world time in ticks.
} FernWorld;

void fern_world_init(FernWorld *w);
void fern_world_clear(FernWorld *w);
FernColumn *fern_world_column(FernWorld *w, int cx, int cz);
FernColumn *fern_world_column_make(FernWorld *w, int cx, int cz);
void fern_world_unload(FernWorld *w, int cx, int cz);
// Give air for a block in a column that is not loaded or out of height.
FernState fern_world_get(FernWorld *w, int x, int y, int z);
void fern_world_set(FernWorld *w, int x, int y, int z, FernState s);
// The sky light and the block light (0 to 15). Outside the world, full sky.
int fern_world_sky(FernWorld *w, int x, int y, int z);
int fern_world_light(FernWorld *w, int x, int y, int z);
// Mark a section and the sections next to a block as dirty.
void fern_world_mark(FernWorld *w, int x, int y, int z);

#endif
