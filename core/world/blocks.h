// blocks.h: the internal block registry.
// The world model stores a block state as a 16-bit value: the internal
// block ID in the high 12 bits and the meta value in the low 4 bits. The
// internal IDs are not wire IDs. A table for each version maps the wire
// IDs to the internal IDs (see fern_block_from_wire). A later protocol adds
// its own table and its own internal blocks.
#ifndef FERN_BLOCKS_H
#define FERN_BLOCKS_H

#include <stdint.h>

typedef uint16_t FernState;

#define FERN_STATE(block, meta) ((FernState)((block) << 4 | ((meta) & 15)))
#define FERN_BLOCK(state) ((state) >> 4)
#define FERN_META(state) ((state) & 15)

#define FERN_BLOCK_MAX 256

// The internal blocks. The first 97 have the order of b1.7.3.
enum {
    B_AIR, B_STONE, B_GRASS, B_DIRT, B_COBBLE, B_PLANKS, B_SAPLING, B_BEDROCK,
    B_WATER_FLOW, B_WATER, B_LAVA_FLOW, B_LAVA, B_SAND, B_GRAVEL, B_GOLD_ORE,
    B_IRON_ORE, B_COAL_ORE, B_LOG, B_LEAVES, B_SPONGE, B_GLASS, B_LAPIS_ORE,
    B_LAPIS_BLOCK, B_DISPENSER, B_SANDSTONE, B_NOTE, B_BED, B_RAIL_POWERED,
    B_RAIL_DETECTOR, B_PISTON_STICKY, B_WEB, B_TALLGRASS, B_DEADBUSH, B_PISTON,
    B_PISTON_HEAD, B_WOOL, B_PISTON_MOVING, B_DANDELION, B_ROSE, B_MUSHROOM_BROWN,
    B_MUSHROOM_RED, B_GOLD_BLOCK, B_IRON_BLOCK, B_DOUBLE_SLAB, B_SLAB, B_BRICK,
    B_TNT, B_BOOKSHELF, B_MOSSY_COBBLE, B_OBSIDIAN, B_TORCH, B_FIRE, B_SPAWNER,
    B_WOOD_STAIRS, B_CHEST, B_REDSTONE_WIRE, B_DIAMOND_ORE, B_DIAMOND_BLOCK,
    B_WORKBENCH, B_CROPS, B_FARMLAND, B_FURNACE, B_FURNACE_LIT, B_SIGN_POST,
    B_DOOR_WOOD, B_LADDER, B_RAIL, B_COBBLE_STAIRS, B_SIGN_WALL, B_LEVER,
    B_PLATE_STONE, B_DOOR_IRON, B_PLATE_WOOD, B_REDSTONE_ORE, B_REDSTONE_ORE_LIT,
    B_REDSTONE_TORCH_OFF, B_REDSTONE_TORCH, B_BUTTON, B_SNOW_LAYER, B_ICE, B_SNOW,
    B_CACTUS, B_CLAY, B_REEDS, B_JUKEBOX, B_FENCE, B_PUMPKIN, B_NETHERRACK,
    B_SOULSAND, B_GLOWSTONE, B_PORTAL, B_JACK_O_LANTERN, B_CAKE, B_REPEATER_OFF,
    B_REPEATER_ON, B_LOCKED_CHEST, B_TRAPDOOR,
    B_COUNT,
    B_UNKNOWN = 255 // A wire ID that the table of the version does not know.
};

// How the mesher draws a block.
typedef enum {
    SHAPE_NONE,
    SHAPE_CUBE,
    SHAPE_CROSS,
    SHAPE_LIQUID,
    SHAPE_FLAT,    // Rails, redstone wire, plates: a thin layer on the floor.
    SHAPE_TORCH,
    SHAPE_SLAB,
    SHAPE_STAIRS,
    SHAPE_FENCE,
    SHAPE_DOOR,
    SHAPE_LADDER,
    SHAPE_CACTUS,
    SHAPE_SNOW,
    SHAPE_CROPS
} FernShape;

enum {
    BF_OPAQUE = 1,        // A full opaque cube. It hides the faces next to it.
    BF_SOLID = 2,         // The player collides with it.
    BF_CUTOUT = 4,        // The texture has holes (alpha test).
    BF_TRANSLUCENT = 8,   // Drawn in the blended pass (water, ice).
    BF_TINT_GRASS = 16,
    BF_TINT_LEAVES = 32,
    BF_REPLACEABLE = 64,  // A placed block replaces it (tall grass, snow).
    BF_LIQUID = 128
};

typedef enum { TOOL_NONE, TOOL_PICK, TOOL_SHOVEL, TOOL_AXE, TOOL_SWORD } FernTool;

typedef struct {
    const char *name;
    uint8_t shape;
    uint8_t flags;
    uint8_t tool;
    uint8_t harvest;   // The pickaxe level to get a drop: 0 any, 1 stone, 2 iron, 3 diamond.
    float hardness;    // -1: the block does not break.
    uint8_t light;     // The light that the block gives, 0 to 15.
} FernBlockInfo;

const FernBlockInfo *fern_block_info(int block);
// The index in terrain.png (16 x 16 tiles) of a face of a block.
// Faces: 0 down, 1 up, 2 north (-z), 3 south (+z), 4 west (-x), 5 east (+x).
int fern_block_texture(FernState state, int face);
// The internal block of a wire ID, for the block table of a version.
int fern_block_from_wire(int table, int wire_id);
// The wire ID of an internal block, for a version. -1 if there is none.
int fern_block_to_wire(int table, int block);

#endif
