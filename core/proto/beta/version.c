#include "version.h"
#include <string.h>

// Milestones 1 to 5 use b1.7.3 only. Milestone 6 adds the older versions.
static const FernVersion versions[] = {
    {"b1.7.3", "beta", 14, 1, 14},
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
