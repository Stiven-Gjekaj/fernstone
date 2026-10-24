#include "client.h"
#include <math.h>
#include <stdio.h>
#include <string.h>
#include "chunk.h"
#include "dig.h"
#include "log.h"

#define TICK (1.0 / 20.0)
#define REACH 4.5

void fern_client_init(FernClient *c) {
    memset(c, 0, sizeof(*c));
    fern_net_init(&c->net);
    fern_writer_init(&c->out);
    fern_world_init(&c->world);
}

static void flush(FernClient *c) {
    if (c->out.len) {
        fern_net_send(&c->net, c->out.data, c->out.len);
        fern_writer_reset(&c->out);
    }
}

static void add_chat(FernClient *c, const char *text) {
    FernChatLine *l = &c->chat[c->chat_head];
    snprintf(l->text, sizeof(l->text), "%s", text);
    l->time = c->now;
    c->chat_head = (c->chat_head + 1) % FERN_CHAT_LINES;
    if (c->chat_count < FERN_CHAT_LINES) c->chat_count++;
}

const FernChatLine *fern_client_chat_line(const FernClient *c, int i) {
    if (i < 0 || i >= c->chat_count) return NULL;
    return &c->chat[(c->chat_head - 1 - i + FERN_CHAT_LINES * 2) % FERN_CHAT_LINES];
}

static void reset_game(FernClient *c) {
    int i;
    fern_world_clear(&c->world);
    fern_entities_clear(&c->ents);
    memset(&c->body, 0, sizeof(c->body));
    c->spawned = 0;
    c->digging = 0;
    for (i = 0; i < 45; i++) c->inv[i].id = -1;
}

void fern_client_connect(FernClient *c, const FernVersion *v, const char *host, int port, const char *name) {
    reset_game(c);
    c->v = v;
    c->reason[0] = 0;
    snprintf(c->name, sizeof(c->name), "%s", name);
    c->health = 20;
    c->held = 0;
    fern_log("client: connect to %s:%d as %s with %s", host, port, name, v->id);
    if (!fern_net_connect(&c->net, host, port)) {
        snprintf(c->reason, sizeof(c->reason), "%s", c->net.error);
        c->state = CLIENT_GONE;
        return;
    }
    c->state = CLIENT_CONNECTING;
}

static void gone(FernClient *c, const char *reason) {
    if (c->state == CLIENT_GONE) return;
    snprintf(c->reason, sizeof(c->reason), "%s", reason);
    fern_log("client: disconnected: %s", reason);
    fern_net_close(&c->net);
    c->state = CLIENT_GONE;
}

void fern_client_disconnect(FernClient *c, const char *reason) {
    if (c->state == CLIENT_PLAY || c->state == CLIENT_LOGIN) {
        fern_send_disconnect(&c->out, c->v, reason);
        flush(c);
        fern_net_poll(&c->net);
    }
    gone(c, reason);
}

void fern_client_chat(FernClient *c, const char *text) {
    char line[120];
    if (c->state != CLIENT_PLAY || !text[0]) return;
    // The server refuses a chat line of more than 119 characters.
    snprintf(line, sizeof(line), "%s", text);
    fern_send_chat(&c->out, c->v, line);
}

void fern_client_hold(FernClient *c, int slot) {
    if (slot < 0 || slot > 8) return;
    c->held = slot;
    if (c->state != CLIENT_PLAY) return;
    if (c->v->wire <= 6)
        fern_send_hold_item(&c->out, c->v, c->self_id, c->inv[36 + slot].id);
    else
        fern_send_hold(&c->out, c->v, slot);
}

void fern_client_respawn(FernClient *c) {
    if (c->state == CLIENT_PLAY) fern_send_respawn(&c->out, c->v, c->dimension);
}

static void set_items(FernClient *c, const FernPacket *p) {
    int i;
    if (p->u.items.window != 0) return;
    for (i = 0; i < p->u.items.count && i < 45; i++) c->inv[i] = p->u.items.items[i];
}

static void handle(FernClient *c, const FernPacket *p) {
    FernEntity *e;
    switch (p->id) {
    case 0x00:
        // Answer each keep-alive.
        fern_send_keepalive(&c->out, c->v);
        break;
    case 0x02:
        // The server answered the handshake. Only now send the login.
        fern_send_login(&c->out, c->v, c->name);
        c->state = CLIENT_LOGIN;
        break;
    case 0x01:
        c->self_id = p->u.login.entity;
        c->seed = p->u.login.seed;
        c->dimension = p->u.login.dimension;
        c->state = CLIENT_PLAY;
        fern_log("client: logged in as entity %d, dimension %d", c->self_id, c->dimension);
        break;
    case 0x03:
        add_chat(c, p->u.str.text);
        fern_log("chat: %s", p->u.str.text);
        break;
    case 0x04:
        c->world.time = p->u.time.time;
        break;
    case 0x05:
        if (c->v->wire <= 6) {
            // Alpha: the main inventory (-1) has the hotbar in slots 0 to 8.
            int i;
            if (p->u.alpha_inv.type != -1) break;
            for (i = 0; i < p->u.alpha_inv.count && i < 36; i++) c->inv[i < 9 ? 36 + i : i] = p->u.alpha_inv.items[i];
            break;
        }
        e = fern_entity_get(&c->ents, p->u.equipment.entity);
        if (e && p->u.equipment.slot == 0) e->item = p->u.equipment.item;
        break;
    case 0x08:
        c->health = p->u.health.health;
        break;
    case 0x09:
        // The server moves the player to a new world.
        c->dimension = p->u.respawn.dimension;
        reset_game(c);
        c->health = 20;
        break;
    case 0x0D:
        c->body.x = p->u.move.x;
        c->body.y = p->u.move.y;
        c->body.z = p->u.move.z;
        c->body.yaw = p->u.move.yaw;
        c->body.pitch = p->u.move.pitch;
        c->body.mx = c->body.my = c->body.mz = 0;
        // Send the position back, so that the server knows that it came.
        fern_send_position_look(&c->out, c->v, c->body.x, c->body.y, c->body.y + FERN_EYE_HEIGHT, c->body.z,
                                c->body.yaw, c->body.pitch, 0);
        if (!c->spawned) fern_log("client: spawn at %.2f %.2f %.2f (stance %.2f), below: %d", c->body.x, c->body.y, c->body.z, p->u.move.stance, FERN_BLOCK(fern_world_get(&c->world, (int)floor(c->body.x), (int)floor(c->body.y) - 1, (int)floor(c->body.z))));
        c->spawned = 1;
        break;
    case 0x12:
        e = fern_entity_get(&c->ents, p->u.anim.entity);
        if (e && p->u.anim.anim == 1) e->swing = 1.0f;
        break;
    case 0x14:
        e = fern_entity_add(&c->ents, p->u.named.entity, ENT_PLAYER);
        if (!e) break;
        snprintf(e->name, sizeof(e->name), "%s", p->u.named.name);
        fern_entity_place(e, p->u.named.x, p->u.named.y, p->u.named.z);
        e->yaw = p->u.named.yaw * 360.0f / 256.0f;
        e->pitch = p->u.named.pitch * 360.0f / 256.0f;
        e->item = p->u.named.item;
        fern_log("client: player %s comes into view", e->name);
        break;
    case 0x15:
        e = fern_entity_add(&c->ents, p->u.pickup.entity, ENT_ITEM);
        if (!e) break;
        fern_entity_place(e, p->u.pickup.x, p->u.pickup.y, p->u.pickup.z);
        e->item = p->u.pickup.item.id;
        break;
    case 0x16:
        fern_entity_remove(&c->ents, p->u.collect.collected);
        break;
    case 0x17:
        e = fern_entity_add(&c->ents, p->u.object.entity, ENT_OBJECT);
        if (!e) break;
        e->type = (uint8_t)p->u.object.type;
        fern_entity_place(e, p->u.object.x, p->u.object.y, p->u.object.z);
        break;
    case 0x18:
        e = fern_entity_add(&c->ents, p->u.mob.entity, ENT_MOB);
        if (!e) break;
        e->type = (uint8_t)p->u.mob.type;
        fern_entity_place(e, p->u.mob.x, p->u.mob.y, p->u.mob.z);
        e->yaw = p->u.mob.yaw * 360.0f / 256.0f;
        e->pitch = p->u.mob.pitch * 360.0f / 256.0f;
        break;
    case 0x1D:
        fern_entity_remove(&c->ents, p->u.entity.entity);
        break;
    case 0x1F: case 0x20: case 0x21:
        e = fern_entity_get(&c->ents, p->u.rel.entity);
        if (!e) break;
        fern_entity_move(e, p->u.rel.dx, p->u.rel.dy, p->u.rel.dz);
        if (p->id != 0x1F) {
            e->yaw = p->u.rel.yaw * 360.0f / 256.0f;
            e->pitch = p->u.rel.pitch * 360.0f / 256.0f;
        }
        break;
    case 0x22:
        e = fern_entity_get(&c->ents, p->u.teleport.entity);
        if (!e) break;
        fern_entity_place(e, p->u.teleport.x, p->u.teleport.y, p->u.teleport.z);
        e->yaw = p->u.teleport.yaw * 360.0f / 256.0f;
        e->pitch = p->u.teleport.pitch * 360.0f / 256.0f;
        break;
    case 0x28: {
        int i;
        e = fern_entity_get(&c->ents, p->u.meta.entity);
        if (!e) break;
        for (i = 0; i < p->u.meta.meta.count; i++)
            if (p->u.meta.meta.entry[i].index == 0) e->crouch = (p->u.meta.meta.entry[i].value & 2) != 0;
        break;
    }
    case 0x32:
        if (!p->u.prechunk.load) fern_world_unload(&c->world, p->u.prechunk.x, p->u.prechunk.z);
        break;
    case 0x33:
        if (!fern_chunk_apply(&c->world, c->v, p)) fern_log("client: a chunk at %d %d has bad data", p->u.chunk.x, p->u.chunk.z);
        c->chunks_in++;
        break;
    case 0x34:
        fern_chunk_apply_multi(&c->world, c->v, p);
        break;
    case 0x35:
        fern_chunk_apply_block(&c->world, c->v, p);
        break;
    case 0x3C: {
        int i;
        int bx = (int)floor(p->u.explosion.x), by = (int)floor(p->u.explosion.y), bz = (int)floor(p->u.explosion.z);
        for (i = 0; i < p->u.explosion.count; i++) {
            const int8_t *r = (const int8_t *)p->u.explosion.records + i * 3;
            fern_world_set(&c->world, bx + r[0], by + r[1], bz + r[2], 0);
        }
        break;
    }
    case 0x64:
        // There is no screen for a chest, a furnace, or a workbench yet.
        // Close the window at once, so that the server does not wait.
        fern_send_close_window(&c->out, c->v, p->u.open_window.window);
        add_chat(c, "\xC2\xA7" "7Fernstone has no inventory screen yet.");
        break;
    case 0x67:
        if (p->u.set_slot.window == 0 && p->u.set_slot.slot >= 0 && p->u.set_slot.slot < 45)
            c->inv[p->u.set_slot.slot] = p->u.set_slot.item;
        break;
    case 0x68:
        set_items(c, p);
        break;
    case 0xFF:
        gone(c, p->u.str.text);
        break;
    default:
        break;
    }
}

static void read_packets(FernClient *c) {
    size_t pos = 0;
    while (c->state != CLIENT_GONE) {
        FernPacket p;
        size_t used = 0;
        FernDecode d;
        // A kick before the login has modified UTF-8 in each version,
        // because the server does not know the protocol yet.
        if (c->state == CLIENT_HANDSHAKE && c->net.in_len > pos && c->net.in[pos] == 0xFF && c->v->str16) {
            FernVersion old = *c->v;
            old.str16 = 0;
            d = fern_packet_decode(&old, c->net.in + pos, c->net.in_len - pos, &p, &used);
        } else {
            d = fern_packet_decode(c->v, c->net.in + pos, c->net.in_len - pos, &p, &used);
        }
        if (d == FERN_DECODE_MORE) break;
        if (d == FERN_DECODE_BAD) {
            char why[64];
            snprintf(why, sizeof(why), "bad packet 0x%02X from the server", c->net.in[pos]);
            gone(c, why);
            return;
        }
        handle(c, &p);
        c->packets_in++;
        pos += used;
    }
    if (c->state != CLIENT_GONE) fern_net_consume(&c->net, pos);
}

static const FernItem *held_item(FernClient *c) {
    return &c->inv[36 + c->held];
}

// The neighbor of a block on a face.
static void face_offset(int face, int *x, int *y, int *z) {
    switch (face) {
    case 0: (*y)--; break;
    case 1: (*y)++; break;
    case 2: (*z)--; break;
    case 3: (*z)++; break;
    case 4: (*x)--; break;
    default: (*x)++; break;
    }
}

static void look_dir(float yaw, float pitch, double *dx, double *dy, double *dz) {
    double y = yaw * M_PI / 180.0, p = pitch * M_PI / 180.0;
    *dx = -sin(y) * cos(p);
    *dy = -sin(p);
    *dz = cos(y) * cos(p);
}

// The blocks that a use opens or switches. A place on them does not put a
// block, so the client does not show one.
static int is_used_block(int block) {
    switch (block) {
    case B_CHEST: case B_LOCKED_CHEST: case B_WORKBENCH: case B_FURNACE: case B_FURNACE_LIT:
    case B_DISPENSER: case B_NOTE: case B_JUKEBOX: case B_BED: case B_DOOR_WOOD: case B_DOOR_IRON:
    case B_TRAPDOOR: case B_LEVER: case B_BUTTON: case B_REPEATER_OFF: case B_REPEATER_ON: case B_CAKE:
        return 1;
    default:
        return 0;
    }
}

static void stop_dig(FernClient *c) {
    c->digging = 0;
    c->dig_done = 0;
}

static void tick_actions(FernClient *c, const FernInput *in) {
    double dx, dy, dz;
    int item = held_item(c)->id;
    if (in->aim) {
        dx = in->aim_x;
        dy = in->aim_y;
        dz = in->aim_z;
    } else {
        look_dir(c->body.yaw, c->body.pitch, &dx, &dy, &dz);
    }
    c->target = fern_raycast(&c->world, c->body.x, c->body.y + FERN_EYE_HEIGHT, c->body.z, dx, dy, dz, REACH);
    if (c->use_cooldown > 0) c->use_cooldown--;

    if (in->attack && c->target.hit) {
        FernRayHit *t = &c->target;
        int block = FERN_BLOCK(fern_world_get(&c->world, t->x, t->y, t->z));
        if (!c->digging || t->x != c->dig_x || t->y != c->dig_y || t->z != c->dig_z) {
            c->digging = 1;
            c->dig_x = t->x;
            c->dig_y = t->y;
            c->dig_z = t->z;
            c->dig_face = t->face;
            c->dig_done = 0;
            fern_send_dig(&c->out, c->v, 0, t->x, t->y, t->z, t->face);
        }
        c->dig_done += fern_dig_speed(block, item);
        if ((c->ticks & 3) == 0) fern_send_animation(&c->out, c->v, c->self_id, 1);
        // Before layout 9, the client sends status 1 while it digs, and
        // status 3 when the block breaks.
        if (c->v->wire <= 8 && c->dig_done < 1.0f)
            fern_send_dig(&c->out, c->v, 1, c->dig_x, c->dig_y, c->dig_z, c->dig_face);
        if (c->dig_done >= 1.0f) {
            fern_send_dig(&c->out, c->v, c->v->wire <= 8 ? 3 : 2, c->dig_x, c->dig_y, c->dig_z, c->dig_face);
            fern_world_set(&c->world, c->dig_x, c->dig_y, c->dig_z, 0);
            stop_dig(c);
            c->use_cooldown = 5;
        }
    } else if (c->digging) {
        stop_dig(c);
    }

    if (in->use && c->use_cooldown == 0) {
        const FernItem *it = held_item(c);
        c->use_cooldown = 4;
        fern_send_animation(&c->out, c->v, c->self_id, 1);
        fern_log("use: aim %d (%.2f %.2f %.2f), target %d at %d %d %d face %d", in->aim, dx, dy, dz, c->target.hit,
                 c->target.x, c->target.y, c->target.z, c->target.face);
        if (c->target.hit) {
            int x = c->target.x, y = c->target.y, z = c->target.z;
            int under = FERN_BLOCK(fern_world_get(&c->world, x, y, z));
            fern_send_place(&c->out, c->v, x, y, z, c->target.face, it);
            // Show a placed block at once. The server sends the true block.
            if (it->id > 0 && it->id < 256 && !is_used_block(under)) {
                int block = fern_block_from_wire(c->v->block_table, it->id);
                FernBox me, boxes[2];
                int n, i, blocked = 0;
                if (!(fern_block_info(under)->flags & BF_REPLACEABLE)) face_offset(c->target.face, &x, &y, &z);
                me.x0 = c->body.x - 0.3; me.x1 = c->body.x + 0.3;
                me.y0 = c->body.y; me.y1 = c->body.y + FERN_PLAYER_HEIGHT;
                me.z0 = c->body.z - 0.3; me.z1 = c->body.z + 0.3;
                n = fern_block_boxes(FERN_STATE(block, 0), x, y, z, boxes);
                for (i = 0; i < n; i++)
                    if (me.x1 > boxes[i].x0 && me.x0 < boxes[i].x1 && me.y1 > boxes[i].y0 && me.y0 < boxes[i].y1 &&
                        me.z1 > boxes[i].z0 && me.z0 < boxes[i].z1)
                        blocked = 1;
                if (!blocked && fern_block_info(FERN_BLOCK(fern_world_get(&c->world, x, y, z)))->flags & BF_REPLACEABLE)
                    fern_world_set(&c->world, x, y, z, FERN_STATE(block, it->damage));
            }
        } else {
            fern_send_place(&c->out, c->v, -1, 255, -1, 255, it);
        }
    }
}

static void tick(FernClient *c, const FernInput *in) {
    FernMoveInput mi;
    int moved, turned;
    c->ticks++;
    if (!c->spawned) return;
    // Wait for the column under the player, so that it does not fall.
    if (!fern_world_column(&c->world, (int)floor(c->body.x) >> 4, (int)floor(c->body.z) >> 4)) {
        fern_send_flying(&c->out, c->v, c->body.on_ground);
        return;
    }
    c->body.yaw = in->yaw;
    c->body.pitch = in->pitch;
    if (c->health > 0) {
        mi.forward = in->forward;
        mi.strafe = in->strafe;
        mi.jump = in->jump;
        mi.sneak = in->sneak;
    } else {
        memset(&mi, 0, sizeof(mi));
    }
    fern_body_tick(&c->body, &c->world, &mi);
    if (c->health > 0) tick_actions(c, in);

    moved = fabs(c->body.x - c->last_x) > 1e-4 || fabs(c->body.y - c->last_y) > 1e-4 ||
            fabs(c->body.z - c->last_z) > 1e-4 || c->since_position >= 20;
    turned = c->body.yaw != c->last_yaw || c->body.pitch != c->last_pitch;
    if (moved && turned)
        fern_send_position_look(&c->out, c->v, c->body.x, c->body.y, c->body.y + FERN_EYE_HEIGHT, c->body.z,
                                c->body.yaw, c->body.pitch, c->body.on_ground);
    else if (moved)
        fern_send_position(&c->out, c->v, c->body.x, c->body.y, c->body.y + FERN_EYE_HEIGHT, c->body.z,
                           c->body.on_ground);
    else if (turned)
        fern_send_look(&c->out, c->v, c->body.yaw, c->body.pitch, c->body.on_ground);
    else
        fern_send_flying(&c->out, c->v, c->body.on_ground);
    if (moved) {
        c->last_x = c->body.x;
        c->last_y = c->body.y;
        c->last_z = c->body.z;
        c->since_position = 0;
    } else {
        c->since_position++;
    }
    c->last_yaw = c->body.yaw;
    c->last_pitch = c->body.pitch;
}

void fern_client_update(FernClient *c, const FernInput *in, double now) {
    FernNetState s;
    c->now = now;
    if (c->state == CLIENT_IDLE || c->state == CLIENT_GONE) return;
    s = fern_net_poll(&c->net);
    if (s == FERN_NET_FAILED || s == FERN_NET_CLOSED) {
        gone(c, c->net.error[0] ? c->net.error : "the connection closed");
        return;
    }
    if (c->state == CLIENT_CONNECTING && s == FERN_NET_OPEN) {
        // Send the handshake alone. The server reads the whole first read
        // as the handshake, so the login waits for the answer.
        fern_send_handshake(&c->out, c->v, c->name);
        flush(c);
        c->state = CLIENT_HANDSHAKE;
        c->tick_time = now;
    }
    read_packets(c);
    if (c->state == CLIENT_PLAY) {
        int n = 0;
        if (now - c->tick_time > 1.0) c->tick_time = now - TICK;
        while (c->tick_time <= now && n < 10) {
            tick(c, in);
            c->tick_time += TICK;
            n++;
        }
    }
    flush(c);
    fern_net_poll(&c->net);
}

void fern_client_free(FernClient *c) {
    fern_net_close(&c->net);
    fern_writer_free(&c->out);
    fern_world_clear(&c->world);
}
