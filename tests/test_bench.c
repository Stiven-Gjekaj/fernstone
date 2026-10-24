#include "bench.h"
#include "test.h"

TEST(bench_runs_for_two_seconds_of_work_in_slices) {
    FernBench b;
    int slices = 0;
    fern_bench_start(&b);
    while (!b.done && slices < 1000) {
        fern_bench_step(&b, 50);
        slices++;
    }
    CHECK(b.done);
    CHECK(b.work_s >= 2.0 && b.work_s < 2.2);
    // A slice can go a little over its time, so there are a bit fewer than 40.
    CHECK(slices >= 30);
    CHECK(b.meshes > 0);
    CHECK_EQ(b.score, (int)(b.meshes / b.work_s / 10.0));
    printf("  bench: %ld meshes, score %d\n", b.meshes, b.score);
}
