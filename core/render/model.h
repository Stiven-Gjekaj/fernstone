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
// A mob with the box model of Beta and the texture of the jar (64 x 32).
// type is the mob type of the protocol (50 creeper to 95 wolf). walk is the
// phase of the legs. Give 0 for a type that has no model.
int fern_model_mob_beta(FernVertexList *l, int type, float yaw, float pitch, float walk, uint8_t light);
// The wool of a sheep (mob/sheep_fur.png), with the color of the wool.
void fern_model_sheep_fur(FernVertexList *l, float yaw, float walk, uint8_t r, uint8_t g, uint8_t b,
                          uint8_t light);
// A flat icon (16 x 16 pixels of a 256 x 256 sheet) that turns to face the
// camera, for a dropped item that is not a block.
void fern_model_sprite(FernVertexList *l, int icon, float camera_yaw, uint8_t light);
// A plain box for a mob: a body of a size in blocks, with one color.
void fern_model_mob(FernVertexList *l, float yaw, float w, float h, uint8_t r, uint8_t g, uint8_t b,
                    uint8_t light);
// A small block of the terrain atlas, for a dropped item.
void fern_model_item(FernVertexList *l, int tile, float spin, uint8_t light);

#endif
