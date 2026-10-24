// dig.h: the time to break a block, with the rules of Beta 1.7.3.
#ifndef FERN_DIG_H
#define FERN_DIG_H

#include "blocks.h"

// The part of the block that one tick breaks, with the item in the hand
// (-1 for an empty hand). 1 or more breaks the block in one tick. 0: never.
float fern_dig_speed(int block, int item);
// The ticks to break a block. -1 if it does not break.
int fern_dig_ticks(int block, int item);

#endif
