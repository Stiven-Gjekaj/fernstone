#include "inventory.h"

int fern_item_max_stack(int id) {
    if (id <= 0) return 64;
    // Tools, weapons, armor, buckets, the boat, the minecarts, the doors,
    // the signs, the saddle, the bed, the cake, and the records.
    if ((id >= 256 && id <= 259) || id == 261 || (id >= 267 && id <= 279) || (id >= 283 && id <= 286) ||
        (id >= 290 && id <= 294) || (id >= 298 && id <= 317) || (id >= 323 && id <= 330) || id == 333 ||
        id == 335 || id == 342 || id == 343 || id == 346 || id == 354 || id == 355 || id == 358 || id == 359 ||
        id >= 2256)
        return 1;
    if (id == 332 || id == 344) return 16; // snowball, egg
    if (id == 357) return 8;               // cookie
    return 64;
}

static int same(const FernItem *a, const FernItem *b) {
    return a->id == b->id && a->damage == b->damage;
}

static void clear(FernItem *it) {
    it->id = -1;
    it->count = 0;
    it->damage = 0;
}

void fern_inv_click(FernItem *slot, FernItem *cursor, int right, int result) {
    int max;
    if (result) {
        // An output: take all of it, if the cursor is empty or holds the same.
        if (slot->id < 0) return;
        if (cursor->id < 0) {
            *cursor = *slot;
            clear(slot);
        } else if (same(cursor, slot) && cursor->count + slot->count <= fern_item_max_stack(slot->id)) {
            cursor->count = (int8_t)(cursor->count + slot->count);
            clear(slot);
        }
        return;
    }
    if (cursor->id < 0) {
        if (slot->id < 0) return;
        if (right) {
            // Take half, the larger half.
            int take = (slot->count + 1) / 2;
            *cursor = *slot;
            cursor->count = (int8_t)take;
            slot->count = (int8_t)(slot->count - take);
            if (slot->count <= 0) clear(slot);
        } else {
            *cursor = *slot;
            clear(slot);
        }
        return;
    }
    if (slot->id < 0) {
        if (right) {
            *slot = *cursor;
            slot->count = 1;
            cursor->count--;
            if (cursor->count <= 0) clear(cursor);
        } else {
            *slot = *cursor;
            clear(cursor);
        }
        return;
    }
    if (same(slot, cursor)) {
        max = fern_item_max_stack(slot->id);
        if (right) {
            if (slot->count < max) {
                slot->count++;
                cursor->count--;
                if (cursor->count <= 0) clear(cursor);
            }
        } else {
            int move = max - slot->count;
            if (move > cursor->count) move = cursor->count;
            if (move < 0) move = 0;
            slot->count = (int8_t)(slot->count + move);
            cursor->count = (int8_t)(cursor->count - move);
            if (cursor->count <= 0) clear(cursor);
        }
        return;
    }
    // Two different items: swap them.
    {
        FernItem t = *slot;
        *slot = *cursor;
        *cursor = t;
    }
}
