#include <stdlib.h>
#include "mesh.h"
#include "test.h"

static FernMeshInput *empty_input(void) {
    FernMeshInput *in = calloc(1, sizeof(*in));
    memset(in->light, 0xF0, sizeof(in->light));
    return in;
}

static void put(FernMeshInput *in, int x, int y, int z, FernState s) {
    in->state[(y + 1) * FERN_SNAP * FERN_SNAP + (z + 1) * FERN_SNAP + (x + 1)] = s;
}

TEST(mesh_draws_six_faces_of_a_lone_cube) {
    FernMeshInput *in = empty_input();
    FernMeshOutput out;
    memset(&out, 0, sizeof(out));
    put(in, 3, 4, 5, FERN_STATE(B_STONE, 0));
    fern_mesh_build(in, &out);
    CHECK_EQ(out.solid.count, 6 * 4);
    CHECK_EQ(out.water.count, 0);
    fern_mesh_free(&out);
    free(in);
}

TEST(mesh_hides_the_faces_between_two_cubes) {
    FernMeshInput *in = empty_input();
    FernMeshOutput out;
    memset(&out, 0, sizeof(out));
    put(in, 3, 4, 5, FERN_STATE(B_STONE, 0));
    put(in, 4, 4, 5, FERN_STATE(B_DIRT, 0));
    fern_mesh_build(in, &out);
    CHECK_EQ(out.solid.count, 10 * 4);
    fern_mesh_free(&out);
    free(in);
}

TEST(mesh_hides_a_face_next_to_the_section_edge) {
    FernMeshInput *in = empty_input();
    FernMeshOutput out;
    memset(&out, 0, sizeof(out));
    // A block in the neighbor section, at x = 16, hides the east face.
    put(in, 15, 0, 0, FERN_STATE(B_STONE, 0));
    put(in, 16, 0, 0, FERN_STATE(B_STONE, 0));
    fern_mesh_build(in, &out);
    CHECK_EQ(out.solid.count, 5 * 4);
    fern_mesh_free(&out);
    free(in);
}

TEST(mesh_puts_water_in_the_translucent_list) {
    FernMeshInput *in = empty_input();
    FernMeshOutput out;
    int i, top = 0;
    memset(&out, 0, sizeof(out));
    put(in, 0, 0, 0, FERN_STATE(B_WATER, 0));
    put(in, 1, 0, 0, FERN_STATE(B_WATER, 0));
    fern_mesh_build(in, &out);
    CHECK_EQ(out.solid.count, 0);
    CHECK_EQ(out.water.count, 10 * 4);
    // The top of still water is 14/16 high.
    for (i = 0; i < out.water.count; i++)
        if (out.water.v[i].y > top) top = out.water.v[i].y;
    CHECK_EQ(top, 14);
    fern_mesh_free(&out);
    free(in);
}

TEST(mesh_gives_the_grass_top_its_tile_and_tint) {
    FernMeshInput *in = empty_input();
    FernMeshOutput out;
    int i, found = 0;
    memset(&out, 0, sizeof(out));
    put(in, 0, 0, 0, FERN_STATE(B_GRASS, 0));
    fern_mesh_build(in, &out);
    for (i = 0; i < out.solid.count; i += 4) {
        if (out.solid.v[i].y == 16 && out.solid.v[i + 1].y == 16 && out.solid.v[i + 2].y == 16) {
            // Tile 0 is at the top left of the atlas.
            CHECK(out.solid.v[i].u < 65535 / 16 + 1);
            CHECK(out.solid.v[i].g > out.solid.v[i].r);
            found = 1;
        }
    }
    CHECK(found);
    fern_mesh_free(&out);
    free(in);
}

TEST(mesh_snapshot_copies_the_neighbors) {
    FernWorld w;
    FernMeshInput *in = empty_input();
    fern_world_init(&w);
    fern_world_column_make(&w, 0, 0);
    fern_world_column_make(&w, 1, 0);
    fern_world_set(&w, 5, 20, 5, FERN_STATE(B_STONE, 0));
    fern_world_set(&w, 16, 20, 5, FERN_STATE(B_DIRT, 0));
    CHECK(fern_mesh_snapshot(&w, 0, 1, 0, in));
    CHECK_EQ(FERN_BLOCK(in->state[(4 + 1) * FERN_SNAP * FERN_SNAP + 6 * FERN_SNAP + 17]), B_DIRT);
    CHECK(!fern_mesh_snapshot(&w, 0, 2, 0, in));
    fern_world_clear(&w);
    free(in);
}
