// model.h: the box models of the players, the mobs, and the dropped items.
// The quads use the vertex format of the terrain, in world blocks * 16
// from an origin that the caller gives.
#ifndef FERN_MODEL_H
#define FERN_MODEL_H

#include "mesh.h"

// A player with a 64 x 32 skin. walk is the phase of the legs, swing the
// arm swing (0 to 1). yaw and pitch are in degrees.
void fern_model_player(FernVertexList *l, float yaw, float pitch, float walk, float swing, int crouch,
                       uint8_t light);
// A plain box for a mob: a body of a size in blocks, with one color.
void fern_model_mob(FernVertexList *l, float yaw, float w, float h, uint8_t r, uint8_t g, uint8_t b,
                    uint8_t light);
// A small block of the terrain atlas, for a dropped item.
void fern_model_item(FernVertexList *l, int tile, float spin, uint8_t light);

#endif
