// physics.h: the movement of the player, with the numbers of Beta 1.7.3.
// One step is one game tick (1/20 s).
#ifndef FERN_PHYSICS_H
#define FERN_PHYSICS_H

#include "world.h"

typedef struct {
    double x0, y0, z0, x1, y1, z1;
} FernBox;

typedef struct {
    double x, y, z;              // The feet, in the middle of the box.
    double mx, my, mz;           // The motion in blocks for each tick.
    float yaw, pitch;            // Degrees, as the protocol sends them.
    int on_ground;
    int collided_h;
    int in_water;
    int fly;                     // A test mode with no gravity. Not sent to the server.
} FernBody;

typedef struct {
    float forward, strafe;       // -1 to 1.
    int jump, sneak;
} FernMoveInput;

#define FERN_PLAYER_WIDTH 0.6
#define FERN_PLAYER_HEIGHT 1.8
#define FERN_EYE_HEIGHT 1.62

// The collision boxes of a block at x, y, z. Give the count (0 to 2).
int fern_block_boxes(FernState s, int x, int y, int z, FernBox out[2]);
// Move the body one tick, with the input, against the blocks of the world.
void fern_body_tick(FernBody *b, FernWorld *w, const FernMoveInput *in);
// Give 1 if the box hits a solid block.
int fern_box_hits(FernWorld *w, const FernBox *box);

typedef struct {
    int hit;
    int x, y, z;                 // The block that the ray hits.
    int face;                    // The face of the block (0 to 5, as in blocks.h).
    double dist;
} FernRayHit;

// Cast a ray from an eye position along a direction, up to max blocks.
// Liquids and air do not stop it.
FernRayHit fern_raycast(FernWorld *w, double ox, double oy, double oz, double dx, double dy, double dz,
                        double max);

#endif
