#include "mesh.h"
#include <stdlib.h>
#include <string.h>

#define S FERN_SNAP
#define AT(x, y, z) (((y) + 1) * S * S + ((z) + 1) * S + ((x) + 1))

int fern_mesh_snapshot(FernWorld *w, int sx, int sy, int sz, FernMeshInput *in) {
    int x, y, z, any = 0;
    int bx = sx * 16, by = sy * 16, bz = sz * 16;
    FernColumn *c = fern_world_column(w, sx, sz);
    in->sx = sx;
    in->sy = sy;
    in->sz = sz;
    if (!c || !c->sec[sy] || c->sec[sy]->solid == 0) {
        return 0;
    }
    for (y = -1; y <= 16; y++)
        for (z = -1; z <= 16; z++)
            for (x = -1; x <= 16; x++) {
                int i = AT(x, y, z);
                int wx = bx + x, wy = by + y, wz = bz + z;
                FernState st = fern_world_get(w, wx, wy, wz);
                in->state[i] = st;
                in->light[i] = (uint8_t)(fern_world_sky(w, wx, wy, wz) << 4 | fern_world_light(w, wx, wy, wz));
                if (st) any = 1;
            }
    return any;
}

static FernVertex *grow(FernVertexList *l, int n) {
    if (l->count + n > l->cap) {
        l->cap = l->cap ? l->cap * 2 : 1024;
        while (l->cap < l->count + n) l->cap *= 2;
        l->v = realloc(l->v, sizeof(FernVertex) * (size_t)l->cap);
    }
    l->count += n;
    return l->v + l->count - n;
}

void fern_mesh_free(FernMeshOutput *out) {
    free(out->solid.v);
    free(out->water.v);
    memset(out, 0, sizeof(*out));
}

typedef struct {
    const FernMeshInput *in;
    FernMeshOutput *out;
    int x, y, z;             // The block in the section.
} Ctx;

// The shade of each face, as Beta draws it.
static const float shade[6] = {0.5f, 1.0f, 0.8f, 0.8f, 0.6f, 0.6f};

static const int dir[6][3] = {{0, -1, 0}, {0, 1, 0}, {0, 0, -1}, {0, 0, 1}, {-1, 0, 0}, {1, 0, 0}};

static FernState at(const Ctx *c, int dx, int dy, int dz) {
    return c->in->state[AT(c->x + dx, c->y + dy, c->z + dz)];
}

static uint8_t light_at(const Ctx *c, int dx, int dy, int dz) {
    return c->in->light[AT(c->x + dx, c->y + dy, c->z + dz)];
}

// The light of a face: the light of the block in front of it. A face in
// a solid block takes the light of its own block, which the server gives.
static uint8_t face_light(const Ctx *c, int face) {
    uint8_t l = light_at(c, dir[face][0], dir[face][1], dir[face][2]);
    uint8_t own = light_at(c, 0, 0, 0);
    if ((l >> 4) < (own >> 4)) l = (uint8_t)((own & 0xF0) | (l & 15));
    if ((l & 15) < (own & 15)) l = (uint8_t)((l & 0xF0) | (own & 15));
    return l;
}

static void tint_of(int block, int face, uint8_t *rgb) {
    const FernBlockInfo *info = fern_block_info(block);
    rgb[0] = rgb[1] = rgb[2] = 255;
    if ((info->flags & BF_TINT_GRASS) && (block != B_GRASS || face == 1)) {
        rgb[0] = 124; rgb[1] = 189; rgb[2] = 107;
    } else if (info->flags & BF_TINT_LEAVES) {
        rgb[0] = 92; rgb[1] = 171; rgb[2] = 60;
    } else if (block == B_REDSTONE_WIRE) {
        rgb[0] = 200; rgb[1] = 30; rgb[2] = 30;
    }
}

// Add a quad. p holds four corners in 1/16 block. uv is u0, v0, u1, v1 in
// pixels of the tile (0 to 16).
static void quad(Ctx *c, FernVertexList *l, const int p[4][3], int tile, const int uv[4], int face,
                 const uint8_t *tint, uint8_t light) {
    FernVertex *v = grow(l, 4);
    float sh = face >= 0 ? shade[face] : 0.85f;
    int tx = (tile & 15) * 16, ty = (tile >> 4) * 16, i;
    // The order of the corners: (u0,v1) (u1,v1) (u1,v0) (u0,v0).
    const int cu[4] = {uv[0], uv[2], uv[2], uv[0]}, cv[4] = {uv[3], uv[3], uv[1], uv[1]};
    for (i = 0; i < 4; i++) {
        v[i].x = (int16_t)(c->x * 16 + p[i][0]);
        v[i].y = (int16_t)(c->y * 16 + p[i][1]);
        v[i].z = (int16_t)(c->z * 16 + p[i][2]);
        v[i].pad = 0;
        // Keep a small margin from the edge of the tile. Without it, the GPU
        // of the 4S samples the next tile of the atlas on some pixels.
        v[i].u = (uint16_t)((tx + cu[i] + (cu[i] == 0 ? 0.05f : cu[i] == 16 ? -0.05f : 0)) * 65535.0f / 256.0f);
        v[i].v = (uint16_t)((ty + cv[i] + (cv[i] == 0 ? 0.05f : cv[i] == 16 ? -0.05f : 0)) * 65535.0f / 256.0f);
        v[i].r = (uint8_t)(tint[0] * sh);
        v[i].g = (uint8_t)(tint[1] * sh);
        v[i].b = (uint8_t)(tint[2] * sh);
        v[i].light = light;
    }
}

// Add one face of a box from (x0,y0,z0) to (x1,y1,z1), in 1/16 block.
// The texture follows the box, as in Beta.
static void box_face(Ctx *c, FernVertexList *l, int face, const int b[6], int tile, const uint8_t *tint,
                     uint8_t light) {
    int x0 = b[0], y0 = b[1], z0 = b[2], x1 = b[3], y1 = b[4], z1 = b[5];
    int p[4][3], uv[4];
    switch (face) {
    case 0: { int q[4][3] = {{x0, y0, z1}, {x1, y0, z1}, {x1, y0, z0}, {x0, y0, z0}}; memcpy(p, q, sizeof(p));
              uv[0] = x0; uv[1] = 16 - z1; uv[2] = x1; uv[3] = 16 - z0; break; }
    case 1: { int q[4][3] = {{x0, y1, z1}, {x1, y1, z1}, {x1, y1, z0}, {x0, y1, z0}}; memcpy(p, q, sizeof(p));
              uv[0] = x0; uv[1] = z0; uv[2] = x1; uv[3] = z1; break; }
    case 2: { int q[4][3] = {{x1, y0, z0}, {x0, y0, z0}, {x0, y1, z0}, {x1, y1, z0}}; memcpy(p, q, sizeof(p));
              uv[0] = 16 - x1; uv[1] = 16 - y1; uv[2] = 16 - x0; uv[3] = 16 - y0; break; }
    case 3: { int q[4][3] = {{x0, y0, z1}, {x1, y0, z1}, {x1, y1, z1}, {x0, y1, z1}}; memcpy(p, q, sizeof(p));
              uv[0] = x0; uv[1] = 16 - y1; uv[2] = x1; uv[3] = 16 - y0; break; }
    case 4: { int q[4][3] = {{x0, y0, z0}, {x0, y0, z1}, {x0, y1, z1}, {x0, y1, z0}}; memcpy(p, q, sizeof(p));
              uv[0] = z0; uv[1] = 16 - y1; uv[2] = z1; uv[3] = 16 - y0; break; }
    default: { int q[4][3] = {{x1, y0, z1}, {x1, y0, z0}, {x1, y1, z0}, {x1, y1, z1}}; memcpy(p, q, sizeof(p));
              uv[0] = 16 - z1; uv[1] = 16 - y1; uv[2] = 16 - z0; uv[3] = 16 - y0; break; }
    }
    quad(c, l, (const int(*)[3])p, tile, uv, face, tint, light);
}

// Give 1 if the face of a block toward a neighbor shows.
static int face_shows(const Ctx *c, int face, int block) {
    FernState n = at(c, dir[face][0], dir[face][1], dir[face][2]);
    int nb = FERN_BLOCK(n);
    const FernBlockInfo *ni = fern_block_info(nb);
    if (ni->flags & BF_OPAQUE) return 0;
    // Glass and ice hide the faces between two blocks of the same kind.
    if (nb == block && (block == B_GLASS || block == B_ICE)) return 0;
    return 1;
}

static void add_box(Ctx *c, FernVertexList *l, int block, FernState st, const int b[6], int cull_edges) {
    int face;
    for (face = 0; face < 6; face++) {
        uint8_t tint[3];
        int on_edge = (face == 0 && b[1] == 0) || (face == 1 && b[4] == 16) || (face == 2 && b[2] == 0) ||
                      (face == 3 && b[5] == 16) || (face == 4 && b[0] == 0) || (face == 5 && b[3] == 16);
        if (cull_edges && on_edge && !face_shows(c, face, block)) continue;
        tint_of(block, face, tint);
        box_face(c, l, face, b, fern_block_texture(st, face), tint, face_light(c, on_edge ? face : 1));
    }
}

static void add_cross(Ctx *c, FernVertexList *l, int block, FernState st, int inset) {
    uint8_t tint[3];
    int tile = fern_block_texture(st, 2);
    int a = inset, b = 16 - inset;
    const int uv[4] = {0, 0, 16, 16};
    const int p1[4][3] = {{a, 0, a}, {b, 0, b}, {b, 16, b}, {a, 16, a}};
    const int p2[4][3] = {{b, 0, b}, {a, 0, a}, {a, 16, a}, {b, 16, b}};
    const int p3[4][3] = {{a, 0, b}, {b, 0, a}, {b, 16, a}, {a, 16, b}};
    const int p4[4][3] = {{b, 0, a}, {a, 0, b}, {a, 16, b}, {b, 16, a}};
    uint8_t light = light_at(c, 0, 0, 0);
    tint_of(block, 1, tint);
    quad(c, l, p1, tile, uv, -1, tint, light);
    quad(c, l, p2, tile, uv, -1, tint, light);
    quad(c, l, p3, tile, uv, -1, tint, light);
    quad(c, l, p4, tile, uv, -1, tint, light);
}

static void add_crops(Ctx *c, FernVertexList *l, FernState st) {
    uint8_t tint[3] = {255, 255, 255};
    int tile = fern_block_texture(st, 2), i;
    const int uv[4] = {0, 0, 16, 16};
    uint8_t light = light_at(c, 0, 0, 0);
    for (i = 0; i < 2; i++) {
        int k = i ? 12 : 4;
        const int px[4][3] = {{k, 0, 0}, {k, 0, 16}, {k, 16, 16}, {k, 16, 0}};
        const int pxb[4][3] = {{k, 0, 16}, {k, 0, 0}, {k, 16, 0}, {k, 16, 16}};
        const int pz[4][3] = {{0, 0, k}, {16, 0, k}, {16, 16, k}, {0, 16, k}};
        const int pzb[4][3] = {{16, 0, k}, {0, 0, k}, {0, 16, k}, {16, 16, k}};
        quad(c, l, px, tile, uv, -1, tint, light);
        quad(c, l, pxb, tile, uv, -1, tint, light);
        quad(c, l, pz, tile, uv, -1, tint, light);
        quad(c, l, pzb, tile, uv, -1, tint, light);
    }
}

// A thin layer on the floor, seen from the top and the bottom.
static void add_flat(Ctx *c, FernVertexList *l, int block, FernState st) {
    uint8_t tint[3];
    int tile = fern_block_texture(st, 1);
    const int uv[4] = {0, 0, 16, 16};
    const int top[4][3] = {{0, 1, 16}, {16, 1, 16}, {16, 1, 0}, {0, 1, 0}};
    const int bottom[4][3] = {{0, 1, 0}, {16, 1, 0}, {16, 1, 16}, {0, 1, 16}};
    uint8_t light = light_at(c, 0, 0, 0);
    int meta = FERN_META(st);
    tint_of(block, 1, tint);
    if (block == B_REDSTONE_WIRE) {
        int k = 80 + meta * 11;
        tint[0] = (uint8_t)(k > 255 ? 255 : k);
    }
    if (block == B_PLATE_STONE || block == B_PLATE_WOOD || block == B_TRAPDOOR) {
        const int b[6] = {1, 0, 1, 15, block == B_TRAPDOOR ? 3 : 1, 15};
        int bb[6];
        memcpy(bb, b, sizeof(bb));
        if (block == B_TRAPDOOR) { bb[0] = 0; bb[2] = 0; bb[3] = 16; bb[5] = 16; }
        add_box(c, l, block, st, bb, 1);
        return;
    }
    if (block == B_BUTTON) {
        const int b[6] = {5, 6, 5, 11, 10, 11};
        add_box(c, l, block, st, b, 0);
        return;
    }
    quad(c, l, top, tile, uv, 1, tint, light);
    quad(c, l, bottom, tile, uv, 1, tint, light);
}

static void add_liquid(Ctx *c, FernVertexList *l, int block, FernState st) {
    int face, h = 14;
    int above = FERN_BLOCK(at(c, 0, 1, 0));
    uint8_t tint[3] = {255, 255, 255};
    int is_water = block == B_WATER || block == B_WATER_FLOW;
    if (above == block || (is_water && (above == B_WATER || above == B_WATER_FLOW)) ||
        (!is_water && (above == B_LAVA || above == B_LAVA_FLOW)))
        h = 16;
    else if (FERN_META(st) > 0 && FERN_META(st) < 8)
        h = 14 - FERN_META(st) * 12 / 8;
    for (face = 0; face < 6; face++) {
        FernState n = at(c, dir[face][0], dir[face][1], dir[face][2]);
        int nb = FERN_BLOCK(n);
        int b[6] = {0, 0, 0, 16, h, 16};
        if (fern_block_info(nb)->flags & BF_OPAQUE) continue;
        if (is_water && (nb == B_WATER || nb == B_WATER_FLOW)) continue;
        if (!is_water && (nb == B_LAVA || nb == B_LAVA_FLOW)) continue;
        if (face == 0 && nb == B_ICE) continue;
        box_face(c, l, face, b, fern_block_texture(st, face), tint, face_light(c, face == 1 && h < 16 ? 1 : face));
    }
}

static void add_block(Ctx *c, FernState st) {
    int block = FERN_BLOCK(st), meta = FERN_META(st);
    const FernBlockInfo *info = fern_block_info(block);
    FernVertexList *l = (info->flags & BF_TRANSLUCENT) ? &c->out->water : &c->out->solid;
    switch (info->shape) {
    case SHAPE_NONE:
        return;
    case SHAPE_CUBE: {
        const int b[6] = {0, 0, 0, 16, 16, 16};
        if (block == B_FARMLAND) {
            const int f[6] = {0, 0, 0, 16, 15, 16};
            add_box(c, l, block, st, f, 1);
            return;
        }
        add_box(c, l, block, st, b, 1);
        return;
    }
    case SHAPE_CROSS:
        add_cross(c, l, block, st, block == B_REEDS ? 0 : 2);
        return;
    case SHAPE_CROPS:
        add_crops(c, l, st);
        return;
    case SHAPE_LIQUID:
        add_liquid(c, block == B_LAVA || block == B_LAVA_FLOW ? &c->out->solid : &c->out->water, block, st);
        return;
    case SHAPE_FLAT:
        add_flat(c, l, block, st);
        return;
    case SHAPE_SLAB: {
        int b[6] = {0, 0, 0, 16, 8, 16};
        if (block == B_BED) b[4] = 9;
        if (block == B_CAKE) { b[0] = 1; b[2] = 1; b[3] = 15; b[5] = 15; }
        add_box(c, l, block, st, b, 1);
        return;
    }
    case SHAPE_SNOW: {
        int b[6] = {0, 0, 0, 16, 2 * ((meta & 7) + 1), 16};
        add_box(c, l, block, st, b, 1);
        return;
    }
    case SHAPE_STAIRS: {
        int low[6] = {0, 0, 0, 16, 8, 16}, high[6] = {0, 8, 0, 16, 16, 16};
        switch (meta & 3) {
        case 0: high[0] = 8; break;
        case 1: high[3] = 8; break;
        case 2: high[2] = 8; break;
        default: high[5] = 8; break;
        }
        add_box(c, l, block, st, low, 1);
        add_box(c, l, block, st, high, 1);
        return;
    }
    case SHAPE_FENCE: {
        const int post[6] = {6, 0, 6, 10, 16, 10};
        static const int arm[4][3] = {{-1, 0, 0}, {1, 0, 0}, {0, 0, -1}, {0, 0, 1}};
        int i;
        add_box(c, l, block, st, post, 0);
        for (i = 0; i < 4; i++) {
            int nb = FERN_BLOCK(at(c, arm[i][0], 0, arm[i][2]));
            if (nb == B_FENCE || (fern_block_info(nb)->flags & BF_OPAQUE)) {
                int bar[6] = {7, 12, 7, 9, 15, 9}, bar2[6] = {7, 6, 7, 9, 9, 9};
                if (arm[i][0] < 0) { bar[0] = 0; bar2[0] = 0; }
                if (arm[i][0] > 0) { bar[3] = 16; bar2[3] = 16; }
                if (arm[i][2] < 0) { bar[2] = 0; bar2[2] = 0; }
                if (arm[i][2] > 0) { bar[5] = 16; bar2[5] = 16; }
                add_box(c, l, block, st, bar, 0);
                add_box(c, l, block, st, bar2, 0);
            }
        }
        return;
    }
    case SHAPE_CACTUS: {
        const int b[6] = {1, 0, 1, 15, 16, 15};
        add_box(c, l, block, st, b, 1);
        return;
    }
    case SHAPE_TORCH: {
        int b[6] = {7, 0, 7, 9, 10, 9};
        if (block == B_SIGN_POST) {
            const int board[6] = {0, 8, 7, 16, 16, 9}, stick[6] = {7, 0, 7, 9, 8, 9};
            add_box(c, l, block, st, board, 0);
            add_box(c, l, block, st, stick, 0);
            return;
        }
        // A torch on a wall leans against it. Draw it upright near the wall.
        switch (meta) {
        case 1: b[0] = 0; b[3] = 2; b[1] = 3; b[4] = 13; break;
        case 2: b[0] = 14; b[3] = 16; b[1] = 3; b[4] = 13; break;
        case 3: b[2] = 0; b[5] = 2; b[1] = 3; b[4] = 13; break;
        case 4: b[2] = 14; b[5] = 16; b[1] = 3; b[4] = 13; break;
        default: break;
        }
        {
            // Use the middle of the tile for each side, as the torch texture has.
            int face;
            uint8_t tint[3] = {255, 255, 255};
            for (face = 0; face < 6; face++) {
                int p[6];
                memcpy(p, b, sizeof(p));
                box_face(c, l, face, p, fern_block_texture(st, face), tint, light_at(c, 0, 0, 0) | 0x0F);
            }
        }
        return;
    }
    case SHAPE_DOOR: {
        int d = meta & 3, b[6] = {0, 0, 0, 16, 16, 16};
        if (meta & 4) d = (d + 1) & 3;
        switch (d) {
        case 0: b[3] = 3; break;
        case 1: b[5] = 3; break;
        case 2: b[0] = 13; break;
        default: b[2] = 13; break;
        }
        add_box(c, l, block, st, b, 0);
        return;
    }
    case SHAPE_LADDER: {
        // The meta is the face of the wall: 2 north, 3 south, 4 west, 5 east.
        int b[6] = {0, 0, 0, 16, 16, 16};
        switch (meta) {
        case 2: b[2] = 15; break;
        case 3: b[5] = 1; break;
        case 4: b[0] = 15; break;
        default: b[3] = 1; break;
        }
        if (block == B_SIGN_WALL) {
            b[1] = 4;
            b[4] = 12;
        }
        add_box(c, l, block, st, b, 0);
        return;
    }
    default: {
        const int b[6] = {0, 0, 0, 16, 16, 16};
        add_box(c, l, block, st, b, 1);
        return;
    }
    }
}

void fern_mesh_build(const FernMeshInput *in, FernMeshOutput *out) {
    Ctx c;
    out->sx = in->sx;
    out->sy = in->sy;
    out->sz = in->sz;
    out->solid.count = 0;
    out->water.count = 0;
    c.in = in;
    c.out = out;
    for (c.y = 0; c.y < 16; c.y++)
        for (c.z = 0; c.z < 16; c.z++)
            for (c.x = 0; c.x < 16; c.x++) {
                FernState st = in->state[AT(c.x, c.y, c.z)];
                if (st) add_block(&c, st);
            }
}
