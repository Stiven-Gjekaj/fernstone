// terrain.h: draw the chunk sections of the world. Each section has one
// VBO. A worker thread on the second core builds the meshes.
#ifndef FERN_TERRAIN_H
#define FERN_TERRAIN_H

#include "gl.h"
#include "mat.h"
#include "world.h"

typedef struct FernTerrain FernTerrain;

typedef struct {
    float day;                 // The sky light, 0 (night) to 1 (day).
    float fog_start, fog_end;  // In blocks.
    float fog[3];              // The fog color.
} FernTerrainLook;

FernTerrain *fern_terrain_new(void);
void fern_terrain_set_atlas(FernTerrain *t, GLuint atlas);
// Send the dirty sections near the camera to the worker, and upload the
// meshes that it finished. radius is the render distance in chunks.
void fern_terrain_update(FernTerrain *t, FernWorld *w, double cx, double cy, double cz, int radius);
// Draw the opaque pass, then the translucent pass.
void fern_terrain_draw(FernTerrain *t, const FernMat *viewproj, double cx, double cy, double cz,
                       const FernTerrainLook *look);
// Draw a list of quads (a model) with a texture, at an origin in blocks
// from the camera. The list uses the vertex format of the terrain.
void fern_terrain_draw_quads(FernTerrain *t, const FernMat *viewproj, GLuint tex, float ox, float oy, float oz,
                             const void *vertices, int count, const FernTerrainLook *look);
// Remove each mesh, for example after a disconnect.
void fern_terrain_clear(FernTerrain *t);
// The count of sections with a mesh, and of the sections drawn last frame.
void fern_terrain_stats(FernTerrain *t, int *meshes, int *drawn, int *queued);
void fern_terrain_free(FernTerrain *t);

#endif
