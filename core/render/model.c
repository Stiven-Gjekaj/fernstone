#include "model.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>

static FernVertex *add(FernVertexList *l, int n) {
    if (l->count + n > l->cap) {
        l->cap = l->cap ? l->cap * 2 : 512;
        while (l->cap < l->count + n) l->cap *= 2;
        l->v = realloc(l->v, sizeof(FernVertex) * (size_t)l->cap);
    }
    l->count += n;
    return l->v + l->count - n;
}

typedef struct {
    float m[3][3];  // A rotation.
    float t[3];     // A translation, in blocks.
} Pose;

static Pose pose_yaw_pitch(float yaw_deg, float pitch_deg, float tx, float ty, float tz) {
    Pose p;
    float y = yaw_deg * (float)M_PI / 180.0f, x = pitch_deg * (float)M_PI / 180.0f;
    float cy = cosf(y), sy = sinf(y), cx = cosf(x), sx = sinf(x);
    // The rotation around y, then around x.
    p.m[0][0] = cy;  p.m[0][1] = sy * sx;  p.m[0][2] = sy * cx;
    p.m[1][0] = 0;   p.m[1][1] = cx;       p.m[1][2] = -sx;
    p.m[2][0] = -sy; p.m[2][1] = cy * sx;  p.m[2][2] = cy * cx;
    p.t[0] = tx;
    p.t[1] = ty;
    p.t[2] = tz;
    return p;
}

static void apply(const Pose *p, const float in[3], float out[3]) {
    int i;
    for (i = 0; i < 3; i++) out[i] = p->m[i][0] * in[0] + p->m[i][1] * in[1] + p->m[i][2] * in[2] + p->t[i];
}

// A box with a skin layout of the Beta model: at (u, v) in the texture, with
// a size of w x h x d pixels. The box goes from x0 to x0 + w (in pixels,
// 1/16 block) around the pivot of the pose. tw, th: the texture size.
static void skin_box(FernVertexList *l, const Pose *pose, float x0, float y0, float z0, int w, int h, int d, int u,
                     int v, int tw, int th, uint8_t light, float shade_scale) {
    static const float sh[6] = {0.5f, 1.0f, 0.8f, 0.8f, 0.6f, 0.6f};
    float x1 = x0 + w, y1 = y0 + h, z1 = z0 + d;
    // The faces: corners in pixels and the texture rectangle of each face.
    float c[6][4][3] = {
        {{x0, y0, z1}, {x1, y0, z1}, {x1, y0, z0}, {x0, y0, z0}},  // down
        {{x0, y1, z0}, {x1, y1, z0}, {x1, y1, z1}, {x0, y1, z1}},  // up
        {{x1, y0, z0}, {x0, y0, z0}, {x0, y1, z0}, {x1, y1, z0}},  // back (-z)
        {{x0, y0, z1}, {x1, y0, z1}, {x1, y1, z1}, {x0, y1, z1}},  // front (+z)
        {{x0, y0, z0}, {x0, y0, z1}, {x0, y1, z1}, {x0, y1, z0}},  // right (-x)
        {{x1, y0, z1}, {x1, y0, z0}, {x1, y1, z0}, {x1, y1, z1}},  // left (+x)
    };
    int r[6][4] = {
        {u + d + w, v, u + d + w + w, v + d},
        {u + d, v, u + d + w, v + d},
        {u + d + w + d, v + d, u + d + w + d + w, v + d + h},
        {u + d, v + d, u + d + w, v + d + h},
        {u, v + d, u + d, v + d + h},
        {u + d + w, v + d, u + d + w + d, v + d + h},
    };
    int f, i;
    for (f = 0; f < 6; f++) {
        FernVertex *q = add(l, 4);
        float cu[4] = {(float)r[f][0], (float)r[f][2], (float)r[f][2], (float)r[f][0]};
        float cv[4] = {(float)r[f][3], (float)r[f][3], (float)r[f][1], (float)r[f][1]};
        for (i = 0; i < 4; i++) {
            float in[3] = {c[f][i][0] / 16.0f, c[f][i][1] / 16.0f, c[f][i][2] / 16.0f}, out[3];
            apply(pose, in, out);
            q[i].x = (int16_t)lroundf(out[0] * 16.0f);
            q[i].y = (int16_t)lroundf(out[1] * 16.0f);
            q[i].z = (int16_t)lroundf(out[2] * 16.0f);
            q[i].pad = 0;
            q[i].u = (uint16_t)(cu[i] / tw * 65535.0f);
            q[i].v = (uint16_t)(cv[i] / th * 65535.0f);
            q[i].r = q[i].g = q[i].b = (uint8_t)(255 * sh[f] * shade_scale);
            q[i].light = light;
        }
    }
}

// Join two poses: the parent, then the child.
static Pose join(const Pose *a, const Pose *b) {
    Pose r;
    int i, j;
    for (i = 0; i < 3; i++) {
        for (j = 0; j < 3; j++) r.m[i][j] = a->m[i][0] * b->m[0][j] + a->m[i][1] * b->m[1][j] + a->m[i][2] * b->m[2][j];
        r.t[i] = a->m[i][0] * b->t[0] + a->m[i][1] * b->t[1] + a->m[i][2] * b->t[2] + a->t[i];
    }
    return r;
}

void fern_model_player(FernVertexList *l, float yaw, float pitch, float walk, float swing, int crouch,
                       uint8_t light) {
    // The model faces +z at yaw 0, as the protocol yaw does.
    Pose body = pose_yaw_pitch(-yaw, 0, 0, 0, 0), part, p;
    float leg = sinf(walk) * 0.8f * 57.3f;
    float arm_r = -leg - swing * 80.0f;
    float lift = crouch ? -1.0f / 16 : 0;
    // The head turns with the pitch, around the neck at 24 px.
    part = pose_yaw_pitch(0, pitch, 0, 24.0f / 16 + lift, 0);
    p = join(&body, &part);
    skin_box(l, &p, -4, 0, -4, 8, 8, 8, 0, 0, 64, 32, light, 1.0f);
    // The body.
    part = pose_yaw_pitch(0, 0, 0, 12.0f / 16 + lift, 0);
    p = join(&body, &part);
    skin_box(l, &p, -4, 0, -2, 8, 12, 4, 16, 16, 64, 32, light, 1.0f);
    // The arms swing around the shoulders.
    part = pose_yaw_pitch(0, arm_r, -6.0f / 16, 22.0f / 16 + lift, 0);
    p = join(&body, &part);
    skin_box(l, &p, -2, -10, -2, 4, 12, 4, 40, 16, 64, 32, light, 1.0f);
    part = pose_yaw_pitch(0, leg, 6.0f / 16, 22.0f / 16 + lift, 0);
    p = join(&body, &part);
    skin_box(l, &p, -2, -10, -2, 4, 12, 4, 40, 16, 64, 32, light, 1.0f);
    // The legs swing around the hips.
    part = pose_yaw_pitch(0, leg, -2.0f / 16, 12.0f / 16, 0);
    p = join(&body, &part);
    skin_box(l, &p, -2, -12, -2, 4, 12, 4, 0, 16, 64, 32, light, 1.0f);
    part = pose_yaw_pitch(0, -leg, 2.0f / 16, 12.0f / 16, 0);
    p = join(&body, &part);
    skin_box(l, &p, -2, -12, -2, 4, 12, 4, 0, 16, 64, 32, light, 1.0f);
}

void fern_model_mob(FernVertexList *l, float yaw, float w, float h, uint8_t r, uint8_t g, uint8_t b,
                    uint8_t light) {
    Pose p = pose_yaw_pitch(-yaw, 0, 0, 0, 0);
    int start = l->count, i;
    int pw = (int)(w * 16), ph = (int)(h * 16);
    // Use a plain white part of the skin: the texture is the white texture.
    skin_box(l, &p, -pw / 2.0f, 0, -pw / 2.0f, pw, ph, pw, 0, 0, 64, 32, light, 1.0f);
    for (i = start; i < l->count; i++) {
        l->v[i].r = (uint8_t)(l->v[i].r * r / 255);
        l->v[i].g = (uint8_t)(l->v[i].g * g / 255);
        l->v[i].b = (uint8_t)(l->v[i].b * b / 255);
        l->v[i].u = 0;
        l->v[i].v = 0;
    }
}

void fern_model_item(FernVertexList *l, int tile, float spin, uint8_t light) {
    Pose p = pose_yaw_pitch(spin, 0, 0, 0, 0);
    int start = l->count, i;
    skin_box(l, &p, -2, 0, -2, 4, 4, 4, 0, 0, 16, 16, light, 1.0f);
    // Map each face to the whole tile of the atlas.
    for (i = start; i < l->count; i += 4) {
        float u0 = (tile & 15) / 16.0f, v0 = (tile >> 4) / 16.0f, s = 1.0f / 16;
        l->v[i].u = (uint16_t)(u0 * 65535);         l->v[i].v = (uint16_t)((v0 + s) * 65535);
        l->v[i + 1].u = (uint16_t)((u0 + s) * 65535); l->v[i + 1].v = (uint16_t)((v0 + s) * 65535);
        l->v[i + 2].u = (uint16_t)((u0 + s) * 65535); l->v[i + 2].v = (uint16_t)(v0 * 65535);
        l->v[i + 3].u = (uint16_t)(u0 * 65535);       l->v[i + 3].v = (uint16_t)(v0 * 65535);
    }
}
