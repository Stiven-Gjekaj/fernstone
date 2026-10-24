#include "version.h"
#include <string.h>

// The last version of each packet layout, from the newest to the oldest.
// The protocol numbers come from the table of wiki.vg. The packet layouts
// come from the source of Project Poseidon Uberbukkit (see
// docs/protocol.md). b1.1_02 sends 8 but uses the layout of 7. The Alpha
// versions before a1.1.0 send 13, 14, or 1. Uberbukkit has no layout for
// them, so they use the oldest known layout (2). No test confirms it.
// "tested": the headless client logged in, received the chunks, and sent a
// chat line to Uberbukkit 2.0.2 (b1.7.3 also to Poseidon). b1.1_02 fails on
// Uberbukkit, because Uberbukkit reads protocol 8 as b1.2 unless its own
// target is 7.
static const FernVersion versions[] = {
    {"b1.7.3", "beta", 14, 14, 1, 14, 1},
    {"b1.6.6", "beta", 13, 13, 1, 13, 1},
    {"b1.5_01", "beta", 11, 11, 1, 11, 1},
    {"b1.4_01", "beta", 10, 10, 0, 10, 1},
    {"b1.3_01", "beta", 9, 9, 0, 9, 1},
    {"b1.2_02", "beta", 8, 8, 0, 8, 1},
    {"b1.1_02", "beta", 8, 7, 0, 7, 0},
    {"b1.1_01", "beta", 7, 7, 0, 7, 1},
    {"a1.2.6", "alpha", 6, 6, 0, 6, 1},
    {"a1.2.3_04", "alpha", 5, 5, 0, 5, 1},
    {"a1.2.2", "alpha", 4, 4, 0, 4, 1},
    {"a1.2.1_01", "alpha", 3, 3, 0, 3, 1},
    {"a1.1.2_01", "alpha", 2, 2, 0, 2, 1},
    {"a1.0.17_04", "alpha", 1, 2, 0, 2, 0},
    {"a1.0.16", "alpha", 13, 2, 0, 2, 0},
    {"a1.0.15", "alpha", 13, 2, 0, 2, 0},
};

int fern_version_count(void) {
    return (int)(sizeof(versions) / sizeof(versions[0]));
}

const FernVersion *fern_version_at(int i) {
    if (i < 0 || i >= fern_version_count()) return NULL;
    return &versions[i];
}

const FernVersion *fern_version_find(const char *id) {
    int i;
    for (i = 0; i < fern_version_count(); i++)
        if (strcmp(versions[i].id, id) == 0) return &versions[i];
    return NULL;
}

const FernVersion *fern_version_latest(void) {
    return &versions[0];
}
