#include "terrain.h"
#include <math.h>
#include <pthread.h>
#include <stdlib.h>
#include <string.h>
#include "gfx.h"
#include "log.h"
#include "mesh.h"

#define SLOTS 8192
#define JOBS 6
#define MAX_QUADS 16384

typedef struct {
    int used;
    int sx, sy, sz;
    GLuint vbo;
    int solid, water;          // Vertex counts.
    int busy;                  // A job for this section is in the worker.
    int stale;                 // The world changed while the job was busy.
} Slot;

typedef struct {
    int state;                 // 0 free, 1 queued, 2 done
    FernMeshInput in;
    FernMeshOutput out;
} Job;

struct FernTerrain {
    Slot slot[SLOTS];
    GLuint program, ibo, atlas;
    GLint u_mvp, u_origin, u_day, u_fog, u_fogc, u_alpha, u_tex;
    Job job[JOBS];
    pthread_t thread;
    pthread_mutex_t lock;
    pthread_cond_t wake;
    int quit;
    int drawn;
    int queued;
};

static const char *vs =
    "attribute vec3 a_pos;\n"
    "attribute vec2 a_uv;\n"
    "attribute vec4 a_color;\n"
    "uniform mat4 u_mvp;\n"
    "uniform vec3 u_origin;\n"
    "uniform float u_day;\n"
    "varying vec2 v_uv;\n"
    "varying vec3 v_color;\n"
    "varying float v_dist;\n"
    "float bright(float l) {\n"
    "    float f = 1.0 - l / 15.0;\n"
    "    return (1.0 - f) / (f * 3.0 + 1.0) * 0.95 + 0.05;\n"
    "}\n"
    "void main() {\n"
    "    vec3 p = a_pos / 16.0 + u_origin;\n"
    "    gl_Position = u_mvp * vec4(p, 1.0);\n"
    "    v_uv = a_uv;\n"
    "    float lv = floor(a_color.a * 255.0 + 0.5);\n"
    "    float sky = floor(lv / 16.0);\n"
    "    float blk = lv - sky * 16.0;\n"
    "    float b = max(bright(sky) * u_day, bright(blk));\n"
    "    v_color = a_color.rgb * b;\n"
    "    v_dist = length(p.xz);\n"
    "}\n";

static const char *fs =
    "precision mediump float;\n"
    "uniform sampler2D u_tex;\n"
    "uniform vec2 u_fog;\n"
    "uniform vec3 u_fogc;\n"
    "uniform float u_alpha;\n"
    "varying vec2 v_uv;\n"
    "varying vec3 v_color;\n"
    "varying float v_dist;\n"
    "void main() {\n"
    "    vec4 t = texture2D(u_tex, v_uv);\n"
    "    if (t.a < 0.5 && u_alpha > 0.99) discard;\n"
    "    float fog = clamp((v_dist - u_fog.x) / (u_fog.y - u_fog.x), 0.0, 1.0);\n"
    "    gl_FragColor = vec4(mix(t.rgb * v_color, u_fogc, fog), u_alpha < 0.99 ? u_alpha : 1.0);\n"
    "}\n";

static void *worker(void *arg) {
    FernTerrain *t = arg;
    for (;;) {
        Job *job = NULL;
        int i;
        pthread_mutex_lock(&t->lock);
        while (!t->quit) {
            for (i = 0; i < JOBS; i++)
                if (t->job[i].state == 1) {
                    job = &t->job[i];
                    break;
                }
            if (job) break;
            pthread_cond_wait(&t->wake, &t->lock);
        }
        pthread_mutex_unlock(&t->lock);
        if (t->quit) return NULL;
        fern_mesh_build(&job->in, &job->out);
        pthread_mutex_lock(&t->lock);
        job->state = 2;
        pthread_mutex_unlock(&t->lock);
    }
}

FernTerrain *fern_terrain_new(void) {
    static const char *attribs[] = {"a_pos", "a_uv", "a_color", NULL};
    FernTerrain *t = calloc(1, sizeof(*t));
    GLushort *idx = malloc(sizeof(GLushort) * MAX_QUADS * 6);
    int i;
    t->program = fern_gl_program(vs, fs, attribs);
    t->u_mvp = glGetUniformLocation(t->program, "u_mvp");
    t->u_origin = glGetUniformLocation(t->program, "u_origin");
    t->u_day = glGetUniformLocation(t->program, "u_day");
    t->u_fog = glGetUniformLocation(t->program, "u_fog");
    t->u_fogc = glGetUniformLocation(t->program, "u_fogc");
    t->u_alpha = glGetUniformLocation(t->program, "u_alpha");
    t->u_tex = glGetUniformLocation(t->program, "u_tex");
    // One index buffer for each quad list: 0 1 2, 0 2 3.
    for (i = 0; i < MAX_QUADS; i++) {
        idx[i * 6 + 0] = (GLushort)(i * 4);
        idx[i * 6 + 1] = (GLushort)(i * 4 + 1);
        idx[i * 6 + 2] = (GLushort)(i * 4 + 2);
        idx[i * 6 + 3] = (GLushort)(i * 4);
        idx[i * 6 + 4] = (GLushort)(i * 4 + 2);
        idx[i * 6 + 5] = (GLushort)(i * 4 + 3);
    }
    glGenBuffers(1, &t->ibo);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, t->ibo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(GLushort) * MAX_QUADS * 6, idx, GL_STATIC_DRAW);
    free(idx);
    pthread_mutex_init(&t->lock, NULL);
    pthread_cond_init(&t->wake, NULL);
    pthread_create(&t->thread, NULL, worker, t);
    return t;
}

void fern_terrain_set_atlas(FernTerrain *t, GLuint atlas) {
    t->atlas = atlas;
}

static unsigned slot_hash(int x, int y, int z) {
    return ((unsigned)x * 73856093u ^ (unsigned)y * 19349663u ^ (unsigned)z * 83492791u) % SLOTS;
}

static Slot *find(FernTerrain *t, int x, int y, int z, int make) {
    unsigned h = slot_hash(x, y, z), i;
    Slot *free_slot = NULL;
    for (i = 0; i < SLOTS; i++) {
        Slot *s = &t->slot[(h + i) % SLOTS];
        if (s->used && s->sx == x && s->sy == y && s->sz == z) return s;
        if (!s->used) {
            if (!free_slot) free_slot = s;
            if (!s->vbo && !s->busy) break;
        }
    }
    if (!make || !free_slot) return NULL;
    memset(free_slot, 0, sizeof(*free_slot));
    free_slot->used = 1;
    free_slot->sx = x;
    free_slot->sy = y;
    free_slot->sz = z;
    return free_slot;
}

static void drop(Slot *s) {
    if (s->vbo) glDeleteBuffers(1, &s->vbo);
    s->vbo = 0;
    s->solid = s->water = 0;
    s->used = 0;
}

static void upload(FernTerrain *t, Job *job) {
    Slot *s = find(t, job->out.sx, job->out.sy, job->out.sz, 0);
    int n;
    if (!s) return;
    s->busy = 0;
    n = job->out.solid.count + job->out.water.count;
    if (n == 0) {
        if (s->vbo) glDeleteBuffers(1, &s->vbo);
        s->vbo = 0;
        s->solid = s->water = 0;
        return;
    }
    if (!s->vbo) glGenBuffers(1, &s->vbo);
    glBindBuffer(GL_ARRAY_BUFFER, s->vbo);
    glBufferData(GL_ARRAY_BUFFER, (GLsizeiptr)(sizeof(FernVertex) * n), NULL, GL_STATIC_DRAW);
    glBufferSubData(GL_ARRAY_BUFFER, 0, (GLsizeiptr)(sizeof(FernVertex) * job->out.solid.count), job->out.solid.v);
    glBufferSubData(GL_ARRAY_BUFFER, (GLintptr)(sizeof(FernVertex) * job->out.solid.count),
                    (GLsizeiptr)(sizeof(FernVertex) * job->out.water.count), job->out.water.v);
    s->solid = job->out.solid.count;
    s->water = job->out.water.count;
}

typedef struct {
    int x, y, z, d;
} Want;

static int cmp_want(const void *a, const void *b) {
    return ((const Want *)a)->d - ((const Want *)b)->d;
}

void fern_terrain_update(FernTerrain *t, FernWorld *w, double cx, double cy, double cz, int radius) {
    static Want want[(2 * 16 + 1) * (2 * 16 + 1) * FERN_SECTIONS];
    int i, n = 0, x, z, y, free_jobs = 0;
    int ccx = (int)floor(cx) >> 4, ccz = (int)floor(cz) >> 4, ccy = (int)floor(cy) >> 4;
    if (radius > 16) radius = 16;
    // Take the finished jobs.
    pthread_mutex_lock(&t->lock);
    for (i = 0; i < JOBS; i++)
        if (t->job[i].state == 2) {
            Slot *s;
            pthread_mutex_unlock(&t->lock);
            upload(t, &t->job[i]);
            s = find(t, t->job[i].out.sx, t->job[i].out.sy, t->job[i].out.sz, 0);
            if (s && s->stale) {
                FernColumn *c = fern_world_column(w, s->sx, s->sz);
                s->stale = 0;
                if (c) c->dirty |= 1u << s->sy;
            }
            pthread_mutex_lock(&t->lock);
            t->job[i].state = 0;
        }
    for (i = 0; i < JOBS; i++)
        if (t->job[i].state == 0) free_jobs++;
    pthread_mutex_unlock(&t->lock);

    // Drop the meshes that are far away or in an unloaded column.
    for (i = 0; i < SLOTS; i++) {
        Slot *s = &t->slot[i];
        if (!s->used || s->busy) continue;
        if (abs(s->sx - ccx) > radius + 1 || abs(s->sz - ccz) > radius + 1 || !fern_world_column(w, s->sx, s->sz)) drop(s);
    }

    // Find the dirty sections in range, nearest first.
    for (x = ccx - radius; x <= ccx + radius; x++)
        for (z = ccz - radius; z <= ccz + radius; z++) {
            FernColumn *c = fern_world_column(w, x, z);
            if (!c || !c->dirty) continue;
            // Wait for the four neighbors, so that the edge faces are right.
            if (!fern_world_column(w, x + 1, z) || !fern_world_column(w, x - 1, z) ||
                !fern_world_column(w, x, z + 1) || !fern_world_column(w, x, z - 1))
                continue;
            for (y = 0; y < FERN_SECTIONS; y++)
                if (c->dirty & (1u << y)) {
                    want[n].x = x;
                    want[n].y = y;
                    want[n].z = z;
                    want[n].d = (x - ccx) * (x - ccx) + (z - ccz) * (z - ccz) + (y - ccy) * (y - ccy) / 4;
                    n++;
                }
        }
    qsort(want, (size_t)n, sizeof(Want), cmp_want);
    t->queued = n;
    for (i = 0; i < n && free_jobs > 0; i++) {
        FernColumn *c = fern_world_column(w, want[i].x, want[i].z);
        Slot *s = find(t, want[i].x, want[i].y, want[i].z, 1);
        int j;
        if (!s) break;
        if (s->busy) {
            s->stale = 1;
            c->dirty &= ~(1u << want[i].y);
            continue;
        }
        c->dirty &= ~(1u << want[i].y);
        for (j = 0; j < JOBS; j++)
            if (t->job[j].state == 0) break;
        if (!fern_mesh_snapshot(w, want[i].x, want[i].y, want[i].z, &t->job[j].in)) {
            // An empty section has no mesh.
            if (s->vbo) glDeleteBuffers(1, &s->vbo);
            s->vbo = 0;
            s->solid = s->water = 0;
            continue;
        }
        s->busy = 1;
        pthread_mutex_lock(&t->lock);
        t->job[j].state = 1;
        pthread_cond_signal(&t->wake);
        pthread_mutex_unlock(&t->lock);
        free_jobs--;
    }
}

// Give 1 if a box of 16 blocks may be in the view.
static int visible(const FernMat *m, float x, float y, float z) {
    int plane, k;
    for (plane = 0; plane < 6; plane++) {
        int axis = plane >> 1;
        float sign = (plane & 1) ? -1.0f : 1.0f;
        int all_out = 1;
        for (k = 0; k < 8; k++) {
            float px = x + ((k & 1) ? 16 : 0), py = y + ((k & 2) ? 16 : 0), pz = z + ((k & 4) ? 16 : 0);
            float cw = m->m[3] * px + m->m[7] * py + m->m[11] * pz + m->m[15];
            float ca = m->m[axis] * px + m->m[4 + axis] * py + m->m[8 + axis] * pz + m->m[12 + axis];
            if (sign * ca <= cw) {
                all_out = 0;
                break;
            }
        }
        if (all_out) return 0;
    }
    return 1;
}

static void bind_slot(Slot *s, int first) {
    glBindBuffer(GL_ARRAY_BUFFER, s->vbo);
    glVertexAttribPointer(0, 3, GL_SHORT, GL_FALSE, sizeof(FernVertex), (void *)(sizeof(FernVertex) * (size_t)first));
    glVertexAttribPointer(1, 2, GL_UNSIGNED_SHORT, GL_TRUE, sizeof(FernVertex), (void *)(sizeof(FernVertex) * (size_t)first + 8));
    glVertexAttribPointer(2, 4, GL_UNSIGNED_BYTE, GL_TRUE, sizeof(FernVertex), (void *)(sizeof(FernVertex) * (size_t)first + 12));
}

typedef struct {
    Slot *s;
    float d;
} Far;

static int cmp_far(const void *a, const void *b) {
    float d = ((const Far *)b)->d - ((const Far *)a)->d;
    return d > 0 ? 1 : d < 0 ? -1 : 0;
}

void fern_terrain_draw(FernTerrain *t, const FernMat *vp, double cx, double cy, double cz, const FernTerrainLook *look) {
    static Far water[SLOTS];
    int i, nw = 0;
    t->drawn = 0;
    glUseProgram(t->program);
    glUniformMatrix4fv(t->u_mvp, 1, GL_FALSE, vp->m);
    glUniform1f(t->u_day, look->day);
    glUniform2f(t->u_fog, look->fog_start, look->fog_end);
    glUniform3f(t->u_fogc, look->fog[0], look->fog[1], look->fog[2]);
    glUniform1f(t->u_alpha, 1.0f);
    glUniform1i(t->u_tex, 0);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, t->atlas);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, t->ibo);
    glEnableVertexAttribArray(0);
    glEnableVertexAttribArray(1);
    glEnableVertexAttribArray(2);
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE);
    glDisable(GL_BLEND);
    for (i = 0; i < SLOTS; i++) {
        Slot *s = &t->slot[i];
        float ox, oy, oz;
        if (!s->used || !s->vbo) continue;
        // The camera is at the origin. This keeps the floats exact far away.
        ox = (float)(s->sx * 16 - cx);
        oy = (float)(s->sy * 16 - cy);
        oz = (float)(s->sz * 16 - cz);
        if (!visible(vp, ox, oy, oz)) continue;
        if (s->water) {
            water[nw].s = s;
            water[nw].d = (ox + 8) * (ox + 8) + (oy + 8) * (oy + 8) + (oz + 8) * (oz + 8);
            nw++;
        }
        if (!s->solid) continue;
        glUniform3f(t->u_origin, ox, oy, oz);
        bind_slot(s, 0);
        glDrawElements(GL_TRIANGLES, (s->solid / 4 < MAX_QUADS ? s->solid / 4 : MAX_QUADS) * 6, GL_UNSIGNED_SHORT, 0);
        t->drawn++;
    }
    // The translucent pass: far to near, with no depth writes.
    qsort(water, (size_t)nw, sizeof(Far), cmp_far);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDepthMask(GL_FALSE);
    glDisable(GL_CULL_FACE);
    glUniform1f(t->u_alpha, 0.7f);
    for (i = 0; i < nw; i++) {
        Slot *s = water[i].s;
        glUniform3f(t->u_origin, (float)(s->sx * 16 - cx), (float)(s->sy * 16 - cy), (float)(s->sz * 16 - cz));
        bind_slot(s, s->solid);
        glDrawElements(GL_TRIANGLES, (s->water / 4 < MAX_QUADS ? s->water / 4 : MAX_QUADS) * 6, GL_UNSIGNED_SHORT, 0);
    }
    glDepthMask(GL_TRUE);
    glDisable(GL_BLEND);
    glDisableVertexAttribArray(2);
}

void fern_terrain_draw_quads(FernTerrain *t, const FernMat *vp, GLuint tex, float ox, float oy, float oz,
                             const void *vertices, int count, const FernTerrainLook *look) {
    static GLuint vbo;
    if (count <= 0) return;
    if (!vbo) glGenBuffers(1, &vbo);
    glUseProgram(t->program);
    glUniformMatrix4fv(t->u_mvp, 1, GL_FALSE, vp->m);
    glUniform1f(t->u_day, look->day);
    glUniform2f(t->u_fog, look->fog_start, look->fog_end);
    glUniform3f(t->u_fogc, look->fog[0], look->fog[1], look->fog[2]);
    glUniform1f(t->u_alpha, 1.0f);
    glUniform3f(t->u_origin, ox, oy, oz);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, tex);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, (GLsizeiptr)(sizeof(FernVertex) * (size_t)count), vertices, GL_STREAM_DRAW);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, t->ibo);
    glEnableVertexAttribArray(0);
    glEnableVertexAttribArray(1);
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(0, 3, GL_SHORT, GL_FALSE, sizeof(FernVertex), (void *)0);
    glVertexAttribPointer(1, 2, GL_UNSIGNED_SHORT, GL_TRUE, sizeof(FernVertex), (void *)8);
    glVertexAttribPointer(2, 4, GL_UNSIGNED_BYTE, GL_TRUE, sizeof(FernVertex), (void *)12);
    glEnable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);
    glDisable(GL_BLEND);
    glDrawElements(GL_TRIANGLES, (count / 4) * 6, GL_UNSIGNED_SHORT, 0);
    glDisableVertexAttribArray(2);
}

void fern_terrain_clear(FernTerrain *t) {
    int i;
    // Wait for the worker to finish its jobs, then drop all.
    for (;;) {
        int busy = 0;
        pthread_mutex_lock(&t->lock);
        for (i = 0; i < JOBS; i++) {
            if (t->job[i].state == 1) busy = 1;
            if (t->job[i].state == 2) t->job[i].state = 0;
        }
        pthread_mutex_unlock(&t->lock);
        if (!busy) break;
    }
    for (i = 0; i < SLOTS; i++) {
        if (t->slot[i].used) drop(&t->slot[i]);
        t->slot[i].busy = 0;
    }
}

void fern_terrain_stats(FernTerrain *t, int *meshes, int *drawn, int *queued) {
    int i, n = 0;
    for (i = 0; i < SLOTS; i++)
        if (t->slot[i].used && t->slot[i].vbo) n++;
    *meshes = n;
    *drawn = t->drawn;
    *queued = t->queued;
}

void fern_terrain_free(FernTerrain *t) {
    int i;
    fern_terrain_clear(t);
    pthread_mutex_lock(&t->lock);
    t->quit = 1;
    pthread_cond_signal(&t->wake);
    pthread_mutex_unlock(&t->lock);
    pthread_join(t->thread, NULL);
    for (i = 0; i < JOBS; i++) fern_mesh_free(&t->job[i].out);
    glDeleteBuffers(1, &t->ibo);
    glDeleteProgram(t->program);
    free(t);
}
