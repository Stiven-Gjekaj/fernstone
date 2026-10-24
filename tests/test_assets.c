#include <stdlib.h>
#include <zlib.h>
#include "png.h"
#include "test.h"
#include "zip.h"

static void put32(uint8_t *p, uint32_t v) {
    p[0] = (uint8_t)(v >> 24); p[1] = (uint8_t)(v >> 16); p[2] = (uint8_t)(v >> 8); p[3] = (uint8_t)v;
}

// Add one chunk to a PNG in a buffer. The CRC is not checked, so it is 0.
static size_t chunk(uint8_t *out, const char *type, const uint8_t *data, uint32_t len) {
    put32(out, len);
    memcpy(out + 4, type, 4);
    if (len) memcpy(out + 8, data, len);
    put32(out + 8 + len, 0);
    return 12 + len;
}

TEST(png_decodes_rgba_with_filters) {
    // 2 x 2 RGBA: row 0 filter "sub", row 1 filter "up".
    uint8_t raw[2 * 9] = {1, 10, 20, 30, 255, 5, 5, 5, 0,
                          2, 1, 1, 1, 0, 2, 2, 2, 0};
    uint8_t png[512], ihdr[13] = {0, 0, 0, 2, 0, 0, 0, 2, 8, 6, 0, 0, 0}, zipped[128];
    uLongf zlen = sizeof(zipped);
    size_t n = 8;
    int w, h;
    uint8_t *px;
    static const uint8_t sig[8] = {137, 80, 78, 71, 13, 10, 26, 10};
    memcpy(png, sig, 8);
    compress(zipped, &zlen, raw, sizeof(raw));
    n += chunk(png + n, "IHDR", ihdr, 13);
    n += chunk(png + n, "IDAT", zipped, (uint32_t)zlen);
    n += chunk(png + n, "IEND", NULL, 0);
    px = fern_png_decode(png, n, &w, &h);
    CHECK(px != NULL);
    CHECK_EQ(w, 2);
    CHECK_EQ(px[0], 10);
    CHECK_EQ(px[3], 255);
    CHECK_EQ(px[4], 15);   // 10 + 5
    CHECK_EQ(px[7], 255);  // 255 + 0
    CHECK_EQ(px[8], 11);   // 10 + 1
    CHECK_EQ(px[12], 17);  // 15 + 2
    free(px);
}

TEST(png_refuses_data_that_is_not_png) {
    int w, h;
    static const uint8_t junk[16] = "not a png file!";
    CHECK(fern_png_decode(junk, sizeof(junk), &w, &h) == NULL);
}

TEST(zip_refuses_a_missing_file) {
    CHECK(fern_zip_open("/nonexistent/fernstone.jar") == NULL);
}
