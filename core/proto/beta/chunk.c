#include "chunk.h"
#include <stdlib.h>
#include <string.h>
#include <zlib.h>

static int nibble(const uint8_t *a, int i) {
    return (i & 1) ? a[i >> 1] >> 4 : a[i >> 1] & 15;
}

int fern_chunk_apply(FernWorld *w, const FernVersion *v, const FernPacket *p) {
    int sx = p->u.chunk.sx, sy = p->u.chunk.sy, sz = p->u.chunk.sz;
    int n = sx * sy * sz;
    size_t need = (size_t)n * 5 / 2;
    uLongf got = (uLongf)need;
    uint8_t *buf = malloc(need);
    const uint8_t *ids, *metas, *blight, *slight;
    int ix, iy, iz, i, rc;
    rc = uncompress(buf, &got, p->u.chunk.data, (uLong)p->u.chunk.size);
    if (rc != Z_OK || got < need) {
        free(buf);
        return 0;
    }
    ids = buf;
    metas = buf + n;
    blight = metas + n / 2;
    slight = blight + n / 2;
    // The data goes in x, then z, then y order, with y the fastest.
    i = 0;
    for (ix = 0; ix < sx; ix++) {
        for (iz = 0; iz < sz; iz++) {
            int x = p->u.chunk.x + ix, z = p->u.chunk.z + iz;
            FernColumn *c = fern_world_column_make(w, x >> 4, z >> 4);
            for (iy = 0; iy < sy; iy++, i++) {
                int y = p->u.chunk.y + iy;
                int block = fern_block_from_wire(v->block_table, ids[i]);
                FernState st = FERN_STATE(block, nibble(metas, i));
                FernSection *s;
                int k;
                if (y < 0 || y >= FERN_HEIGHT) continue;
                s = c->sec[y >> 4];
                if (!s) {
                    if (st == 0 && nibble(slight, i) == 15 && nibble(blight, i) == 0) continue;
                    s = calloc(1, sizeof(*s));
                    memset(s->light, 0xF0, sizeof(s->light));
                    c->sec[y >> 4] = s;
                }
                k = (y & 15) << 8 | (z & 15) << 4 | (x & 15);
                if (s->state[k] == 0 && st != 0) s->solid++;
                if (s->state[k] != 0 && st == 0) s->solid--;
                s->state[k] = st;
                s->light[k] = (uint8_t)(nibble(slight, i) << 4 | nibble(blight, i));
            }
            c->version++;
        }
    }
    // Mark the box and the sections around it, so that the faces on the
    // edges get a new mesh.
    for (ix = -16; ix <= sx + 15; ix += 16)
        for (iz = -16; iz <= sz + 15; iz += 16)
            for (iy = -16; iy <= sy + 15; iy += 16) {
                int x = p->u.chunk.x + (ix < sx ? ix : sx), z = p->u.chunk.z + (iz < sz ? iz : sz);
                int y = p->u.chunk.y + (iy < sy ? iy : sy);
                FernColumn *c = fern_world_column(w, x >> 4, z >> 4);
                if (c && y >= 0 && y < FERN_HEIGHT) c->dirty |= 1u << (y >> 4);
            }
    free(buf);
    return 1;
}

void fern_chunk_apply_multi(FernWorld *w, const FernVersion *v, const FernPacket *p) {
    int i;
    for (i = 0; i < p->u.multi.count; i++) {
        int c = p->u.multi.coords[i * 2] << 8 | p->u.multi.coords[i * 2 + 1];
        int x = p->u.multi.cx * 16 + (c >> 12 & 15);
        int z = p->u.multi.cz * 16 + (c >> 8 & 15);
        int y = c & 255;
        int block = fern_block_from_wire(v->block_table, p->u.multi.types[i]);
        fern_world_set(w, x, y, z, FERN_STATE(block, p->u.multi.metas[i]));
    }
}

void fern_chunk_apply_block(FernWorld *w, const FernVersion *v, const FernPacket *p) {
    int block = fern_block_from_wire(v->block_table, p->u.block.type);
    fern_world_set(w, p->u.block.x, p->u.block.y, p->u.block.z, FERN_STATE(block, p->u.block.meta));
}
