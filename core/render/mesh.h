// mesh.h: build the vertices of one chunk section from a copy of its
// blocks. The mesher reads only the copy, so it can run on a second thread
// while the main thread changes the world.
#ifndef FERN_MESH_H
#define FERN_MESH_H

#include <stdint.h>
#include "world.h"

#define FERN_SNAP 18   // The section and one block on each side.

// One vertex: 16 bytes.
typedef struct {
    int16_t x, y, z, pad;  // 1/16 block, from the corner of the section.
    uint16_t u, v;         // The atlas position, 0 to 65535.
    uint8_t r, g, b;       // The tint and the shade of the face.
    uint8_t light;         // Sky light << 4 | block light.
} FernVertex;

typedef struct {
    FernVertex *v;
    int count, cap;
} FernVertexList;

typedef struct {
    int sx, sy, sz;                          // The section position (in sections).
    FernState state[FERN_SNAP * FERN_SNAP * FERN_SNAP];
    uint8_t light[FERN_SNAP * FERN_SNAP * FERN_SNAP];
} FernMeshInput;

typedef struct {
    int sx, sy, sz;
    FernVertexList solid;                    // Opaque and cutout faces.
    FernVertexList water;                    // Translucent faces.
} FernMeshOutput;

// Copy the blocks of a section and the blocks around it from the world.
// Give 0 if the section and its neighbors have no blocks.
int fern_mesh_snapshot(FernWorld *w, int sx, int sy, int sz, FernMeshInput *in);
// Build the quads of a section. Four vertices make one quad.
void fern_mesh_build(const FernMeshInput *in, FernMeshOutput *out);
void fern_mesh_free(FernMeshOutput *out);

#endif
