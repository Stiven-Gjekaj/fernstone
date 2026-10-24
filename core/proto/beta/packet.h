// packet.h: decode the packets of the server and encode the packets of the
// client, for the protocols from Alpha 1.0.15 to Beta 1.7.3.
// A packet has no length on the wire. The decoder reads the fields of the
// packet ID. If the data stops before the packet ends, it asks for more.
#ifndef FERN_PACKET_H
#define FERN_PACKET_H

#include <stdint.h>
#include "buf.h"
#include "version.h"

#define FERN_STR_MAX 256
#define FERN_META_MAX 16

typedef struct {
    int16_t id; // -1 for an empty slot.
    int8_t count;
    int16_t damage;
} FernItem;

// One entry of the entity metadata. Only the number types are kept.
typedef struct {
    uint8_t index;
    uint8_t type;
    int32_t value;
} FernMeta;

typedef struct {
    uint8_t count;
    FernMeta entry[FERN_META_MAX];
} FernMetaList;

typedef struct {
    int id;
    union {
        struct { char text[FERN_STR_MAX]; } str; // 0x02, 0x03, 0xFF
        struct { int32_t entity; int64_t seed; int8_t dimension; } login;
        struct { int64_t time; } time;
        struct { int32_t entity; int16_t slot, item, damage; } equipment;
        struct { int32_t x, y, z; } spawn;
        struct { int16_t health; } health;
        struct { int8_t dimension; } respawn;
        struct { double x, y, stance, z; float yaw, pitch; uint8_t on_ground; } move;
        struct { int32_t entity; uint8_t in_bed; int32_t x, y, z; } bed;
        struct { int32_t entity; int8_t anim; } anim;
        struct {
            int32_t entity;
            char name[32];
            int32_t x, y, z;
            int8_t yaw, pitch;
            int16_t item;
        } named;
        struct { int32_t entity; FernItem item; int32_t x, y, z; } pickup;
        struct { int32_t collected, collector; } collect;
        struct { int32_t entity; int8_t type; int32_t x, y, z; } object;
        struct {
            int32_t entity;
            int8_t type;
            int32_t x, y, z;
            int8_t yaw, pitch;
            FernMetaList meta;
        } mob;
        struct { int32_t entity; int16_t vx, vy, vz; } velocity;
        struct { int32_t entity; } entity;
        struct { int32_t entity; int8_t dx, dy, dz, yaw, pitch; } rel;
        struct { int32_t entity; int32_t x, y, z; int8_t yaw, pitch; } teleport;
        struct { int32_t entity; int8_t status; } status;
        struct { int32_t entity, vehicle; } attach;
        struct { int32_t entity; FernMetaList meta; } meta;
        struct { int32_t x, z; uint8_t load; } prechunk;
        struct {
            int32_t x, y, z;
            int sx, sy, sz;          // The size of the box, 1 to 128.
            int32_t size;
            const uint8_t *data;     // The zlib data. It points into the input.
        } chunk;
        struct {
            int32_t cx, cz;
            int16_t count;
            const uint8_t *coords;   // count big-endian shorts: x<<12 | z<<8 | y
            const uint8_t *types;
            const uint8_t *metas;
        } multi;
        struct { int32_t x, y, z; uint8_t type, meta; } block;
        struct { int8_t reason; } state;
        struct { int8_t window; int16_t slot; FernItem item; } set_slot;
        struct { int8_t window; int16_t count; FernItem items[100]; } items;
        struct { int32_t x, y, z; char lines[4][64]; } sign;
    } u;
} FernPacket;

typedef enum {
    FERN_DECODE_OK = 1,
    FERN_DECODE_MORE = 0,  // The data stops inside the packet.
    FERN_DECODE_BAD = -1   // The packet ID is not known, or a field is bad.
} FernDecode;

// Decode one packet from the server at the start of data.
// On FERN_DECODE_OK, *used is the size of the packet.
FernDecode fern_packet_decode(const FernVersion *v, const uint8_t *data, size_t len,
                              FernPacket *p, size_t *used);

// The packets of the client.
void fern_send_keepalive(FernWriter *w, const FernVersion *v);
void fern_send_handshake(FernWriter *w, const FernVersion *v, const char *name);
void fern_send_login(FernWriter *w, const FernVersion *v, const char *name);
void fern_send_chat(FernWriter *w, const FernVersion *v, const char *text);
void fern_send_flying(FernWriter *w, const FernVersion *v, int on_ground);
void fern_send_position(FernWriter *w, const FernVersion *v, double x, double y, double stance,
                        double z, int on_ground);
void fern_send_look(FernWriter *w, const FernVersion *v, float yaw, float pitch, int on_ground);
void fern_send_position_look(FernWriter *w, const FernVersion *v, double x, double y, double stance,
                             double z, float yaw, float pitch, int on_ground);
void fern_send_dig(FernWriter *w, const FernVersion *v, int status, int x, int y, int z, int face);
void fern_send_place(FernWriter *w, const FernVersion *v, int x, int y, int z, int face,
                     const FernItem *item);
void fern_send_hold(FernWriter *w, const FernVersion *v, int slot);
void fern_send_animation(FernWriter *w, const FernVersion *v, int entity, int anim);
void fern_send_respawn(FernWriter *w, const FernVersion *v, int dimension);
void fern_send_use_entity(FernWriter *w, const FernVersion *v, int self, int target, int left);
void fern_send_disconnect(FernWriter *w, const FernVersion *v, const char *reason);

#endif
