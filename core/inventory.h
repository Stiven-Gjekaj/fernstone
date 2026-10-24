// inventory.h: the rules of a click on a slot of a window, as Beta has them.
// The client shows the result at once. The server answers each click and
// sends the true slots.
#ifndef FERN_INVENTORY_H
#define FERN_INVENTORY_H

#include "packet.h"

// The size of a full stack of an item.
int fern_item_max_stack(int id);
// Change a slot and the item on the cursor for a click. right: the right
// button. result: the slot is a crafting or furnace output, so a click only
// takes from it.
void fern_inv_click(FernItem *slot, FernItem *cursor, int right, int result);

#endif
