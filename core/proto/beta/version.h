// version.h: the table of the Alpha and Beta multiplayer versions.
// Each entry gives the protocol number and the wire differences that the
// packet code reads. The packet code is the same for each version.
#ifndef FERN_VERSION_H
#define FERN_VERSION_H

typedef struct {
    const char *id;     // The name of the version, for example "b1.7.3".
    const char *era;    // "alpha" or "beta".
    int pvn;            // The protocol number in the login packet.
    int wire;           // The packet layout: the protocol of Uberbukkit
                        // that has the same fields (2 to 14).
    int str16;          // 1: strings are UTF-16. 0: modified UTF-8.
    int block_table;    // The block ID table (see blocks.h).
    int tested;         // 1: a test joined a real server with this version.
} FernVersion;

int fern_version_count(void);
const FernVersion *fern_version_at(int i);
// Find a version by name. Give NULL for a name that is not in the table.
const FernVersion *fern_version_find(const char *id);
// The newest version: b1.7.3.
const FernVersion *fern_version_latest(void);

#endif
