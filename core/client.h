// client.h: one connection to a server and the game state that it gives.
// The client sends the handshake and the login, applies each packet to
// the world model, runs the player physics at 20 ticks a second, and sends
// the position, the digging, the placing, and the chat.
#ifndef FERN_CLIENT_H
#define FERN_CLIENT_H

#include "entity.h"
#include "net.h"
#include "packet.h"
#include "physics.h"
#include "world.h"

typedef enum {
    CLIENT_IDLE,
    CLIENT_CONNECTING,
    CLIENT_HANDSHAKE,  // The handshake went out. The client waits for its answer.
    CLIENT_LOGIN,      // The login went out.
    CLIENT_PLAY,
    CLIENT_GONE        // The connection ended. reason says why.
} FernClientState;

#define FERN_CHAT_LINES 100

typedef struct {
    char text[FERN_STR_MAX];
    double time;
} FernChatLine;

typedef struct {
    float forward, strafe;   // -1 to 1
    int jump, sneak;
    float yaw, pitch;        // Degrees
    int attack;              // Hold to dig the block in the view.
    int use;                 // Set for one frame to place a block or use an item.
    int aim;                 // 1: dig and place along aim_x, aim_y, aim_z, not
    float aim_x, aim_y, aim_z; // along the view (a touch under a finger).
} FernInput;

typedef struct {
    const FernVersion *v;
    FernClientState state;
    char reason[FERN_STR_MAX];
    char name[17];
    FernNet net;
    FernWriter out;

    FernWorld world;
    FernEntities ents;
    FernBody body;
    int self_id;
    int dimension;
    int health;
    int spawned;            // The server sent the first position.
    int64_t seed;
    FernItem inv[45];       // Window 0: 36 to 44 is the hotbar.
    int held;               // 0 to 8

    FernChatLine chat[FERN_CHAT_LINES];
    int chat_count;         // The lines in the ring, up to FERN_CHAT_LINES.
    int chat_head;          // The next line to write.

    // Digging
    FernRayHit target;
    int dig_x, dig_y, dig_z, dig_face;
    int digging;
    float dig_done;         // 0 to 1
    int use_cooldown;

    double tick_time;       // The time of the next tick.
    double now;
    long ticks;
    long packets_in;
    long chunks_in;
    float last_yaw, last_pitch;
    double last_x, last_y, last_z;
    int since_position;     // Ticks since the last position packet.
} FernClient;

void fern_client_init(FernClient *c);
// Start a connection. host is an IP address or a name.
void fern_client_connect(FernClient *c, const FernVersion *v, const char *host, int port, const char *name);
// Read the packets, run the ticks up to the time now, and send.
void fern_client_update(FernClient *c, const FernInput *in, double now);
void fern_client_chat(FernClient *c, const char *text);
void fern_client_hold(FernClient *c, int slot);
void fern_client_respawn(FernClient *c);
void fern_client_disconnect(FernClient *c, const char *reason);
// The chat line i, from 0 (the newest) up. NULL if there is none.
const FernChatLine *fern_client_chat_line(const FernClient *c, int i);
void fern_client_free(FernClient *c);

#endif
