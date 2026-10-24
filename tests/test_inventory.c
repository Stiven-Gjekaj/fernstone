#include "inventory.h"
#include "test.h"

static FernItem item(int id, int count) {
    FernItem it;
    it.id = (int16_t)id;
    it.count = (int8_t)count;
    it.damage = 0;
    return it;
}

TEST(inventory_picks_up_and_puts_down_a_stack) {
    FernItem slot = item(4, 20), cursor = item(-1, 0), other = item(-1, 0);
    fern_inv_click(&slot, &cursor, 0, 0);
    CHECK_EQ(slot.id, -1);
    CHECK_EQ(cursor.count, 20);
    fern_inv_click(&other, &cursor, 0, 0);
    CHECK_EQ(other.count, 20);
    CHECK_EQ(cursor.id, -1);
}

TEST(inventory_takes_half_and_puts_one_with_the_right_button) {
    FernItem slot = item(4, 7), cursor = item(-1, 0), other = item(-1, 0);
    fern_inv_click(&slot, &cursor, 1, 0);
    CHECK_EQ(cursor.count, 4);
    CHECK_EQ(slot.count, 3);
    fern_inv_click(&other, &cursor, 1, 0);
    CHECK_EQ(other.count, 1);
    CHECK_EQ(cursor.count, 3);
}

TEST(inventory_merges_up_to_a_full_stack) {
    FernItem slot = item(4, 60), cursor = item(4, 10);
    fern_inv_click(&slot, &cursor, 0, 0);
    CHECK_EQ(slot.count, 64);
    CHECK_EQ(cursor.count, 6);
}

TEST(inventory_swaps_two_items) {
    FernItem slot = item(4, 5), cursor = item(276, 1);
    fern_inv_click(&slot, &cursor, 0, 0);
    CHECK_EQ(slot.id, 276);
    CHECK_EQ(cursor.id, 4);
}

TEST(inventory_only_takes_from_an_output) {
    FernItem out = item(58, 1), cursor = item(4, 3);
    fern_inv_click(&out, &cursor, 0, 1);
    CHECK_EQ(out.id, 58);
    cursor = item(-1, 0);
    fern_inv_click(&out, &cursor, 0, 1);
    CHECK_EQ(cursor.id, 58);
    CHECK_EQ(out.id, -1);
}

TEST(inventory_knows_the_stack_sizes) {
    CHECK_EQ(fern_item_max_stack(4), 64);
    CHECK_EQ(fern_item_max_stack(276), 1);
    CHECK_EQ(fern_item_max_stack(332), 16);
}
