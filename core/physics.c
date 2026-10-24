#include "physics.h"
#include <math.h>

static FernBox box(double x0, double y0, double z0, double x1, double y1, double z1) {
    FernBox b = {x0, y0, z0, x1, y1, z1};
    return b;
}

int fern_block_boxes(FernState s, int x, int y, int z, FernBox out[2]) {
    int block = FERN_BLOCK(s), meta = FERN_META(s);
    const FernBlockInfo *info = fern_block_info(block);
    if (!(info->flags & BF_SOLID)) return 0;
    switch (block) {
    case B_SLAB:
        out[0] = box(x, y, z, x + 1, y + 0.5, z + 1);
        return 1;
    case B_BED:
        out[0] = box(x, y, z, x + 1, y + 0.5625, z + 1);
        return 1;
    case B_CAKE:
        out[0] = box(x + 0.0625, y, z + 0.0625, x + 0.9375, y + 0.5, z + 0.9375);
        return 1;
    case B_SOULSAND:
        out[0] = box(x, y, z, x + 1, y + 0.875, z + 1);
        return 1;
    case B_CACTUS:
        out[0] = box(x + 0.0625, y, z + 0.0625, x + 0.9375, y + 0.9375, z + 0.9375);
        return 1;
    case B_FENCE:
        out[0] = box(x, y, z, x + 1, y + 1.5, z + 1);
        return 1;
    case B_TRAPDOOR:
        if (meta & 4) {
            switch (meta & 3) {
            case 0: out[0] = box(x, y, z + 0.8125, x + 1, y + 1, z + 1); break;
            case 1: out[0] = box(x, y, z, x + 1, y + 1, z + 0.1875); break;
            case 2: out[0] = box(x + 0.8125, y, z, x + 1, y + 1, z + 1); break;
            default: out[0] = box(x, y, z, x + 0.1875, y + 1, z + 1); break;
            }
        } else {
            out[0] = box(x, y, z, x + 1, y + 0.1875, z + 1);
        }
        return 1;
    case B_DOOR_WOOD: case B_DOOR_IRON: {
        // The meta of the lower half has the direction and the open bit.
        // The world model keeps the upper half separately, so use its own.
        int dir = meta & 3;
        double t = 0.1875;
        if (meta & 4) dir = (dir + 1) & 3;
        switch (dir) {
        case 0: out[0] = box(x, y, z, x + t, y + 1, z + 1); break;
        case 1: out[0] = box(x, y, z, x + 1, y + 1, z + t); break;
        case 2: out[0] = box(x + 1 - t, y, z, x + 1, y + 1, z + 1); break;
        default: out[0] = box(x, y, z + 1 - t, x + 1, y + 1, z + 1); break;
        }
        return 1;
    }
    case B_WOOD_STAIRS: case B_COBBLE_STAIRS:
        out[0] = box(x, y, z, x + 1, y + 0.5, z + 1);
        switch (meta & 3) {
        case 0: out[1] = box(x + 0.5, y + 0.5, z, x + 1, y + 1, z + 1); break;
        case 1: out[1] = box(x, y + 0.5, z, x + 0.5, y + 1, z + 1); break;
        case 2: out[1] = box(x, y + 0.5, z + 0.5, x + 1, y + 1, z + 1); break;
        default: out[1] = box(x, y + 0.5, z, x + 1, y + 1, z + 0.5); break;
        }
        return 2;
    default:
        out[0] = box(x, y, z, x + 1, y + 1, z + 1);
        return 1;
    }
}

static int overlap(const FernBox *a, const FernBox *b) {
    return a->x1 > b->x0 && a->x0 < b->x1 && a->y1 > b->y0 && a->y0 < b->y1 && a->z1 > b->z0 && a->z0 < b->z1;
}

int fern_box_hits(FernWorld *w, const FernBox *b) {
    int x, y, z, i, n;
    FernBox boxes[2];
    for (x = (int)floor(b->x0); x <= (int)floor(b->x1); x++)
        for (y = (int)floor(b->y0) - 1; y <= (int)floor(b->y1); y++)
            for (z = (int)floor(b->z0); z <= (int)floor(b->z1); z++) {
                n = fern_block_boxes(fern_world_get(w, x, y, z), x, y, z, boxes);
                for (i = 0; i < n; i++)
                    if (overlap(b, &boxes[i])) return 1;
            }
    return 0;
}

// Gather the boxes of the blocks around a region. At most 256.
static int gather(FernWorld *w, const FernBox *r, FernBox *out, int max) {
    int x, y, z, i, n, count = 0;
    FernBox boxes[2];
    for (x = (int)floor(r->x0); x <= (int)floor(r->x1); x++)
        for (y = (int)floor(r->y0) - 1; y <= (int)floor(r->y1); y++)
            for (z = (int)floor(r->z0); z <= (int)floor(r->z1); z++) {
                n = fern_block_boxes(fern_world_get(w, x, y, z), x, y, z, boxes);
                for (i = 0; i < n && count < max; i++) out[count++] = boxes[i];
            }
    return count;
}

// Clip a move on one axis against a box, as in AxisAlignedBB of Beta.
static double clip_y(const FernBox *me, const FernBox *b, double d) {
    if (me->x1 <= b->x0 || me->x0 >= b->x1 || me->z1 <= b->z0 || me->z0 >= b->z1) return d;
    if (d > 0 && me->y1 <= b->y0 && b->y0 - me->y1 < d) d = b->y0 - me->y1;
    if (d < 0 && me->y0 >= b->y1 && b->y1 - me->y0 > d) d = b->y1 - me->y0;
    return d;
}

static double clip_x(const FernBox *me, const FernBox *b, double d) {
    if (me->y1 <= b->y0 || me->y0 >= b->y1 || me->z1 <= b->z0 || me->z0 >= b->z1) return d;
    if (d > 0 && me->x1 <= b->x0 && b->x0 - me->x1 < d) d = b->x0 - me->x1;
    if (d < 0 && me->x0 >= b->x1 && b->x1 - me->x0 > d) d = b->x1 - me->x0;
    return d;
}

static double clip_z(const FernBox *me, const FernBox *b, double d) {
    if (me->y1 <= b->y0 || me->y0 >= b->y1 || me->x1 <= b->x0 || me->x0 >= b->x1) return d;
    if (d > 0 && me->z1 <= b->z0 && b->z0 - me->z1 < d) d = b->z0 - me->z1;
    if (d < 0 && me->z0 >= b->z1 && b->z1 - me->z0 > d) d = b->z1 - me->z0;
    return d;
}

static FernBox body_box(const FernBody *b) {
    double h = FERN_PLAYER_WIDTH / 2;
    return box(b->x - h, b->y, b->z - h, b->x + h, b->y + FERN_PLAYER_HEIGHT, b->z + h);
}

static void offset(FernBox *b, double dx, double dy, double dz) {
    b->x0 += dx; b->x1 += dx;
    b->y0 += dy; b->y1 += dy;
    b->z0 += dz; b->z1 += dz;
}

// Move the box by dx, dy, dz with collision. Write the move that happened.
static void collide(FernWorld *w, FernBox *me, double *dx, double *dy, double *dz) {
    FernBox boxes[256], region = *me;
    int n, i;
    if (*dx < 0) region.x0 += *dx; else region.x1 += *dx;
    if (*dy < 0) region.y0 += *dy; else region.y1 += *dy;
    if (*dz < 0) region.z0 += *dz; else region.z1 += *dz;
    n = gather(w, &region, boxes, 256);
    for (i = 0; i < n; i++) *dy = clip_y(me, &boxes[i], *dy);
    offset(me, 0, *dy, 0);
    for (i = 0; i < n; i++) *dx = clip_x(me, &boxes[i], *dx);
    offset(me, *dx, 0, 0);
    for (i = 0; i < n; i++) *dz = clip_z(me, &boxes[i], *dz);
    offset(me, 0, 0, *dz);
}

static void move(FernBody *b, FernWorld *w, double dx, double dy, double dz) {
    FernBox me = body_box(b), start = me;
    double ox = dx, oy = dy, oz = dz;
    double ax = dx, ay = dy, az = dz;
    collide(w, &me, &ax, &ay, &az);
    // Step up 0.5 when a wall stops a move on the ground.
    if ((b->on_ground || (oy != ay && oy < 0)) && (ax != ox || az != oz)) {
        FernBox up = start;
        double sx = ox, sy = 0.5, sz = oz;
        collide(w, &up, &sx, &sy, &sz);
        if (sx * sx + sz * sz > ax * ax + az * az) {
            // Come back down onto the step.
            double down = -sy;
            FernBox boxes[256], region = up;
            int n, i;
            region.y0 += down;
            n = gather(w, &region, boxes, 256);
            for (i = 0; i < n; i++) down = clip_y(&up, &boxes[i], down);
            offset(&up, 0, down, 0);
            me = up;
            ax = sx;
            az = sz;
            ay = up.y0 - start.y0;
        }
    }
    b->x = (me.x0 + me.x1) / 2;
    b->y = me.y0;
    b->z = (me.z0 + me.z1) / 2;
    b->collided_h = ax != ox || az != oz;
    b->on_ground = oy != ay && oy < 0;
    if (ax != ox) b->mx = 0;
    if (ay != oy) b->my = 0;
    if (az != oz) b->mz = 0;
}

static void move_flying(FernBody *b, float strafe, float forward, double speed) {
    double d = sqrt((double)strafe * strafe + (double)forward * forward);
    double s, c;
    if (d < 0.01) return;
    if (d < 1) d = 1;
    d = speed / d;
    s = sin(b->yaw * M_PI / 180.0);
    c = cos(b->yaw * M_PI / 180.0);
    b->mx += strafe * d * c - forward * d * s;
    b->mz += forward * d * c + strafe * d * s;
}

static int box_in_liquid(FernWorld *w, const FernBox *me) {
    int x, y, z;
    for (x = (int)floor(me->x0); x <= (int)floor(me->x1); x++)
        for (y = (int)floor(me->y0); y <= (int)floor(me->y1); y++)
            for (z = (int)floor(me->z0); z <= (int)floor(me->z1); z++)
                if (fern_block_info(FERN_BLOCK(fern_world_get(w, x, y, z)))->flags & BF_LIQUID) return 1;
    return 0;
}

static int body_in_liquid(FernBody *b, FernWorld *w) {
    FernBox me = body_box(b);
    int x, y, z;
    me.y0 += 0.4;
    me.y1 -= 0.4;
    for (x = (int)floor(me.x0 + 0.001); x <= (int)floor(me.x1 - 0.001); x++)
        for (y = (int)floor(me.y0); y <= (int)floor(me.y1); y++)
            for (z = (int)floor(me.z0 + 0.001); z <= (int)floor(me.z1 - 0.001); z++)
                if (fern_block_info(FERN_BLOCK(fern_world_get(w, x, y, z)))->flags & BF_LIQUID) return 1;
    return 0;
}

static int on_ladder(FernBody *b, FernWorld *w) {
    int block = FERN_BLOCK(fern_world_get(w, (int)floor(b->x), (int)floor(b->y), (int)floor(b->z)));
    return block == B_LADDER;
}

void fern_body_tick(FernBody *b, FernWorld *w, const FernMoveInput *in) {
    float forward = in->forward * 0.98f, strafe = in->strafe * 0.98f;
    if (in->sneak) {
        forward *= 0.3f;
        strafe *= 0.3f;
    }
    if (b->fly) {
        move_flying(b, strafe, forward, 0.1);
        b->my = (in->jump ? 0.3 : 0) - (in->sneak ? 0.3 : 0);
        move(b, w, b->mx, b->my, b->mz);
        b->mx *= 0.6;
        b->mz *= 0.6;
        return;
    }
    b->in_water = body_in_liquid(b, w);
    if (b->in_water) {
        double oy = b->y;
        if (in->jump) b->my += 0.04;
        move_flying(b, strafe, forward, 0.02);
        move(b, w, b->mx, b->my, b->mz);
        b->mx *= 0.8;
        b->my *= 0.8;
        b->mz *= 0.8;
        b->my -= 0.02;
        // A body that swims into a wall climbs out when the place above the
        // edge is free, as in Beta.
        if (b->collided_h) {
            FernBox o = body_box(b);
            offset(&o, b->mx, b->my + 0.6 - b->y + oy, b->mz);
            if (!fern_box_hits(w, &o) && !box_in_liquid(w, &o)) b->my = 0.3;
        }
        return;
    }
    if (in->jump && b->on_ground) b->my = 0.42;
    {
        double friction = 0.91;
        double speed;
        if (b->on_ground) {
            int below = FERN_BLOCK(fern_world_get(w, (int)floor(b->x), (int)floor(b->y) - 1, (int)floor(b->z)));
            friction = (below == B_ICE ? 0.98 : 0.6) * 0.91;
        }
        speed = b->on_ground ? 0.1 * (0.16277136 / (friction * friction * friction)) : 0.02;
        move_flying(b, strafe, forward, speed);
        if (on_ladder(b, w)) {
            if (b->mx > 0.15) b->mx = 0.15;
            if (b->mx < -0.15) b->mx = -0.15;
            if (b->mz > 0.15) b->mz = 0.15;
            if (b->mz < -0.15) b->mz = -0.15;
            if (b->my < -0.15) b->my = -0.15;
            if (in->sneak && b->my < 0) b->my = 0;
        }
        move(b, w, b->mx, b->my, b->mz);
        if (b->collided_h && on_ladder(b, w)) b->my = 0.2;
        b->my -= 0.08;
        b->my *= 0.98;
        b->mx *= friction;
        b->mz *= friction;
    }
}

FernRayHit fern_raycast(FernWorld *w, double ox, double oy, double oz, double dx, double dy, double dz,
                        double max) {
    FernRayHit hit = {0, 0, 0, 0, 0, 0};
    int x = (int)floor(ox), y = (int)floor(oy), z = (int)floor(oz);
    int sx = dx > 0 ? 1 : -1, sy = dy > 0 ? 1 : -1, sz = dz > 0 ? 1 : -1;
    double tx = dx != 0 ? ((sx > 0 ? x + 1 - ox : ox - x) / fabs(dx)) : 1e30;
    double ty = dy != 0 ? ((sy > 0 ? y + 1 - oy : oy - y) / fabs(dy)) : 1e30;
    double tz = dz != 0 ? ((sz > 0 ? z + 1 - oz : oz - z) / fabs(dz)) : 1e30;
    double ddx = dx != 0 ? 1 / fabs(dx) : 1e30, ddy = dy != 0 ? 1 / fabs(dy) : 1e30;
    double ddz = dz != 0 ? 1 / fabs(dz) : 1e30;
    double t = 0;
    int face = -1;
    while (t <= max) {
        int block = FERN_BLOCK(fern_world_get(w, x, y, z));
        const FernBlockInfo *info = fern_block_info(block);
        if (block != B_AIR && !(info->flags & BF_LIQUID) && face >= 0) {
            hit.hit = 1;
            hit.x = x;
            hit.y = y;
            hit.z = z;
            hit.face = face;
            hit.dist = t;
            return hit;
        }
        if (tx < ty && tx < tz) {
            x += sx;
            t = tx;
            tx += ddx;
            face = sx > 0 ? 4 : 5;
        } else if (ty < tz) {
            y += sy;
            t = ty;
            ty += ddy;
            face = sy > 0 ? 0 : 1;
        } else {
            z += sz;
            t = tz;
            tz += ddz;
            face = sz > 0 ? 2 : 3;
        }
    }
    return hit;
}
