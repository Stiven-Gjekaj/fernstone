#include "bench.h"
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include "mesh.h"

#define BENCH_SECONDS 2.0

static FernMeshInput *input;
static FernMeshOutput output;

double fern_bench_now(void) {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (double)tv.tv_sec + tv.tv_usec / 1e6;
}

// A section like the ground of a Beta world: stone, dirt and grass, a few
// ores, caves, water, flowers, and one tree, so that each face rule runs.
static void make_section(FernMeshInput *in) {
    int x, y, z;
    unsigned seed = 12345;
    memset(in, 0, sizeof(*in));
    for (y = -1; y <= 16; y++)
        for (z = -1; z <= 16; z++)
            for (x = -1; x <= 16; x++) {
                int i = (y + 1) * FERN_SNAP * FERN_SNAP + (z + 1) * FERN_SNAP + (x + 1);
                int ground = 9 + ((x * 7 + z * 13) % 4);
                FernState s = 0;
                seed = seed * 1103515245u + 12345u;
                if (y < ground - 3) s = FERN_STATE((seed >> 16) % 23 == 0 ? B_COAL_ORE : B_STONE, 0);
                else if (y < ground) s = FERN_STATE(B_DIRT, 0);
                else if (y == ground) s = FERN_STATE(B_GRASS, 0);
                else if (y == ground + 1 && (seed >> 16) % 11 == 0) s = FERN_STATE(B_TALLGRASS, 1);
                if (y > 2 && y < 6 && ((x - 8) * (x - 8) + (z - 5) * (z - 5)) < 9) s = 0;
                if (x > 10 && z > 10 && y > ground - 2 && y <= ground) s = FERN_STATE(B_WATER, 0);
                if (x == 4 && z == 4 && y > ground && y < ground + 5) s = FERN_STATE(B_LOG, 0);
                if (abs(x - 4) <= 2 && abs(z - 4) <= 2 && y >= ground + 4 && y <= ground + 6 && !(x == 4 && z == 4))
                    s = FERN_STATE(B_LEAVES, 0);
                in->state[i] = s;
                in->light[i] = (uint8_t)(s ? 0x00 : 0xF0);
            }
}

void fern_bench_start(FernBench *b) {
    memset(b, 0, sizeof(*b));
    if (!input) {
        input = malloc(sizeof(*input));
        make_section(input);
    }
}

void fern_bench_step(FernBench *b, double slice_ms) {
    double start = fern_bench_now(), t = start;
    if (b->done) return;
    while ((t - start) * 1000.0 < slice_ms && b->work_s + (t - start) < BENCH_SECONDS) {
        fern_mesh_build(input, &output);
        b->meshes++;
        t = fern_bench_now();
    }
    b->work_s += t - start;
    if (b->work_s >= BENCH_SECONDS) {
        b->done = 1;
        b->score = (int)(b->meshes / b->work_s / 10.0);
    }
}
