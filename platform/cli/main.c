// fernstone-cli: a client with no window, for tests on the Mac.
// Usage: fernstone-cli HOST[:PORT] NAME [--seconds N] [--script FILE]
// It connects, logs in, receives the chunks, and answers the keep-alives.
// It reads commands from the script, or from stdin when there is no script:
//   say TEXT          send a chat line
//   wait S            do nothing for S seconds
//   walk F S SECONDS  move with forward F and strafe S (-1 to 1)
//   jump              jump one time
//   look YAW PITCH    turn to a direction in degrees
//   dig SECONDS       hold the attack on the block in view
//   place             place the held block on the block in view
//   hold N            select hotbar slot N (0 to 8)
//   status            print the position and the counts
//   quit              disconnect
#include <fcntl.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <unistd.h>
#include "client.h"
#include "log.h"

static double now_s(void) {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return tv.tv_sec + tv.tv_usec / 1e6;
}

static void status(FernClient *c) {
    int i, players = 0, mobs = 0;
    for (i = 0; i < FERN_ENTITY_MAX; i++) {
        if (!c->ents.e[i].used) continue;
        if (c->ents.e[i].kind == ENT_PLAYER) players++;
        if (c->ents.e[i].kind == ENT_MOB) mobs++;
    }
    printf("status: state %d, pos %.2f %.2f %.2f, ground %d, health %d, columns %d, chunks %ld, "
           "packets %ld, players %d, mobs %d, time %lld\n",
           c->state, c->body.x, c->body.y, c->body.z, c->body.on_ground, c->health, c->world.columns,
           c->chunks_in, c->packets_in, players, mobs, (long long)c->world.time);
    for (i = 0; i < FERN_ENTITY_MAX; i++)
        if (c->ents.e[i].used && c->ents.e[i].kind == ENT_PLAYER)
            printf("status: player %s at %.2f %.2f %.2f\n", c->ents.e[i].name, c->ents.e[i].x, c->ents.e[i].y,
                   c->ents.e[i].z);
    fflush(stdout);
}

int main(int argc, char **argv) {
    static FernClient c;
    FernInput in;
    char host[128] = "127.0.0.1";
    int port = 25565, i;
    double seconds = 0, start, until = 0;
    FILE *script = NULL;
    char line[512];
    int jump_once = 0, use_once = 0;
    char *colon;
    if (argc < 3) {
        fprintf(stderr, "usage: fernstone-cli HOST[:PORT] NAME [--seconds N] [--script FILE]\n");
        return 2;
    }
    snprintf(host, sizeof(host), "%s", argv[1]);
    colon = strchr(host, ':');
    if (colon) {
        *colon = 0;
        port = atoi(colon + 1);
    }
    for (i = 3; i < argc; i++) {
        if (!strcmp(argv[i], "--seconds") && i + 1 < argc) seconds = atof(argv[++i]);
        else if (!strcmp(argv[i], "--script") && i + 1 < argc) script = fopen(argv[++i], "r");
    }
    if (!script) fcntl(0, F_SETFL, fcntl(0, F_GETFL) | O_NONBLOCK);
    memset(&in, 0, sizeof(in));
    fern_client_init(&c);
    start = now_s();
    fern_client_connect(&c, fern_version_latest(), host, port, argv[2]);
    for (;;) {
        double t = now_s();
        if (seconds > 0 && t - start > seconds) break;
        if (c.state == CLIENT_GONE) break;
        // Read the next command when the last one is done.
        if (t >= until && c.state == CLIENT_PLAY && c.spawned) {
            char *got = script ? fgets(line, sizeof(line), script) : fgets(line, sizeof(line), stdin);
            in.forward = in.strafe = 0;
            in.attack = 0;
            if (got) {
                float a, b, d;
                line[strcspn(line, "\r\n")] = 0;
                if (line[0]) printf("cmd: %s\n", line);
                if (!strncmp(line, "say ", 4)) fern_client_chat(&c, line + 4);
                else if (sscanf(line, "wait %f", &a) == 1) until = t + a;
                else if (sscanf(line, "walk %f %f %f", &a, &b, &d) == 3) {
                    in.forward = a;
                    in.strafe = b;
                    until = t + d;
                } else if (!strcmp(line, "jump")) jump_once = 1;
                else if (sscanf(line, "look %f %f", &a, &b) == 2) {
                    in.yaw = a;
                    in.pitch = b;
                } else if (sscanf(line, "dig %f", &a) == 1) {
                    in.attack = 1;
                    until = t + a;
                } else if (!strcmp(line, "place")) use_once = 1;
                else if (sscanf(line, "hold %f", &a) == 1) fern_client_hold(&c, (int)a);
                else if (!strcmp(line, "status")) status(&c);
                else if (!strcmp(line, "quit")) break;
            } else if (script && seconds == 0) {
                break;
            }
        }
        in.jump = jump_once;
        in.use = use_once;
        fern_client_update(&c, &in, t);
        if (c.ticks > 0) {
            jump_once = 0;
            use_once = 0;
        }
        usleep(5000);
    }
    status(&c);
    if (c.state == CLIENT_GONE) {
        printf("gone: %s\n", c.reason);
        return 1;
    }
    fern_client_disconnect(&c, "Quitting");
    fern_client_free(&c);
    return 0;
}
