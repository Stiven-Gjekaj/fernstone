#include "manifest.h"
#include "test.h"
#include "version.h"

TEST(manifest_parses_the_format_of_the_spec) {
    FernManifest m;
    CHECK(fern_manifest_parse("{ \"id\": \"b1.7.3\", \"era\": \"beta\", "
                              "\"min\": { \"ramMB\": 256, \"gl\": \"es2\" }, "
                              "\"recommended\": { \"ramMB\": 512, \"gl\": \"es2\", \"benchScore\": 10 } }",
                              &m));
    CHECK_STR(m.id, "b1.7.3");
    CHECK_STR(m.era, "beta");
    CHECK_EQ(m.min.ram_mb, 256);
    CHECK_EQ(m.min.bench, 0);
    CHECK_EQ(m.rec.ram_mb, 512);
    CHECK_EQ(m.rec.bench, 10);
    CHECK(!fern_manifest_parse("{ \"id\": \"x\" }", &m));
}

TEST(manifest_has_one_entry_for_each_version) {
    int i;
    for (i = 0; i < fern_version_count(); i++) CHECK(fern_manifest_find(fern_version_at(i)->id) != NULL);
    CHECK_EQ(fern_manifest_count(), fern_version_count());
}

TEST(manifest_tags_the_newest_version_that_meets_recommended) {
    // Made-up manifests are not possible here, so the test uses the built-in
    // ones and devices that are above, between, and under the needs.
    const char *ids[3] = {"b1.7.3", "b1.6.6", "a1.2.6"};
    FernTag tags[3];
    FernDevice strong = {512, 2, 30}, slow = {512, 2, 3}, tiny = {128, 2, 30};
    const FernManifest *m = fern_manifest_find("b1.7.3");
    CHECK(m && strong.bench >= m->rec.bench && slow.bench < m->rec.bench && tiny.ram_mb < m->min.ram_mb);
    fern_manifest_tags(&strong, ids, 3, tags);
    CHECK_EQ(tags[0], TAG_RECOMMENDED);
    CHECK_EQ(tags[1], TAG_NONE);
    fern_manifest_tags(&slow, ids, 3, tags);
    CHECK_EQ(tags[0], TAG_MAY_LAG);
    fern_manifest_tags(&tiny, ids, 3, tags);
    CHECK_EQ(tags[2], TAG_TOO_OLD_DEVICE);
}

TEST(manifest_rounds_the_memory_of_the_4s) {
    CHECK_EQ(fern_device_ram(505.0), 512);
    CHECK_EQ(fern_device_ram(16384.0), 16384);
    CHECK_EQ(fern_device_ram(7680.0), 7680);
}
