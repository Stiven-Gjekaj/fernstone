#include "dig.h"
#include <math.h>
#include <stddef.h>

typedef struct {
    int16_t id;
    uint8_t tool;
    uint8_t level;   // 0 wood, 1 stone, 2 iron, 3 diamond, 0 gold
    float speed;
} ToolItem;

// The tools of b1.7.3 and their speed on the blocks of their kind.
static const ToolItem tools[] = {
    {270, TOOL_PICK, 0, 2}, {274, TOOL_PICK, 1, 4}, {257, TOOL_PICK, 2, 6}, {278, TOOL_PICK, 3, 8}, {285, TOOL_PICK, 0, 12},
    {269, TOOL_SHOVEL, 0, 2}, {273, TOOL_SHOVEL, 1, 4}, {256, TOOL_SHOVEL, 2, 6}, {277, TOOL_SHOVEL, 3, 8}, {284, TOOL_SHOVEL, 0, 12},
    {271, TOOL_AXE, 0, 2}, {275, TOOL_AXE, 1, 4}, {258, TOOL_AXE, 2, 6}, {279, TOOL_AXE, 3, 8}, {286, TOOL_AXE, 0, 12},
    {268, TOOL_SWORD, 0, 1.5f}, {272, TOOL_SWORD, 1, 1.5f}, {267, TOOL_SWORD, 2, 1.5f}, {276, TOOL_SWORD, 3, 1.5f},
    {283, TOOL_SWORD, 0, 1.5f},
};

static const ToolItem *tool_of(int item) {
    unsigned i;
    for (i = 0; i < sizeof(tools) / sizeof(tools[0]); i++)
        if (tools[i].id == item) return &tools[i];
    return NULL;
}

// A pickaxe is needed to get a drop from rock and metal blocks, and a
// shovel from snow. Other blocks drop with any item.
static int needs_tool(const FernBlockInfo *b, int block) {
    return b->tool == TOOL_PICK || block == B_SNOW || block == B_SNOW_LAYER || block == B_WEB;
}

float fern_dig_speed(int block, int item) {
    const FernBlockInfo *b = fern_block_info(block);
    const ToolItem *t = tool_of(item);
    float strength = 1.0f;
    int harvest;
    if (b->hardness < 0) return 0;
    if (b->hardness == 0) return 1.0f;
    if (t && t->tool == b->tool && t->tool != TOOL_NONE) strength = t->speed;
    if (t && t->tool == TOOL_SWORD && block == B_WEB) strength = 15.0f;
    if (item == 359 && (block == B_LEAVES || block == B_WEB || block == B_WOOL)) strength = block == B_WOOL ? 5 : 15;
    harvest = !needs_tool(b, block);
    if (!harvest && t) {
        if (b->tool == TOOL_PICK) harvest = t->tool == TOOL_PICK && t->level >= b->harvest;
        else if (block == B_WEB) harvest = t->tool == TOOL_SWORD || item == 359;
        else harvest = t->tool == TOOL_SHOVEL;
    }
    return harvest ? strength / b->hardness / 30.0f : 1.0f / b->hardness / 100.0f;
}

int fern_dig_ticks(int block, int item) {
    float s = fern_dig_speed(block, item);
    if (s <= 0) return -1;
    // The small margin keeps a float error from adding one tick.
    return (int)ceil(1.0 / s - 1e-4);
}
