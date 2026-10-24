// entity.h: the entities that the server shows: players, mobs, items, and
// objects. Positions are in blocks. The renderer moves each entity from its
// last position to its new one over a few frames.
#ifndef FERN_ENTITY_H
#define FERN_ENTITY_H

#include <stdint.h>

typedef enum { ENT_PLAYER, ENT_MOB, ENT_ITEM, ENT_OBJECT } FernEntityKind;

typedef struct {
    int32_t id;
    uint8_t used;
    uint8_t kind;
    uint8_t type;           // The mob or object type of the protocol.
    uint8_t crouch;
    char name[32];
    int16_t item;           // The item in the hand, or the item of a drop.
    double x, y, z;         // The position that the server sent last.
    double px, py, pz;      // The position that the renderer shows now.
    float yaw, pitch;       // Degrees.
    float swing;            // The arm swing, from 1 down to 0.
    int32_t fx, fy, fz;     // The fixed-point position (1/32 block).
} FernEntity;

#define FERN_ENTITY_MAX 512

typedef struct {
    FernEntity e[FERN_ENTITY_MAX];
} FernEntities;

void fern_entities_clear(FernEntities *t);
// Find an entity, or give NULL.
FernEntity *fern_entity_get(FernEntities *t, int32_t id);
// Add an entity, or replace the one with the same ID.
FernEntity *fern_entity_add(FernEntities *t, int32_t id, FernEntityKind kind);
void fern_entity_remove(FernEntities *t, int32_t id);
// Set the fixed-point position (1/32 block), or move it by a delta.
void fern_entity_place(FernEntity *e, int32_t fx, int32_t fy, int32_t fz);
void fern_entity_move(FernEntity *e, int dx, int dy, int dz);
// Move the shown position toward the server position. t is the part of
// the distance to cover, 0 to 1.
void fern_entities_step(FernEntities *t, double part);

#endif
