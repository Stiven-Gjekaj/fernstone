#include "world.h"
#include <stdlib.h>
#include <string.h>

void fern_world_init(FernWorld *w) {
    memset(w, 0, sizeof(*w));
}

static unsigned hash(int cx, int cz) {
    return ((unsigned)cx * 73856093u ^ (unsigned)cz * 19349663u) % FERN_WORLD_BUCKETS;
}

static void free_column(FernColumn *c) {
    int i;
    for (i = 0; i < FERN_SECTIONS; i++) free(c->sec[i]);
    free(c);
}

void fern_world_clear(FernWorld *w) {
    int i;
    for (i = 0; i < FERN_WORLD_BUCKETS; i++) {
        FernColumn *c = w->bucket[i];
        while (c) {
            FernColumn *next = c->next;
            free_column(c);
            c = next;
        }
        w->bucket[i] = NULL;
    }
    w->columns = 0;
}

FernColumn *fern_world_column(FernWorld *w, int cx, int cz) {
    FernColumn *c = w->bucket[hash(cx, cz)];
    while (c && (c->x != cx || c->z != cz)) c = c->next;
    return c;
}

FernColumn *fern_world_column_make(FernWorld *w, int cx, int cz) {
    FernColumn *c = fern_world_column(w, cx, cz);
    unsigned h;
    if (c) return c;
    c = calloc(1, sizeof(*c));
    c->x = cx;
    c->z = cz;
    c->dirty = (1u << FERN_SECTIONS) - 1;
    h = hash(cx, cz);
    c->next = w->bucket[h];
    w->bucket[h] = c;
    w->columns++;
    return c;
}

void fern_world_unload(FernWorld *w, int cx, int cz) {
    FernColumn **p = &w->bucket[hash(cx, cz)];
    while (*p) {
        if ((*p)->x == cx && (*p)->z == cz) {
            FernColumn *c = *p;
            *p = c->next;
            free_column(c);
            w->columns--;
            return;
        }
        p = &(*p)->next;
    }
}

static FernSection *section_at(FernWorld *w, int x, int y, int z, FernColumn **col) {
    FernColumn *c;
    if (y < 0 || y >= FERN_HEIGHT) return NULL;
    c = fern_world_column(w, x >> 4, z >> 4);
    if (col) *col = c;
    return c ? c->sec[y >> 4] : NULL;
}

#define INDEX(x, y, z) (((y) & 15) << 8 | ((z) & 15) << 4 | ((x) & 15))

FernState fern_world_get(FernWorld *w, int x, int y, int z) {
    FernSection *s = section_at(w, x, y, z, NULL);
    return s ? s->state[INDEX(x, y, z)] : 0;
}

void fern_world_mark(FernWorld *w, int x, int y, int z) {
    static const int d[7][3] = {{0, 0, 0}, {1, 0, 0}, {-1, 0, 0}, {0, 1, 0}, {0, -1, 0}, {0, 0, 1}, {0, 0, -1}};
    int i;
    for (i = 0; i < 7; i++) {
        int bx = x + d[i][0], by = y + d[i][1], bz = z + d[i][2];
        FernColumn *c;
        if (by < 0 || by >= FERN_HEIGHT) continue;
        c = fern_world_column(w, bx >> 4, bz >> 4);
        if (c) {
            c->dirty |= 1u << (by >> 4);
            c->version++;
        }
    }
}

void fern_world_set(FernWorld *w, int x, int y, int z, FernState st) {
    FernColumn *c = NULL;
    FernSection *s;
    int i;
    if (y < 0 || y >= FERN_HEIGHT) return;
    c = fern_world_column(w, x >> 4, z >> 4);
    if (!c) return;
    s = c->sec[y >> 4];
    if (!s) {
        if (st == 0) return;
        s = calloc(1, sizeof(*s));
        // A new section in the open has full sky light.
        memset(s->light, 0xF0, sizeof(s->light));
        c->sec[y >> 4] = s;
    }
    i = INDEX(x, y, z);
    if (s->state[i] == 0 && st != 0) s->solid++;
    if (s->state[i] != 0 && st == 0) s->solid--;
    s->state[i] = st;
    // The server sends no light change. Give a block that the change opens
    // the light of its neighbors, as the client of Beta does in short form:
    // full sky light under open sky, else the brightest neighbor less one.
    if (!(fern_block_info(FERN_BLOCK(st))->flags & BF_OPAQUE)) {
        static const int d[6][3] = {{1, 0, 0}, {-1, 0, 0}, {0, 1, 0}, {0, -1, 0}, {0, 0, 1}, {0, 0, -1}};
        int k, sky = 0, blk = fern_block_info(FERN_BLOCK(st))->light;
        for (k = 0; k < 6; k++) {
            int ns = fern_world_sky(w, x + d[k][0], y + d[k][1], z + d[k][2]);
            int nb = fern_world_light(w, x + d[k][0], y + d[k][1], z + d[k][2]);
            if (k == 2 && ns == 15) ns = 16;
            if (ns - 1 > sky) sky = ns - 1;
            if (nb - 1 > blk) blk = nb - 1;
        }
        s->light[i] = (uint8_t)(sky << 4 | blk);
    } else {
        s->light[i] = 0;
    }
    fern_world_mark(w, x, y, z);
}

int fern_world_sky(FernWorld *w, int x, int y, int z) {
    FernColumn *c = NULL;
    FernSection *s;
    if (y >= FERN_HEIGHT) return 15;
    if (y < 0) return 0;
    s = section_at(w, x, y, z, &c);
    if (!s) return c ? 15 : 15;
    return s->light[INDEX(x, y, z)] >> 4;
}

int fern_world_light(FernWorld *w, int x, int y, int z) {
    FernSection *s = section_at(w, x, y, z, NULL);
    return s ? s->light[INDEX(x, y, z)] & 15 : 0;
}
