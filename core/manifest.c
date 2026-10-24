#include "manifest.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "manifest_data.h"

// Find the value of a key after a position, in a small flat JSON text.
static const char *after_key(const char *json, const char *key) {
    char pat[64];
    const char *p;
    size_t n = (size_t)snprintf(pat, sizeof(pat), "\"%s\"", key);
    p = strstr(json, pat);
    if (!p) return NULL;
    p += n;
    while (*p == ' ' || *p == ':' || *p == '\t' || *p == '\n') p++;
    return p;
}

static int read_string(const char *json, const char *key, char *out, size_t cap) {
    const char *p = after_key(json, key);
    size_t n = 0;
    if (!p || *p != '"') return 0;
    p++;
    while (*p && *p != '"' && n + 1 < cap) out[n++] = *p++;
    out[n] = 0;
    return 1;
}

static int read_needs(const char *json, const char *key, FernNeeds *n) {
    const char *p = after_key(json, key), *end, *v;
    char gl[8];
    if (!p || *p != '{') return 0;
    end = strchr(p, '}');
    if (!end) return 0;
    {
        char part[256];
        size_t len = (size_t)(end - p + 1);
        if (len >= sizeof(part)) return 0;
        memcpy(part, p, len);
        part[len] = 0;
        v = after_key(part, "ramMB");
        if (!v) return 0;
        n->ram_mb = atoi(v);
        if (!read_string(part, "gl", gl, sizeof(gl))) return 0;
        n->gl = !strcmp(gl, "es3") ? 3 : 2;
        v = after_key(part, "benchScore");
        n->bench = v ? atoi(v) : 0;
    }
    return 1;
}

int fern_manifest_parse(const char *json, FernManifest *m) {
    memset(m, 0, sizeof(*m));
    return read_string(json, "id", m->id, sizeof(m->id)) && read_string(json, "era", m->era, sizeof(m->era)) &&
           read_needs(json, "min", &m->min) && read_needs(json, "recommended", &m->rec);
}

static FernManifest table[64];
static int loaded = -1;

static void load(void) {
    int i;
    if (loaded >= 0) return;
    loaded = 0;
    for (i = 0; fern_manifest_json[i] && loaded < 64; i++)
        if (fern_manifest_parse(fern_manifest_json[i], &table[loaded])) loaded++;
}

int fern_manifest_count(void) {
    load();
    return loaded;
}

const FernManifest *fern_manifest_find(const char *id) {
    int i;
    load();
    for (i = 0; i < loaded; i++)
        if (!strcmp(table[i].id, id)) return &table[i];
    return NULL;
}

static int meets(const FernDevice *d, const FernNeeds *n) {
    return d->ram_mb >= n->ram_mb && d->gl >= n->gl && d->bench >= n->bench;
}

void fern_manifest_tags(const FernDevice *d, const char *const *ids, int n, FernTag *tags) {
    int i, recommended = 0;
    for (i = 0; i < n; i++) {
        const FernManifest *m = fern_manifest_find(ids[i]);
        if (!m || !meets(d, &m->min)) {
            tags[i] = TAG_TOO_OLD_DEVICE;
        } else if (meets(d, &m->rec)) {
            tags[i] = recommended ? TAG_NONE : TAG_RECOMMENDED;
            recommended = 1;
        } else {
            tags[i] = TAG_MAY_LAG;
        }
    }
}

int fern_device_ram(double physical_mb) {
    return (int)((physical_mb + 32) / 64) * 64;
}
