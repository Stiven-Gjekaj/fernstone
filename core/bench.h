// bench.h: the chunk meshing benchmark of the version browser.
// It builds the mesh of the same made-up section again and again for 2
// seconds of work. The score is the count of meshes in one second, / 10.
// The app runs it in short slices over many frames, so the screen stays live.
#ifndef FERN_BENCH_H
#define FERN_BENCH_H

typedef struct {
    double work_s;      // The time of the meshing so far.
    long meshes;
    int done;
    int score;
} FernBench;

void fern_bench_start(FernBench *b);
// Run the benchmark for about slice_ms. Set done when 2 s of work are done.
void fern_bench_step(FernBench *b, double slice_ms);
// The time in seconds, for the benchmark. iOS 6 has no clock_gettime.
double fern_bench_now(void);

#endif
