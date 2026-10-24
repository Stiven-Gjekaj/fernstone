#include "entity.h"
#include <string.h>

void fern_entities_clear(FernEntities *t) {
    memset(t, 0, sizeof(*t));
}

static int slot_of(int32_t id) {
    return (int)((uint32_t)id * 2654435761u % FERN_ENTITY_MAX);
}

FernEntity *fern_entity_get(FernEntities *t, int32_t id) {
    int i, s = slot_of(id);
    for (i = 0; i < FERN_ENTITY_MAX; i++) {
        FernEntity *e = &t->e[(s + i) % FERN_ENTITY_MAX];
        if (e->used && e->id == id) return e;
    }
    return NULL;
}

FernEntity *fern_entity_add(FernEntities *t, int32_t id, FernEntityKind kind) {
    FernEntity *e = fern_entity_get(t, id);
    int i, s = slot_of(id);
    for (i = 0; !e && i < FERN_ENTITY_MAX; i++) {
        FernEntity *f = &t->e[(s + i) % FERN_ENTITY_MAX];
        if (!f->used) e = f;
    }
    if (!e) return NULL;
    memset(e, 0, sizeof(*e));
    e->used = 1;
    e->id = id;
    e->kind = (uint8_t)kind;
    e->item = -1;
    return e;
}

void fern_entity_remove(FernEntities *t, int32_t id) {
    FernEntity *e = fern_entity_get(t, id);
    if (e) e->used = 0;
}

void fern_entity_place(FernEntity *e, int32_t fx, int32_t fy, int32_t fz) {
    int first = e->fx == 0 && e->fy == 0 && e->fz == 0 && e->px == 0 && e->py == 0 && e->pz == 0;
    e->fx = fx;
    e->fy = fy;
    e->fz = fz;
    e->x = fx / 32.0;
    e->y = fy / 32.0;
    e->z = fz / 32.0;
    if (first) {
        e->px = e->x;
        e->py = e->y;
        e->pz = e->z;
    }
}

void fern_entity_move(FernEntity *e, int dx, int dy, int dz) {
    fern_entity_place(e, e->fx + dx, e->fy + dy, e->fz + dz);
}

void fern_entities_step(FernEntities *t, double part) {
    int i;
    for (i = 0; i < FERN_ENTITY_MAX; i++) {
        FernEntity *e = &t->e[i];
        if (!e->used) continue;
        e->px += (e->x - e->px) * part;
        e->py += (e->y - e->py) * part;
        e->pz += (e->z - e->pz) * part;
        if (e->swing > 0) e->swing -= (float)part;
    }
}
