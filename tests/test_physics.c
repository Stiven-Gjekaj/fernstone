#include <math.h>
#include "physics.h"
#include "test.h"

// A flat world: stone up to y = 63, air above.
static void flat(FernWorld *w) {
    int x, z, y;
    fern_world_init(w);
    for (x = -1; x <= 1; x++)
        for (z = -1; z <= 1; z++) fern_world_column_make(w, x, z);
    for (x = -16; x < 32; x++)
        for (z = -16; z < 32; z++)
            for (y = 60; y < 64; y++) fern_world_set(w, x, y, z, FERN_STATE(B_STONE, 0));
}

static FernBody body_at(double x, double y, double z) {
    FernBody b;
    memset(&b, 0, sizeof(b));
    b.x = x;
    b.y = y;
    b.z = z;
    return b;
}

TEST(physics_falls_and_lands_on_the_ground) {
    FernWorld w;
    FernMoveInput in = {0, 0, 0, 0};
    FernBody b = body_at(0.5, 70, 0.5);
    int i;
    flat(&w);
    for (i = 0; i < 60; i++) fern_body_tick(&b, &w, &in);
    CHECK(fabs(b.y - 64.0) < 1e-9);
    CHECK(b.on_ground);
    fern_world_clear(&w);
}

TEST(physics_jumps_a_bit_more_than_one_block) {
    FernWorld w;
    FernMoveInput in = {0, 0, 1, 0};
    FernBody b = body_at(0.5, 64, 0.5);
    double top = 0;
    int i;
    flat(&w);
    b.on_ground = 1;
    for (i = 0; i < 12; i++) {
        fern_body_tick(&b, &w, &in);
        in.jump = 0;
        if (b.y > top) top = b.y;
    }
    // Beta jumps about 1.25 blocks.
    CHECK(top - 64 > 1.2 && top - 64 < 1.3);
    fern_world_clear(&w);
}

TEST(physics_walks_at_the_speed_of_beta) {
    FernWorld w;
    FernMoveInput in = {1, 0, 0, 0};
    FernBody b = body_at(0.5, 64, 0.5);
    int i;
    double z0;
    flat(&w);
    b.on_ground = 1;
    for (i = 0; i < 20; i++) fern_body_tick(&b, &w, &in);
    z0 = b.z;
    for (i = 0; i < 20; i++) fern_body_tick(&b, &w, &in);
    // About 4.3 blocks in one second, toward +z at yaw 0.
    CHECK(b.z - z0 > 4.1 && b.z - z0 < 4.4);
    fern_world_clear(&w);
}

TEST(physics_stops_at_a_wall_and_steps_on_a_slab) {
    FernWorld w;
    FernMoveInput in = {1, 0, 0, 0};
    FernBody b = body_at(0.5, 64, 0.5);
    int i;
    flat(&w);
    fern_world_set(&w, 0, 64, 3, FERN_STATE(B_STONE, 0));
    fern_world_set(&w, 0, 65, 3, FERN_STATE(B_STONE, 0));
    b.on_ground = 1;
    for (i = 0; i < 40; i++) fern_body_tick(&b, &w, &in);
    CHECK(fabs(b.z - 2.7) < 1e-6);
    fern_world_set(&w, 0, 65, 3, 0);
    fern_world_set(&w, 0, 64, 3, FERN_STATE(B_SLAB, 0));
    for (i = 0; i < 20 && b.z < 3.5; i++) fern_body_tick(&b, &w, &in);
    // The body is now over the slab, so it stands on its top.
    CHECK(b.z > 3.5 && b.z < 4.0);
    CHECK(fabs(b.y - 64.5) < 1e-9);
    fern_world_clear(&w);
}

TEST(physics_casts_a_ray_onto_the_top_face) {
    FernWorld w;
    FernRayHit h;
    flat(&w);
    h = fern_raycast(&w, 0.5, 65.62, 0.5, 0, -1, 0.0001, 5);
    CHECK(h.hit);
    CHECK_EQ(h.y, 63);
    CHECK_EQ(h.face, 1);
    h = fern_raycast(&w, 0.5, 65.62, 0.5, 0, 1, 0, 5);
    CHECK(!h.hit);
    fern_world_clear(&w);
}
