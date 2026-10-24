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
// The growth of the next boxes on each side, in pixels: the wool of a sheep
// is a larger copy of its body. The texture stays the same.
static float grow;

static void skin_box(FernVertexList *l, const Pose *pose, float x0, float y0, float z0, int w, int h, int d, int u,
                     int v, int tw, int th, uint8_t light, float shade_scale) {
    static const float sh[6] = {0.5f, 1.0f, 0.8f, 0.8f, 0.6f, 0.6f};
    float x1 = x0 + w + grow, y1 = y0 + h + grow, z1 = z0 + d + grow;
    x0 -= grow;
    y0 -= grow;
    z0 -= grow;
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

// One box of a Beta model. The numbers are those of the ModelRenderer of
// Beta: a box from (x, y, z) with a size of w x h x d pixels, around a
// pivot (px, py, pz), turned by pitch degrees around x. Beta has y down,
// the ground at y = 24, and the front toward -z. This model has y up and the
// front toward +z, so the box turns over.
static void beta_part(FernVertexList *l, const Pose *body, int u, int v, float x, float y, float z, int w, int h,
                      int d, float px, float py, float pz, float pitch, uint8_t light) {
    Pose part = pose_yaw_pitch(0, pitch, px / 16.0f, (24 - py) / 16.0f, -pz / 16.0f);
    Pose p = join(body, &part);
    skin_box(l, &p, x, -(y + h), -(z + d), w, h, d, u, v, 64, 32, light, 1.0f);
}

static void biped(FernVertexList *l, const Pose *b, float swing, int arms_forward, int thin, uint8_t light) {
    float arm = arms_forward ? -90.0f : -swing;
    int t = thin ? 2 : 4;
    beta_part(l, b, 0, 0, -4, -8, -4, 8, 8, 8, 0, 0, 0, 0, light);
    beta_part(l, b, 16, 16, -4, 0, -2, 8, 12, 4, 0, 0, 0, 0, light);
    beta_part(l, b, 40, 16, thin ? -1 : -3, -2, -t / 2.0f, t, 12, t, -5, 2, 0, arm, light);
    beta_part(l, b, 40, 16, -1, -2, -t / 2.0f, t, 12, t, 5, 2, 0, arms_forward ? -90.0f : swing, light);
    beta_part(l, b, 0, 16, -t / 2.0f, 0, -t / 2.0f, t, 12, t, -2, 12, 0, swing, light);
    beta_part(l, b, 0, 16, -t / 2.0f, 0, -t / 2.0f, t, 12, t, 2, 12, 0, -swing, light);
}

static void quad_legs(FernVertexList *l, const Pose *b, int u, int v, int size, int height, float x, float y,
                      float zf, float zb, float swing, uint8_t light) {
    beta_part(l, b, u, v, -size / 2.0f, 0, -size / 2.0f, size, height, size, -x, y, zb, swing, light);
    beta_part(l, b, u, v, -size / 2.0f, 0, -size / 2.0f, size, height, size, x, y, zb, -swing, light);
    beta_part(l, b, u, v, -size / 2.0f, 0, -size / 2.0f, size, height, size, -x, y, zf, -swing, light);
    beta_part(l, b, u, v, -size / 2.0f, 0, -size / 2.0f, size, height, size, x, y, zf, swing, light);
}

int fern_model_mob_beta(FernVertexList *l, int type, float yaw, float pitch, float walk, uint8_t light) {
    Pose b = pose_yaw_pitch(-yaw, 0, 0, 0, 0);
    float swing = sinf(walk) * 40.0f;
    int i;
    (void)pitch;
    switch (type) {
    case 51: // skeleton
        biped(l, &b, swing, 0, 1, light);
        return 1;
    case 54: case 57: // zombie, zombie pigman
        biped(l, &b, swing, 1, 0, light);
        return 1;
    case 50: // creeper
        beta_part(l, &b, 0, 0, -4, -8, -4, 8, 8, 8, 0, 4, 0, 0, light);
        beta_part(l, &b, 16, 16, -4, 0, -2, 8, 12, 4, 0, 4, 0, 0, light);
        quad_legs(l, &b, 0, 16, 4, 6, 2, 16, -4, 4, swing, light);
        return 1;
    case 90: // pig
        beta_part(l, &b, 0, 0, -4, -4, -8, 8, 8, 8, 0, 12, -6, 0, light);
        beta_part(l, &b, 16, 16, -2, 0, -9, 4, 3, 1, 0, 12, -6, 0, light);
        beta_part(l, &b, 28, 8, -5, -10, -7, 10, 16, 8, 0, 11, 2, 90, light);
        quad_legs(l, &b, 0, 16, 4, 6, 3, 18, -5, 7, swing, light);
        return 1;
    case 92: // cow
        beta_part(l, &b, 0, 0, -4, -4, -6, 8, 8, 6, 0, 4, -8, 0, light);
        beta_part(l, &b, 22, 0, -5, -5, -4, 1, 3, 1, 0, 3, -7, 0, light);
        beta_part(l, &b, 22, 0, 4, -5, -4, 1, 3, 1, 0, 3, -7, 0, light);
        beta_part(l, &b, 18, 4, -6, -10, -7, 12, 18, 10, 0, 5, 2, 90, light);
        quad_legs(l, &b, 0, 16, 4, 12, 4, 12, -6, 7, swing, light);
        return 1;
    case 91: // sheep
        beta_part(l, &b, 0, 0, -3, -4, -6, 6, 6, 8, 0, 6, -8, 0, light);
        beta_part(l, &b, 28, 8, -4, -10, -7, 8, 16, 6, 0, 5, 2, 90, light);
        quad_legs(l, &b, 0, 16, 4, 12, 3, 12, -5, 7, swing, light);
        return 1;
    case 93: // chicken
        beta_part(l, &b, 0, 0, -2, -6, -2, 4, 6, 3, 0, 15, -4, 0, light);
        beta_part(l, &b, 14, 0, -2, -4, -4, 4, 2, 2, 0, 15, -4, 0, light);
        beta_part(l, &b, 14, 4, -1, -2, -3, 2, 2, 2, 0, 15, -4, 0, light);
        beta_part(l, &b, 0, 9, -3, -4, -3, 6, 8, 6, 0, 16, 0, 90, light);
        beta_part(l, &b, 26, 0, -1, 0, -3, 3, 5, 3, -2, 19, 1, swing, light);
        beta_part(l, &b, 26, 0, -1, 0, -3, 3, 5, 3, 1, 19, 1, -swing, light);
        beta_part(l, &b, 24, 13, 0, 0, -3, 1, 4, 6, -4, 13, 0, 0, light);
        beta_part(l, &b, 24, 13, -1, 0, -3, 1, 4, 6, 4, 13, 0, 0, light);
        return 1;
    case 52: // spider: the head, the neck, the body, and eight legs
        beta_part(l, &b, 32, 4, -4, -4, -8, 8, 8, 8, 0, 15, -3, 0, light);
        beta_part(l, &b, 0, 0, -3, -3, -3, 6, 6, 6, 0, 15, 0, 0, light);
        beta_part(l, &b, 0, 12, -5, -4, -6, 10, 8, 12, 0, 15, 9, 0, light);
        for (i = 0; i < 4; i++) {
            float z = -1.0f + i * 1.0f, spread = (float)(i - 1.5) * 20.0f + swing * 0.3f;
            Pose leg = pose_yaw_pitch(spread, 0, 0, 0, 0), pl = join(&b, &leg);
            beta_part(l, &pl, 18, 0, -15, -1, -1, 16, 2, 2, -4, 15, z, 0, light);
            leg = pose_yaw_pitch(-spread, 0, 0, 0, 0);
            pl = join(&b, &leg);
            beta_part(l, &pl, 18, 0, -1, -1, -1, 16, 2, 2, 4, 15, z, 0, light);
        }
        return 1;
    case 95: // wolf
        beta_part(l, &b, 0, 0, -3, -3, -2, 6, 6, 4, -1, 13.5f, -7, 0, light);
        beta_part(l, &b, 0, 10, -2, 0, -5, 3, 3, 4, -0.5f, 13.5f, -7, 0, light);
        beta_part(l, &b, 18, 14, -4, -2, -3, 6, 9, 6, 0, 14, 2, 90, light);
        beta_part(l, &b, 21, 0, -4, -3, -3, 8, 6, 7, -1, 14, 2, 90, light);
        quad_legs(l, &b, 0, 18, 2, 8, 1.5f, 16, -4, 7, swing, light);
        beta_part(l, &b, 9, 18, -1, 0, -1, 2, 8, 2, -1, 12, 8, 50, light);
        return 1;
    case 55: // slime: the outer cube
        beta_part(l, &b, 0, 0, -4, 16, -4, 8, 8, 8, 0, 0, 0, 0, light);
        return 1;
    case 94: // squid: the body and the tentacles
        beta_part(l, &b, 0, 0, -6, -8, -6, 12, 16, 12, 0, 0, 0, 0, light);
        for (i = 0; i < 8; i++) {
            float a = i * 45.0f * (float)M_PI / 180.0f;
            beta_part(l, &b, 48, 0, -1, 0, -1, 2, 18, 2, cosf(a) * 5, 8, sinf(a) * 5, swing * 0.3f, light);
        }
        return 1;
    default:
        return 0;
    }
}

void fern_model_sheep_fur(FernVertexList *l, float yaw, float walk, uint8_t r, uint8_t g, uint8_t b,
                          uint8_t light) {
    Pose body = pose_yaw_pitch(-yaw, 0, 0, 0, 0);
    float swing = sinf(walk) * 40.0f;
    int start = l->count, i;
    // The numbers of ModelSheep1 of Beta.
    grow = 0.6f;
    beta_part(l, &body, 0, 0, -3, -4, -4, 6, 6, 6, 0, 6, -8, 0, light);
    grow = 1.75f;
    beta_part(l, &body, 28, 8, -4, -10, -7, 8, 16, 6, 0, 5, 2, 90, light);
    grow = 0.5f;
    quad_legs(l, &body, 0, 16, 4, 6, 3, 12, -5, 7, swing, light);
    grow = 0;
    for (i = start; i < l->count; i++) {
        l->v[i].r = (uint8_t)(l->v[i].r * r / 255);
        l->v[i].g = (uint8_t)(l->v[i].g * g / 255);
        l->v[i].b = (uint8_t)(l->v[i].b * b / 255);
    }
}
