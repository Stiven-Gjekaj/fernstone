#include "blocks.h"

#define O BF_OPAQUE
#define S BF_SOLID
#define C BF_CUTOUT
#define T BF_TRANSLUCENT
#define R BF_REPLACEABLE
#define P TOOL_PICK
#define SH TOOL_SHOVEL
#define AX TOOL_AXE
#define N TOOL_NONE

// The hardness, the tool, and the pickaxe level come from b1.7.3.
static const FernBlockInfo infos[B_COUNT] = {
    {"air", SHAPE_NONE, R, N, 0, 0, 0},
    {"stone", SHAPE_CUBE, O | S, P, 0, 1.5f, 0},
    {"grass", SHAPE_CUBE, O | S | BF_TINT_GRASS, SH, 0, 0.6f, 0},
    {"dirt", SHAPE_CUBE, O | S, SH, 0, 0.5f, 0},
    {"cobblestone", SHAPE_CUBE, O | S, P, 0, 2.0f, 0},
    {"planks", SHAPE_CUBE, O | S, AX, 0, 2.0f, 0},
    {"sapling", SHAPE_CROSS, C, N, 0, 0, 0},
    {"bedrock", SHAPE_CUBE, O | S, N, 0, -1, 0},
    {"flowing_water", SHAPE_LIQUID, T | R | BF_LIQUID, N, 0, 100, 0},
    {"water", SHAPE_LIQUID, T | R | BF_LIQUID, N, 0, 100, 0},
    {"flowing_lava", SHAPE_LIQUID, R | BF_LIQUID, N, 0, 0, 15},
    {"lava", SHAPE_LIQUID, R | BF_LIQUID, N, 0, 100, 15},
    {"sand", SHAPE_CUBE, O | S, SH, 0, 0.5f, 0},
    {"gravel", SHAPE_CUBE, O | S, SH, 0, 0.6f, 0},
    {"gold_ore", SHAPE_CUBE, O | S, P, 2, 3.0f, 0},
    {"iron_ore", SHAPE_CUBE, O | S, P, 1, 3.0f, 0},
    {"coal_ore", SHAPE_CUBE, O | S, P, 0, 3.0f, 0},
    {"log", SHAPE_CUBE, O | S, AX, 0, 2.0f, 0},
    {"leaves", SHAPE_CUBE, S | C | BF_TINT_LEAVES, N, 0, 0.2f, 0},
    {"sponge", SHAPE_CUBE, O | S, N, 0, 0.6f, 0},
    {"glass", SHAPE_CUBE, S | C, N, 0, 0.3f, 0},
    {"lapis_ore", SHAPE_CUBE, O | S, P, 1, 3.0f, 0},
    {"lapis_block", SHAPE_CUBE, O | S, P, 1, 3.0f, 0},
    {"dispenser", SHAPE_CUBE, O | S, P, 0, 3.5f, 0},
    {"sandstone", SHAPE_CUBE, O | S, P, 0, 0.8f, 0},
    {"note_block", SHAPE_CUBE, O | S, AX, 0, 0.8f, 0},
    {"bed", SHAPE_SLAB, S, N, 0, 0.2f, 0},
    {"powered_rail", SHAPE_FLAT, C, P, 0, 0.7f, 0},
    {"detector_rail", SHAPE_FLAT, C, P, 0, 0.7f, 0},
    {"sticky_piston", SHAPE_CUBE, O | S, N, 0, 0.5f, 0},
    {"web", SHAPE_CROSS, C, TOOL_SWORD, 0, 4.0f, 0},
    {"tallgrass", SHAPE_CROSS, C | R | BF_TINT_GRASS, N, 0, 0, 0},
    {"deadbush", SHAPE_CROSS, C | R, N, 0, 0, 0},
    {"piston", SHAPE_CUBE, O | S, N, 0, 0.5f, 0},
    {"piston_head", SHAPE_CUBE, S, N, 0, 0.5f, 0},
    {"wool", SHAPE_CUBE, O | S, N, 0, 0.8f, 0},
    {"piston_moving", SHAPE_NONE, 0, N, 0, -1, 0},
    {"dandelion", SHAPE_CROSS, C, N, 0, 0, 0},
    {"rose", SHAPE_CROSS, C, N, 0, 0, 0},
    {"brown_mushroom", SHAPE_CROSS, C, N, 0, 0, 1},
    {"red_mushroom", SHAPE_CROSS, C, N, 0, 0, 0},
    {"gold_block", SHAPE_CUBE, O | S, P, 2, 3.0f, 0},
    {"iron_block", SHAPE_CUBE, O | S, P, 1, 5.0f, 0},
    {"double_slab", SHAPE_CUBE, O | S, P, 0, 2.0f, 0},
    {"slab", SHAPE_SLAB, S, P, 0, 2.0f, 0},
    {"brick", SHAPE_CUBE, O | S, P, 0, 2.0f, 0},
    {"tnt", SHAPE_CUBE, O | S, N, 0, 0, 0},
    {"bookshelf", SHAPE_CUBE, O | S, AX, 0, 1.5f, 0},
    {"mossy_cobblestone", SHAPE_CUBE, O | S, P, 0, 2.0f, 0},
    {"obsidian", SHAPE_CUBE, O | S, P, 3, 10.0f, 0},
    {"torch", SHAPE_TORCH, C, N, 0, 0, 14},
    {"fire", SHAPE_CROSS, C | R, N, 0, 0, 15},
    {"mob_spawner", SHAPE_CUBE, S | C, P, 0, 5.0f, 0},
    {"wooden_stairs", SHAPE_STAIRS, S, AX, 0, 2.0f, 0},
    {"chest", SHAPE_CUBE, O | S, AX, 0, 2.5f, 0},
    {"redstone_wire", SHAPE_FLAT, C, N, 0, 0, 0},
    {"diamond_ore", SHAPE_CUBE, O | S, P, 2, 3.0f, 0},
    {"diamond_block", SHAPE_CUBE, O | S, P, 2, 5.0f, 0},
    {"workbench", SHAPE_CUBE, O | S, AX, 0, 2.5f, 0},
    {"crops", SHAPE_CROPS, C, N, 0, 0, 0},
    {"farmland", SHAPE_CUBE, O | S, SH, 0, 0.6f, 0},
    {"furnace", SHAPE_CUBE, O | S, P, 0, 3.5f, 0},
    {"lit_furnace", SHAPE_CUBE, O | S, P, 0, 3.5f, 13},
    {"sign", SHAPE_TORCH, 0, AX, 0, 1.0f, 0},
    {"wooden_door", SHAPE_DOOR, S | C, AX, 0, 3.0f, 0},
    {"ladder", SHAPE_LADDER, C, AX, 0, 0.4f, 0},
    {"rail", SHAPE_FLAT, C, P, 0, 0.7f, 0},
    {"stone_stairs", SHAPE_STAIRS, S, P, 0, 2.0f, 0},
    {"wall_sign", SHAPE_LADDER, 0, AX, 0, 1.0f, 0},
    {"lever", SHAPE_TORCH, C, N, 0, 0.5f, 0},
    {"stone_pressure_plate", SHAPE_FLAT, 0, P, 0, 0.5f, 0},
    {"iron_door", SHAPE_DOOR, S | C, P, 0, 5.0f, 0},
    {"wooden_pressure_plate", SHAPE_FLAT, 0, AX, 0, 0.5f, 0},
    {"redstone_ore", SHAPE_CUBE, O | S, P, 2, 3.0f, 0},
    {"lit_redstone_ore", SHAPE_CUBE, O | S, P, 2, 3.0f, 9},
    {"unlit_redstone_torch", SHAPE_TORCH, C, N, 0, 0, 0},
    {"redstone_torch", SHAPE_TORCH, C, N, 0, 0, 7},
    {"stone_button", SHAPE_FLAT, 0, P, 0, 0.5f, 0},
    {"snow_layer", SHAPE_SNOW, R, SH, 0, 0.1f, 0},
    {"ice", SHAPE_CUBE, S | T, P, 0, 0.5f, 0},
    {"snow", SHAPE_CUBE, O | S, SH, 0, 0.2f, 0},
    {"cactus", SHAPE_CACTUS, S | C, N, 0, 0.4f, 0},
    {"clay", SHAPE_CUBE, O | S, SH, 0, 0.6f, 0},
    {"reeds", SHAPE_CROSS, C, N, 0, 0, 0},
    {"jukebox", SHAPE_CUBE, O | S, AX, 0, 2.0f, 0},
    {"fence", SHAPE_FENCE, S, AX, 0, 2.0f, 0},
    {"pumpkin", SHAPE_CUBE, O | S, AX, 0, 1.0f, 0},
    {"netherrack", SHAPE_CUBE, O | S, P, 0, 0.4f, 0},
    {"soul_sand", SHAPE_CUBE, O | S, SH, 0, 0.5f, 0},
    {"glowstone", SHAPE_CUBE, O | S, N, 0, 0.3f, 15},
    {"portal", SHAPE_CUBE, T, N, 0, -1, 11},
    {"lit_pumpkin", SHAPE_CUBE, O | S, AX, 0, 1.0f, 15},
    {"cake", SHAPE_SLAB, S, N, 0, 0.5f, 0},
    {"unpowered_repeater", SHAPE_FLAT, 0, N, 0, 0, 0},
    {"powered_repeater", SHAPE_FLAT, 0, N, 0, 0, 9},
    {"locked_chest", SHAPE_CUBE, O | S, AX, 0, 0, 15},
    {"trapdoor", SHAPE_FLAT, S | C, AX, 0, 3.0f, 0},
};

static const FernBlockInfo unknown = {"unknown", SHAPE_CUBE, O | S, N, 0, 1.0f, 0};

const FernBlockInfo *fern_block_info(int block) {
    if (block >= 0 && block < B_COUNT) return &infos[block];
    return &unknown;
}

// The wool colors, by meta value.
static const uint8_t wool_tex[16] = {64, 210, 194, 178, 162, 146, 130, 114,
                                     225, 209, 193, 177, 161, 145, 129, 113};

// The face that the front of a furnace, a dispenser, or a pumpkin uses.
// The meta of a furnace is the face (2 to 5). A pumpkin uses 0 to 3:
// south, west, north, east.
static int pumpkin_front(int meta) {
    static const uint8_t face[4] = {3, 4, 2, 5};
    return face[meta & 3];
}

int fern_block_texture(FernState state, int face) {
    int block = FERN_BLOCK(state), meta = FERN_META(state);
    int side = face >= 2;
    switch (block) {
    case B_STONE: return 1;
    case B_GRASS: return face == 1 ? 0 : face == 0 ? 2 : 3;
    case B_DIRT: return 2;
    case B_COBBLE: return 16;
    case B_PLANKS: return 4;
    case B_SAPLING: return meta == 1 ? 63 : meta == 2 ? 79 : 15;
    case B_BEDROCK: return 17;
    case B_WATER_FLOW: case B_WATER: return 205;
    case B_LAVA_FLOW: case B_LAVA: return 237;
    case B_SAND: return 18;
    case B_GRAVEL: return 19;
    case B_GOLD_ORE: return 32;
    case B_IRON_ORE: return 33;
    case B_COAL_ORE: return 34;
    case B_LOG:
        if (!side) return 21;
        return meta == 1 ? 116 : meta == 2 ? 117 : 20;
    case B_LEAVES: return meta == 1 ? 132 : 52;
    case B_SPONGE: return 48;
    case B_GLASS: return 49;
    case B_LAPIS_ORE: return 160;
    case B_LAPIS_BLOCK: return 144;
    case B_DISPENSER:
        if (!side) return 62;
        return face == meta ? 46 : 45;
    case B_SANDSTONE: return face == 1 ? 176 : face == 0 ? 208 : 192;
    case B_NOTE: return 74;
    case B_BED: return face == 1 ? ((meta & 8) ? 135 : 134) : 4;
    case B_RAIL_POWERED: return (meta & 8) ? 163 : 179;
    case B_RAIL_DETECTOR: return 195;
    case B_PISTON_STICKY: case B_PISTON: case B_PISTON_HEAD: {
        int dir = meta & 7;
        if (face == dir) return block == B_PISTON_STICKY ? 106 : 107;
        if ((face ^ 1) == dir) return 109;
        return 108;
    }
    case B_WEB: return 11;
    case B_TALLGRASS: return meta == 0 ? 55 : meta == 2 ? 56 : 39;
    case B_DEADBUSH: return 55;
    case B_WOOL: return wool_tex[meta];
    case B_DANDELION: return 13;
    case B_ROSE: return 12;
    case B_MUSHROOM_BROWN: return 29;
    case B_MUSHROOM_RED: return 28;
    case B_GOLD_BLOCK: return 23;
    case B_IRON_BLOCK: return 22;
    case B_DOUBLE_SLAB: case B_SLAB:
        switch (meta & 7) {
        case 1: return face == 1 ? 176 : face == 0 ? 208 : 192;
        case 2: return 4;
        case 3: return 16;
        default: return side ? 5 : 6;
        }
    case B_BRICK: return 7;
    case B_TNT: return face == 1 ? 9 : face == 0 ? 10 : 8;
    case B_BOOKSHELF: return side ? 35 : 4;
    case B_MOSSY_COBBLE: return 36;
    case B_OBSIDIAN: return 37;
    case B_TORCH: return 80;
    case B_FIRE: return 31;
    case B_SPAWNER: return 65;
    case B_WOOD_STAIRS: return 4;
    case B_CHEST: case B_LOCKED_CHEST:
        if (!side) return 25;
        return face == 3 ? 27 : 26;
    case B_REDSTONE_WIRE: return 164;
    case B_DIAMOND_ORE: return 50;
    case B_DIAMOND_BLOCK: return 24;
    case B_WORKBENCH:
        if (face == 1) return 43;
        if (face == 0) return 4;
        return (face == 2 || face == 4) ? 60 : 59;
    case B_CROPS: return 88 + (meta & 7);
    case B_FARMLAND: return face == 1 ? (meta > 0 ? 86 : 87) : 2;
    case B_FURNACE: case B_FURNACE_LIT:
        if (!side) return 62;
        if (face == meta) return block == B_FURNACE_LIT ? 61 : 44;
        return 45;
    case B_SIGN_POST: case B_SIGN_WALL: return 4;
    case B_DOOR_WOOD: return (meta & 8) ? 81 : 97;
    case B_LADDER: return 83;
    case B_RAIL: return meta >= 6 ? 112 : 128;
    case B_COBBLE_STAIRS: return 16;
    case B_LEVER: return 96;
    case B_PLATE_STONE: return 1;
    case B_DOOR_IRON: return (meta & 8) ? 82 : 98;
    case B_PLATE_WOOD: return 4;
    case B_REDSTONE_ORE: case B_REDSTONE_ORE_LIT: return 51;
    case B_REDSTONE_TORCH_OFF: return 115;
    case B_REDSTONE_TORCH: return 99;
    case B_BUTTON: return 1;
    case B_SNOW_LAYER: return 66;
    case B_ICE: return 67;
    case B_SNOW: return 66;
    case B_CACTUS: return face == 1 ? 69 : face == 0 ? 71 : 70;
    case B_CLAY: return 72;
    case B_REEDS: return 73;
    case B_JUKEBOX: return face == 1 ? 75 : 74;
    case B_FENCE: return 4;
    case B_PUMPKIN: case B_JACK_O_LANTERN:
        if (!side) return 102;
        if (face == pumpkin_front(meta)) return block == B_PUMPKIN ? 119 : 120;
        return 118;
    case B_NETHERRACK: return 103;
    case B_SOULSAND: return 104;
    case B_GLOWSTONE: return 105;
    case B_PORTAL: return 14;
    case B_CAKE: return face == 1 ? 121 : face == 0 ? 124 : 122;
    case B_REPEATER_OFF: return face == 1 ? 131 : 1;
    case B_REPEATER_ON: return face == 1 ? 147 : 1;
    case B_TRAPDOOR: return 84;
    default: return 1;
    }
}

// The block tables. The table number is the packet layout of the version
// (2 to 14). Each table has the wire IDs of b1.7.3, without the blocks that
// came later. Uberbukkit gives the lists (Protocol2 to Protocol13).
static int known(int table, int id) {
    if (table <= 13 && (id == 29 || id == 33 || id == 34 || id == 36)) return 0;
    if (table <= 11 && (id == 31 || id == 32 || id == 96)) return 0;
    if (table <= 10 && (id == 27 || id == 28 || id == 30)) return 0;
    if (table <= 9 && id == 95) return 0;
    if (table <= 8 && (id == 26 || id == 93 || id == 94)) return 0;
    if (table <= 7 && id >= 21 && id <= 25) return 0;
    if (table <= 2 && id >= 86 && id <= 91) return 0;
    return 1;
}

int fern_block_from_wire(int table, int wire_id) {
    if (wire_id == 0) return B_AIR;
    if (wire_id < 0 || wire_id > B_TRAPDOOR || !known(table, wire_id)) return B_UNKNOWN;
    return wire_id;
}

int fern_block_to_wire(int table, int block) {
    if (block <= 0 || block > B_TRAPDOOR || !known(table, block)) return block == 0 ? 0 : -1;
    return block;
}
