#include "test.h"
#include "world.h"

TEST(world_gives_air_where_nothing_is_loaded) {
    FernWorld w;
    fern_world_init(&w);
    CHECK_EQ(fern_world_get(&w, 5, 64, 5), 0);
    fern_world_set(&w, 5, 64, 5, FERN_STATE(B_STONE, 0));
    CHECK_EQ(fern_world_get(&w, 5, 64, 5), 0);
}

TEST(world_keeps_a_block_in_a_loaded_column) {
    FernWorld w;
    fern_world_init(&w);
    fern_world_column_make(&w, -1, 2);
    fern_world_set(&w, -3, 70, 40, FERN_STATE(B_WOOL, 14));
    CHECK_EQ(FERN_BLOCK(fern_world_get(&w, -3, 70, 40)), B_WOOL);
    CHECK_EQ(FERN_META(fern_world_get(&w, -3, 70, 40)), 14);
    CHECK_EQ(fern_world_get(&w, -3, 71, 40), 0);
    CHECK_EQ(fern_world_column(&w, -1, 2)->sec[70 >> 4]->solid, 1);
    fern_world_set(&w, -3, 70, 40, 0);
    CHECK_EQ(fern_world_column(&w, -1, 2)->sec[70 >> 4]->solid, 0);
    fern_world_clear(&w);
}

TEST(world_marks_the_section_next_to_an_edge) {
    FernWorld w;
    FernColumn *a, *b;
    fern_world_init(&w);
    a = fern_world_column_make(&w, 0, 0);
    b = fern_world_column_make(&w, 1, 0);
    a->dirty = b->dirty = 0;
    fern_world_set(&w, 15, 33, 4, FERN_STATE(B_DIRT, 0));
    CHECK_EQ(a->dirty, (1u << 2));
    CHECK_EQ(b->dirty, (1u << 2));
    fern_world_clear(&w);
}

TEST(world_unloads_a_column) {
    FernWorld w;
    fern_world_init(&w);
    fern_world_column_make(&w, 3, 3);
    fern_world_column_make(&w, 3 + FERN_WORLD_BUCKETS, 3);
    CHECK_EQ(w.columns, 2);
    fern_world_unload(&w, 3, 3);
    CHECK(fern_world_column(&w, 3, 3) == NULL);
    CHECK(fern_world_column(&w, 3 + FERN_WORLD_BUCKETS, 3) != NULL);
    fern_world_clear(&w);
    CHECK_EQ(w.columns, 0);
}

TEST(blocks_give_the_terrain_tiles) {
    CHECK_EQ(fern_block_texture(FERN_STATE(B_GRASS, 0), 1), 0);
    CHECK_EQ(fern_block_texture(FERN_STATE(B_GRASS, 0), 2), 3);
    CHECK_EQ(fern_block_texture(FERN_STATE(B_MOSSY_COBBLE, 0), 4), 36);
    CHECK_EQ(fern_block_texture(FERN_STATE(B_WOOL, 0), 0), 64);
    CHECK_EQ(fern_block_texture(FERN_STATE(B_WOOL, 15), 0), 113);
    CHECK_EQ(fern_block_texture(FERN_STATE(B_FURNACE, 3), 3), 44);
    CHECK_EQ(fern_block_texture(FERN_STATE(B_FURNACE, 3), 2), 45);
}

TEST(blocks_map_unknown_wire_ids) {
    CHECK_EQ(fern_block_from_wire(14, 48), B_MOSSY_COBBLE);
    CHECK_EQ(fern_block_from_wire(14, 97), B_UNKNOWN);
    CHECK_EQ(fern_block_from_wire(14, 0), B_AIR);
    CHECK(fern_block_info(B_UNKNOWN)->flags & BF_SOLID);
}
