#include "dig.h"
#include "test.h"

TEST(dig_breaks_dirt_by_hand_in_fifteen_ticks) {
    // 1 / 0.5 / 30 for each tick: 15 ticks, 0.75 s.
    CHECK_EQ(fern_dig_ticks(B_DIRT, -1), 15);
}

TEST(dig_breaks_stone_fast_with_a_pickaxe) {
    CHECK_EQ(fern_dig_ticks(B_STONE, -1), 150);
    CHECK_EQ(fern_dig_ticks(B_STONE, 270), 23);
    CHECK_EQ(fern_dig_ticks(B_STONE, 278), 6);
}

TEST(dig_needs_an_iron_pickaxe_for_diamond_ore) {
    CHECK(fern_dig_ticks(B_DIAMOND_ORE, 274) > fern_dig_ticks(B_DIAMOND_ORE, 257) * 10);
}

TEST(dig_breaks_flowers_at_once_and_bedrock_never) {
    CHECK_EQ(fern_dig_ticks(B_ROSE, -1), 1);
    CHECK_EQ(fern_dig_ticks(B_BEDROCK, 278), -1);
}
