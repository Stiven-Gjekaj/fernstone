#include "buf.h"
#include "test.h"

TEST(buf_reads_big_endian) {
    static const uint8_t data[] = {0x12, 0x34, 0xFF, 0xFF, 0xFF, 0xFE, 0x01};
    FernReader r;
    fern_reader_init(&r, data, sizeof(data));
    CHECK_EQ(fern_read_i16(&r), 0x1234);
    CHECK_EQ(fern_read_i32(&r), -2);
    CHECK_EQ(fern_read_u8(&r), 1);
    CHECK(fern_reader_ok(&r));
}

TEST(buf_marks_a_short_read) {
    static const uint8_t data[] = {0x00, 0x01, 0x02};
    FernReader r;
    fern_reader_init(&r, data, sizeof(data));
    CHECK_EQ(fern_read_i32(&r), 0);
    CHECK(r.short_read);
    CHECK_EQ(fern_read_u8(&r), 0);
}

TEST(buf_writes_and_reads_numbers) {
    FernWriter w;
    FernReader r;
    fern_writer_init(&w);
    fern_write_i64(&w, -1234567890123LL);
    fern_write_f64(&w, 64.5);
    fern_write_f32(&w, -0.25f);
    fern_reader_init(&r, w.data, w.len);
    CHECK_EQ(fern_read_i64(&r), -1234567890123LL);
    CHECK(fern_read_f64(&r) == 64.5);
    CHECK(fern_read_f32(&r) == -0.25f);
    CHECK(fern_reader_ok(&r));
    fern_writer_free(&w);
}

TEST(buf_writes_a_utf16_string) {
    FernWriter w;
    fern_writer_init(&w);
    fern_write_str16(&w, "Ab\xC3\xA9");
    CHECK_EQ(w.len, 2 + 3 * 2);
    CHECK_EQ(w.data[1], 3);
    CHECK_EQ(w.data[3], 'A');
    CHECK_EQ(w.data[6], 0x00);
    CHECK_EQ(w.data[7], 0xE9);
    fern_writer_free(&w);
}

TEST(buf_reads_a_utf16_string) {
    static const uint8_t data[] = {0x00, 0x03, 0x00, 0xA7, 0x00, 'e', 0x00, 'x'};
    char out[16];
    FernReader r;
    fern_reader_init(&r, data, sizeof(data));
    fern_read_str16(&r, out, sizeof(out));
    CHECK_STR(out, "\xC2\xA7" "ex");
    CHECK(fern_reader_ok(&r));
}

TEST(buf_cuts_a_long_string) {
    FernWriter w;
    FernReader r;
    char out[4];
    fern_writer_init(&w);
    fern_write_str16(&w, "abcdef");
    fern_write_u8(&w, 9);
    fern_reader_init(&r, w.data, w.len);
    fern_read_str16(&r, out, sizeof(out));
    CHECK_STR(out, "abc");
    CHECK_EQ(fern_read_u8(&r), 9);
    fern_writer_free(&w);
}

TEST(buf_refuses_a_negative_string_length) {
    static const uint8_t data[] = {0xFF, 0xFF};
    char out[8];
    FernReader r;
    fern_reader_init(&r, data, sizeof(data));
    fern_read_str16(&r, out, sizeof(out));
    CHECK(r.bad);
}

TEST(buf_writes_and_reads_a_utf8_string) {
    FernWriter w;
    FernReader r;
    char out[16];
    fern_writer_init(&w);
    fern_write_str8(&w, "Notch");
    CHECK_EQ(w.len, 7);
    fern_reader_init(&r, w.data, w.len);
    fern_read_str8(&r, out, sizeof(out));
    CHECK_STR(out, "Notch");
    fern_writer_free(&w);
}
