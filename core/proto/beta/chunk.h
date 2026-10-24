// chunk.h: write the block data of the server into the world model.
#ifndef FERN_CHUNK_H
#define FERN_CHUNK_H

#include "packet.h"
#include "world.h"

// Inflate a map chunk packet (0x33) and write its box into the world.
// Make the columns that it touches. Give 0 if the data is bad.
int fern_chunk_apply(FernWorld *w, const FernVersion *v, const FernPacket *p);
// Write a multi block change (0x34) or a block change (0x35).
void fern_chunk_apply_multi(FernWorld *w, const FernVersion *v, const FernPacket *p);
void fern_chunk_apply_block(FernWorld *w, const FernVersion *v, const FernPacket *p);

#endif
