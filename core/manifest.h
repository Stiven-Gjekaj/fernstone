// manifest.h: the needs of each version, and the measured device.
// Each version has a manifest with "min" and "recommended" needs. The
// version browser compares them with the numbers that it measures on the
// device: the memory, the GL version, and a chunk meshing benchmark.
#ifndef FERN_MANIFEST_H
#define FERN_MANIFEST_H

typedef struct {
    int ram_mb;
    int gl;          // 2 for "es2", 3 for "es3"
    int bench;       // 0: no need
} FernNeeds;

typedef struct {
    char id[16];
    char era[8];
    FernNeeds min, rec;
} FernManifest;

typedef struct {
    int ram_mb;      // The physical memory, rounded to 64 MB.
    int gl;          // The GLES major version of the context.
    int bench;       // The benchmark score: sections meshed in one second / 10.
} FernDevice;

typedef enum { TAG_NONE, TAG_RECOMMENDED, TAG_MAY_LAG, TAG_TOO_OLD_DEVICE } FernTag;

// Read one manifest from JSON. Give 0 if a field is missing.
int fern_manifest_parse(const char *json, FernManifest *m);
// The manifests that the app has built in.
int fern_manifest_count(void);
const FernManifest *fern_manifest_find(const char *id);
// The tag of each version in ids (newest first): the newest version that
// meets "recommended" is Recommended, a version that meets only "min" May
// lag, and a version that does not meet "min" is grayed out.
void fern_manifest_tags(const FernDevice *d, const char *const *ids, int n, FernTag *tags);
// Round the physical memory of the system to the nearest 64 MB. iOS gives
// a value a little under the true size, because the GPU keeps a part.
int fern_device_ram(double physical_mb);

#endif
