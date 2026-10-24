#include "packet.h"
#include <string.h>

static void read_str(FernReader *r, const FernVersion *v, char *out, size_t cap) {
    if (v->str16)
        fern_read_str16(r, out, cap);
    else
        fern_read_str8(r, out, cap);
}

static void write_str(FernWriter *w, const FernVersion *v, const char *s) {
    if (v->str16)
        fern_write_str16(w, s);
    else
        fern_write_str8(w, s);
}

// An item in a window packet: a short ID, and for an ID that is not -1, a
// count and a damage value.
static void read_item(FernReader *r, FernItem *item) {
    item->id = fern_read_i16(r);
    item->count = 0;
    item->damage = 0;
    if (item->id >= 0) {
        item->count = fern_read_i8(r);
        item->damage = fern_read_i16(r);
    }
}

static void write_item(FernWriter *w, const FernItem *item) {
    if (!item || item->id < 0) {
        fern_write_i16(w, -1);
        return;
    }
    fern_write_i16(w, item->id);
    fern_write_u8(w, (uint8_t)item->count);
    fern_write_i16(w, item->damage);
}

// The metadata of an entity: entries until the byte 0x7F.
static void read_meta(FernReader *r, const FernVersion *v, FernMetaList *m) {
    char tmp[FERN_STR_MAX];
    m->count = 0;
    for (;;) {
        uint8_t b = fern_read_u8(r);
        int32_t value = 0;
        if (!fern_reader_ok(r) || b == 0x7F) break;
        switch (b >> 5) {
        case 0: value = fern_read_i8(r); break;
        case 1: value = fern_read_i16(r); break;
        case 2: value = fern_read_i32(r); break;
        case 3: value = (int32_t)fern_read_f32(r); break;
        case 4: read_str(r, v, tmp, sizeof(tmp)); break;
        case 5:
            value = fern_read_i16(r);
            fern_read_i8(r);
            fern_read_i16(r);
            break;
        case 6:
            fern_read_i32(r);
            fern_read_i32(r);
            fern_read_i32(r);
            break;
        default: r->bad = 1; return;
        }
        if (m->count < FERN_META_MAX) {
            m->entry[m->count].index = b & 0x1F;
            m->entry[m->count].type = b >> 5;
            m->entry[m->count].value = value;
            m->count++;
        }
    }
}

FernDecode fern_packet_decode(const FernVersion *v, const uint8_t *data, size_t len,
                              FernPacket *p, size_t *used) {
    FernReader rd, *r = &rd;
    int i;
    char skip[FERN_STR_MAX];
    if (len == 0) return FERN_DECODE_MORE;
    fern_reader_init(r, data, len);
    p->id = fern_read_u8(r);
    switch (p->id) {
    case 0x00: // Keep alive
        break;
    case 0x01: // Login
        p->u.login.entity = fern_read_i32(r);
        read_str(r, v, skip, sizeof(skip));
        p->u.login.seed = fern_read_i64(r);
        p->u.login.dimension = fern_read_i8(r);
        break;
    case 0x02: // Handshake
    case 0x03: // Chat
    case 0xFF: // Kick
        read_str(r, v, p->u.str.text, sizeof(p->u.str.text));
        break;
    case 0x04: // Time
        p->u.time.time = fern_read_i64(r);
        break;
    case 0x05: // Entity equipment
        p->u.equipment.entity = fern_read_i32(r);
        p->u.equipment.slot = fern_read_i16(r);
        p->u.equipment.item = fern_read_i16(r);
        p->u.equipment.damage = fern_read_i16(r);
        break;
    case 0x06: // Spawn position
        p->u.spawn.x = fern_read_i32(r);
        p->u.spawn.y = fern_read_i32(r);
        p->u.spawn.z = fern_read_i32(r);
        break;
    case 0x07: // Use entity
        fern_read_bytes(r, 9);
        break;
    case 0x08: // Health
        p->u.health.health = fern_read_i16(r);
        break;
    case 0x09: // Respawn
        p->u.respawn.dimension = fern_read_i8(r);
        break;
    case 0x0A: // On ground
        p->u.move.on_ground = fern_read_u8(r);
        break;
    case 0x0B: // Position
        p->u.move.x = fern_read_f64(r);
        p->u.move.y = fern_read_f64(r);
        p->u.move.stance = fern_read_f64(r);
        p->u.move.z = fern_read_f64(r);
        p->u.move.on_ground = fern_read_u8(r);
        break;
    case 0x0C: // Look
        p->u.move.yaw = fern_read_f32(r);
        p->u.move.pitch = fern_read_f32(r);
        p->u.move.on_ground = fern_read_u8(r);
        break;
    case 0x0D: // Position and look. From the server, the stance comes first.
        p->u.move.x = fern_read_f64(r);
        p->u.move.stance = fern_read_f64(r);
        p->u.move.y = fern_read_f64(r);
        p->u.move.z = fern_read_f64(r);
        p->u.move.yaw = fern_read_f32(r);
        p->u.move.pitch = fern_read_f32(r);
        p->u.move.on_ground = fern_read_u8(r);
        break;
    case 0x0E: // Digging
        fern_read_bytes(r, 11);
        break;
    case 0x0F: { // Place
        FernItem item;
        fern_read_bytes(r, 10);
        read_item(r, &item);
        break;
    }
    case 0x10: // Holding change
        fern_read_i16(r);
        break;
    case 0x11: // Use bed
        p->u.bed.entity = fern_read_i32(r);
        p->u.bed.in_bed = fern_read_u8(r);
        p->u.bed.x = fern_read_i32(r);
        p->u.bed.y = fern_read_i8(r);
        p->u.bed.z = fern_read_i32(r);
        break;
    case 0x12: // Animation
        p->u.anim.entity = fern_read_i32(r);
        p->u.anim.anim = fern_read_i8(r);
        break;
    case 0x13: // Entity action
        fern_read_bytes(r, 5);
        break;
    case 0x14: // Named entity spawn
        p->u.named.entity = fern_read_i32(r);
        read_str(r, v, p->u.named.name, sizeof(p->u.named.name));
        p->u.named.x = fern_read_i32(r);
        p->u.named.y = fern_read_i32(r);
        p->u.named.z = fern_read_i32(r);
        p->u.named.yaw = fern_read_i8(r);
        p->u.named.pitch = fern_read_i8(r);
        p->u.named.item = fern_read_i16(r);
        break;
    case 0x15: // Pickup spawn
        p->u.pickup.entity = fern_read_i32(r);
        p->u.pickup.item.id = fern_read_i16(r);
        p->u.pickup.item.count = fern_read_i8(r);
        p->u.pickup.item.damage = fern_read_i16(r);
        p->u.pickup.x = fern_read_i32(r);
        p->u.pickup.y = fern_read_i32(r);
        p->u.pickup.z = fern_read_i32(r);
        fern_read_bytes(r, 3);
        break;
    case 0x16: // Collect item
        p->u.collect.collected = fern_read_i32(r);
        p->u.collect.collector = fern_read_i32(r);
        break;
    case 0x17: { // Add object or vehicle
        int32_t thrower;
        p->u.object.entity = fern_read_i32(r);
        p->u.object.type = fern_read_i8(r);
        p->u.object.x = fern_read_i32(r);
        p->u.object.y = fern_read_i32(r);
        p->u.object.z = fern_read_i32(r);
        thrower = fern_read_i32(r);
        if (thrower > 0) fern_read_bytes(r, 6);
        break;
    }
    case 0x18: // Mob spawn
        p->u.mob.entity = fern_read_i32(r);
        p->u.mob.type = fern_read_i8(r);
        p->u.mob.x = fern_read_i32(r);
        p->u.mob.y = fern_read_i32(r);
        p->u.mob.z = fern_read_i32(r);
        p->u.mob.yaw = fern_read_i8(r);
        p->u.mob.pitch = fern_read_i8(r);
        read_meta(r, v, &p->u.mob.meta);
        break;
    case 0x19: // Painting
        fern_read_i32(r);
        read_str(r, v, skip, sizeof(skip));
        fern_read_bytes(r, 16);
        break;
    case 0x1B: // Stance update
        fern_read_bytes(r, 18);
        break;
    case 0x1C: // Velocity
        p->u.velocity.entity = fern_read_i32(r);
        p->u.velocity.vx = fern_read_i16(r);
        p->u.velocity.vy = fern_read_i16(r);
        p->u.velocity.vz = fern_read_i16(r);
        break;
    case 0x1D: // Destroy entity
    case 0x1E: // Entity
        p->u.entity.entity = fern_read_i32(r);
        break;
    case 0x1F: // Relative move
        p->u.rel.entity = fern_read_i32(r);
        p->u.rel.dx = fern_read_i8(r);
        p->u.rel.dy = fern_read_i8(r);
        p->u.rel.dz = fern_read_i8(r);
        p->u.rel.yaw = 0;
        p->u.rel.pitch = 0;
        break;
    case 0x20: // Look
        p->u.rel.entity = fern_read_i32(r);
        p->u.rel.dx = p->u.rel.dy = p->u.rel.dz = 0;
        p->u.rel.yaw = fern_read_i8(r);
        p->u.rel.pitch = fern_read_i8(r);
        break;
    case 0x21: // Relative move and look
        p->u.rel.entity = fern_read_i32(r);
        p->u.rel.dx = fern_read_i8(r);
        p->u.rel.dy = fern_read_i8(r);
        p->u.rel.dz = fern_read_i8(r);
        p->u.rel.yaw = fern_read_i8(r);
        p->u.rel.pitch = fern_read_i8(r);
        break;
    case 0x22: // Teleport
        p->u.teleport.entity = fern_read_i32(r);
        p->u.teleport.x = fern_read_i32(r);
        p->u.teleport.y = fern_read_i32(r);
        p->u.teleport.z = fern_read_i32(r);
        p->u.teleport.yaw = fern_read_i8(r);
        p->u.teleport.pitch = fern_read_i8(r);
        break;
    case 0x26: // Entity status
        p->u.status.entity = fern_read_i32(r);
        p->u.status.status = fern_read_i8(r);
        break;
    case 0x27: // Attach
        p->u.attach.entity = fern_read_i32(r);
        p->u.attach.vehicle = fern_read_i32(r);
        break;
    case 0x28: // Metadata
        p->u.meta.entity = fern_read_i32(r);
        read_meta(r, v, &p->u.meta.meta);
        break;
    case 0x32: // Pre-chunk
        p->u.prechunk.x = fern_read_i32(r);
        p->u.prechunk.z = fern_read_i32(r);
        p->u.prechunk.load = fern_read_u8(r);
        break;
    case 0x33: // Map chunk
        p->u.chunk.x = fern_read_i32(r);
        p->u.chunk.y = fern_read_i16(r);
        p->u.chunk.z = fern_read_i32(r);
        p->u.chunk.sx = fern_read_u8(r) + 1;
        p->u.chunk.sy = fern_read_u8(r) + 1;
        p->u.chunk.sz = fern_read_u8(r) + 1;
        p->u.chunk.size = fern_read_i32(r);
        if (p->u.chunk.size < 0) r->bad = 1;
        p->u.chunk.data = fern_read_bytes(r, fern_reader_ok(r) ? (size_t)p->u.chunk.size : 0);
        break;
    case 0x34: // Multi block change
        p->u.multi.cx = fern_read_i32(r);
        p->u.multi.cz = fern_read_i32(r);
        p->u.multi.count = fern_read_i16(r);
        if (p->u.multi.count < 0) r->bad = 1;
        i = fern_reader_ok(r) ? p->u.multi.count : 0;
        p->u.multi.coords = fern_read_bytes(r, (size_t)i * 2);
        p->u.multi.types = fern_read_bytes(r, (size_t)i);
        p->u.multi.metas = fern_read_bytes(r, (size_t)i);
        break;
    case 0x35: // Block change
        p->u.block.x = fern_read_i32(r);
        p->u.block.y = fern_read_i8(r);
        p->u.block.z = fern_read_i32(r);
        p->u.block.type = fern_read_u8(r);
        p->u.block.meta = fern_read_u8(r);
        break;
    case 0x36: // Block action
        fern_read_bytes(r, 12);
        break;
    case 0x3C: // Explosion
        p->u.explosion.x = fern_read_f64(r);
        p->u.explosion.y = fern_read_f64(r);
        p->u.explosion.z = fern_read_f64(r);
        fern_read_f32(r);
        p->u.explosion.count = fern_read_i32(r);
        if (p->u.explosion.count < 0) r->bad = 1;
        p->u.explosion.records =
            fern_read_bytes(r, fern_reader_ok(r) ? (size_t)p->u.explosion.count * 3 : 0);
        break;
    case 0x3D: // Sound or particle effect
        fern_read_bytes(r, 17);
        break;
    case 0x46: // New state
        p->u.state.reason = fern_read_i8(r);
        break;
    case 0x47: // Thunderbolt
        fern_read_bytes(r, 17);
        break;
    case 0x64: // Open window. The title is modified UTF-8 in each version.
        fern_read_i8(r);
        fern_read_i8(r);
        fern_read_str8(r, skip, sizeof(skip));
        fern_read_i8(r);
        break;
    case 0x65: // Close window
        fern_read_i8(r);
        break;
    case 0x67: // Set slot
        p->u.set_slot.window = fern_read_i8(r);
        p->u.set_slot.slot = fern_read_i16(r);
        read_item(r, &p->u.set_slot.item);
        break;
    case 0x68: { // Window items
        FernItem item;
        p->u.items.window = fern_read_i8(r);
        p->u.items.count = fern_read_i16(r);
        if (p->u.items.count < 0) r->bad = 1;
        for (i = 0; fern_reader_ok(r) && i < p->u.items.count; i++) {
            read_item(r, &item);
            if (i < (int)(sizeof(p->u.items.items) / sizeof(item))) p->u.items.items[i] = item;
        }
        break;
    }
    case 0x69: // Progress bar
        fern_read_bytes(r, 5);
        break;
    case 0x6A: // Transaction
        fern_read_bytes(r, 4);
        break;
    case 0x82: // Sign
        p->u.sign.x = fern_read_i32(r);
        p->u.sign.y = fern_read_i16(r);
        p->u.sign.z = fern_read_i32(r);
        for (i = 0; i < 4; i++) read_str(r, v, p->u.sign.lines[i], sizeof(p->u.sign.lines[i]));
        break;
    case 0x83: { // Map data
        uint8_t n;
        fern_read_bytes(r, 4);
        n = fern_read_u8(r);
        fern_read_bytes(r, n);
        break;
    }
    case 0xC8: // Statistic
        fern_read_bytes(r, 5);
        break;
    default:
        return FERN_DECODE_BAD;
    }
    if (r->bad) return FERN_DECODE_BAD;
    if (r->short_read) return FERN_DECODE_MORE;
    *used = r->pos;
    return FERN_DECODE_OK;
}

void fern_send_keepalive(FernWriter *w, const FernVersion *v) {
    (void)v;
    fern_write_u8(w, 0x00);
}

void fern_send_handshake(FernWriter *w, const FernVersion *v, const char *name) {
    fern_write_u8(w, 0x02);
    write_str(w, v, name);
}

void fern_send_login(FernWriter *w, const FernVersion *v, const char *name) {
    fern_write_u8(w, 0x01);
    fern_write_i32(w, v->pvn);
    write_str(w, v, name);
    fern_write_i64(w, 0);
    fern_write_u8(w, 0);
}

void fern_send_chat(FernWriter *w, const FernVersion *v, const char *text) {
    fern_write_u8(w, 0x03);
    write_str(w, v, text);
}

void fern_send_flying(FernWriter *w, const FernVersion *v, int on_ground) {
    (void)v;
    fern_write_u8(w, 0x0A);
    fern_write_u8(w, (uint8_t)(on_ground != 0));
}

void fern_send_position(FernWriter *w, const FernVersion *v, double x, double y, double stance,
                        double z, int on_ground) {
    (void)v;
    fern_write_u8(w, 0x0B);
    fern_write_f64(w, x);
    fern_write_f64(w, y);
    fern_write_f64(w, stance);
    fern_write_f64(w, z);
    fern_write_u8(w, (uint8_t)(on_ground != 0));
}

void fern_send_look(FernWriter *w, const FernVersion *v, float yaw, float pitch, int on_ground) {
    (void)v;
    fern_write_u8(w, 0x0C);
    fern_write_f32(w, yaw);
    fern_write_f32(w, pitch);
    fern_write_u8(w, (uint8_t)(on_ground != 0));
}

void fern_send_position_look(FernWriter *w, const FernVersion *v, double x, double y, double stance,
                             double z, float yaw, float pitch, int on_ground) {
    (void)v;
    fern_write_u8(w, 0x0D);
    fern_write_f64(w, x);
    fern_write_f64(w, y);
    fern_write_f64(w, stance);
    fern_write_f64(w, z);
    fern_write_f32(w, yaw);
    fern_write_f32(w, pitch);
    fern_write_u8(w, (uint8_t)(on_ground != 0));
}

void fern_send_dig(FernWriter *w, const FernVersion *v, int status, int x, int y, int z, int face) {
    (void)v;
    fern_write_u8(w, 0x0E);
    fern_write_u8(w, (uint8_t)status);
    fern_write_i32(w, x);
    fern_write_u8(w, (uint8_t)y);
    fern_write_i32(w, z);
    fern_write_u8(w, (uint8_t)face);
}

void fern_send_place(FernWriter *w, const FernVersion *v, int x, int y, int z, int face,
                     const FernItem *item) {
    (void)v;
    fern_write_u8(w, 0x0F);
    fern_write_i32(w, x);
    fern_write_u8(w, (uint8_t)y);
    fern_write_i32(w, z);
    fern_write_u8(w, (uint8_t)face);
    write_item(w, item);
}

void fern_send_hold(FernWriter *w, const FernVersion *v, int slot) {
    (void)v;
    fern_write_u8(w, 0x10);
    fern_write_i16(w, (int16_t)slot);
}

void fern_send_animation(FernWriter *w, const FernVersion *v, int entity, int anim) {
    (void)v;
    fern_write_u8(w, 0x12);
    fern_write_i32(w, entity);
    fern_write_u8(w, (uint8_t)anim);
}

void fern_send_respawn(FernWriter *w, const FernVersion *v, int dimension) {
    (void)v;
    fern_write_u8(w, 0x09);
    fern_write_u8(w, (uint8_t)dimension);
}

void fern_send_use_entity(FernWriter *w, const FernVersion *v, int self, int target, int left) {
    (void)v;
    fern_write_u8(w, 0x07);
    fern_write_i32(w, self);
    fern_write_i32(w, target);
    fern_write_u8(w, (uint8_t)(left != 0));
}

void fern_send_disconnect(FernWriter *w, const FernVersion *v, const char *reason) {
    fern_write_u8(w, 0xFF);
    write_str(w, v, reason);
}
