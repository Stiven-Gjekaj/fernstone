#include "packet.h"
#include "test.h"

// Each test builds the bytes of a server packet in a writer, as Poseidon
// writes them, then decodes them.

static FernDecode decode(FernWriter *w, FernPacket *p, size_t *used) {
    return fern_packet_decode(fern_version_find("b1.7.3"), w->data, w->len, p, used);
}

TEST(packet_decodes_a_handshake) {
    FernWriter w;
    FernPacket p;
    size_t used = 0;
    fern_writer_init(&w);
    fern_write_u8(&w, 0x02);
    fern_write_str16(&w, "-");
    CHECK_EQ(decode(&w, &p, &used), FERN_DECODE_OK);
    CHECK_EQ(used, w.len);
    CHECK_STR(p.u.str.text, "-");
    fern_writer_free(&w);
}

TEST(packet_asks_for_more_of_a_short_packet) {
    FernWriter w;
    FernPacket p;
    size_t used = 0;
    fern_writer_init(&w);
    fern_write_u8(&w, 0x01);
    fern_write_i32(&w, 42);
    fern_write_str16(&w, "");
    fern_write_i64(&w, 123);
    CHECK_EQ(decode(&w, &p, &used), FERN_DECODE_MORE);
    fern_write_u8(&w, 0);
    CHECK_EQ(decode(&w, &p, &used), FERN_DECODE_OK);
    CHECK_EQ(p.u.login.entity, 42);
    CHECK_EQ(p.u.login.seed, 123);
    fern_writer_free(&w);
}

TEST(packet_refuses_an_unknown_id) {
    FernWriter w;
    FernPacket p;
    size_t used = 0;
    fern_writer_init(&w);
    fern_write_u8(&w, 0xF0);
    CHECK_EQ(decode(&w, &p, &used), FERN_DECODE_BAD);
    fern_writer_free(&w);
}

TEST(packet_reads_the_stance_of_a_server_move_second) {
    FernWriter w;
    FernPacket p;
    size_t used = 0;
    fern_writer_init(&w);
    fern_write_u8(&w, 0x0D);
    fern_write_f64(&w, 10.5);
    fern_write_f64(&w, 66.62);
    fern_write_f64(&w, 65.0);
    fern_write_f64(&w, -3.5);
    fern_write_f32(&w, 90.0f);
    fern_write_f32(&w, 0.0f);
    fern_write_u8(&w, 1);
    CHECK_EQ(decode(&w, &p, &used), FERN_DECODE_OK);
    CHECK(p.u.move.y == 65.0);
    CHECK(p.u.move.stance == 66.62);
    CHECK(p.u.move.z == -3.5);
    CHECK_EQ(used, 42);
    fern_writer_free(&w);
}

TEST(packet_reads_mob_metadata) {
    FernWriter w;
    FernPacket p;
    size_t used = 0;
    fern_writer_init(&w);
    fern_write_u8(&w, 0x18);
    fern_write_i32(&w, 7);
    fern_write_u8(&w, 90);
    fern_write_i32(&w, 32);
    fern_write_i32(&w, 64 * 32);
    fern_write_i32(&w, 0);
    fern_write_u8(&w, 0);
    fern_write_u8(&w, 0);
    fern_write_u8(&w, 0x00); // byte, index 0
    fern_write_u8(&w, 2);
    fern_write_u8(&w, 0x50); // string, index 16
    fern_write_str16(&w, "x");
    fern_write_u8(&w, 0x7F);
    fern_write_u8(&w, 0x00); // The next packet.
    CHECK_EQ(decode(&w, &p, &used), FERN_DECODE_OK);
    CHECK_EQ(used, w.len - 1);
    CHECK_EQ(p.u.mob.meta.count, 2);
    CHECK_EQ(p.u.mob.meta.entry[0].value, 2);
    CHECK_EQ(p.u.mob.meta.entry[1].index, 16);
    fern_writer_free(&w);
}

TEST(packet_skips_the_speed_of_a_thrown_object) {
    FernWriter w;
    FernPacket p;
    size_t used = 0;
    fern_writer_init(&w);
    fern_write_u8(&w, 0x17);
    fern_write_i32(&w, 9);
    fern_write_u8(&w, 63);
    fern_write_i32(&w, 0);
    fern_write_i32(&w, 0);
    fern_write_i32(&w, 0);
    fern_write_i32(&w, 5); // The thrower, so three shorts follow.
    fern_write_i16(&w, 1);
    fern_write_i16(&w, 2);
    fern_write_i16(&w, 3);
    CHECK_EQ(decode(&w, &p, &used), FERN_DECODE_OK);
    CHECK_EQ(used, w.len);
    fern_writer_free(&w);
}

TEST(packet_reads_window_items) {
    FernWriter w;
    FernPacket p;
    size_t used = 0;
    fern_writer_init(&w);
    fern_write_u8(&w, 0x68);
    fern_write_u8(&w, 0);
    fern_write_i16(&w, 2);
    fern_write_i16(&w, -1);
    fern_write_i16(&w, 4);
    fern_write_u8(&w, 64);
    fern_write_i16(&w, 0);
    CHECK_EQ(decode(&w, &p, &used), FERN_DECODE_OK);
    CHECK_EQ(used, w.len);
    CHECK_EQ(p.u.items.items[0].id, -1);
    CHECK_EQ(p.u.items.items[1].id, 4);
    CHECK_EQ(p.u.items.items[1].count, 64);
    fern_writer_free(&w);
}

TEST(packet_writes_a_login) {
    FernWriter w;
    fern_writer_init(&w);
    fern_send_login(&w, fern_version_find("b1.7.3"), "Ab");
    // ID, protocol, name (2 + 2 * 2), seed, dimension.
    CHECK_EQ(w.len, 1 + 4 + 6 + 8 + 1);
    CHECK_EQ(w.data[4], 14);
    fern_writer_free(&w);
}

TEST(packet_writes_an_empty_place_item) {
    FernWriter w;
    fern_writer_init(&w);
    fern_send_place(&w, fern_version_find("b1.7.3"), -1, 255, -1, 255, NULL);
    CHECK_EQ(w.len, 1 + 4 + 1 + 4 + 1 + 2);
    CHECK_EQ(w.data[11], 0xFF);
    CHECK_EQ(w.data[12], 0xFF);
    fern_writer_free(&w);
}

TEST(packet_reads_the_records_of_an_explosion) {
    FernWriter w;
    FernPacket p;
    size_t used = 0;
    fern_writer_init(&w);
    fern_write_u8(&w, 0x3C);
    fern_write_f64(&w, 1.5);
    fern_write_f64(&w, 64.0);
    fern_write_f64(&w, -2.5);
    fern_write_f32(&w, 4.0f);
    fern_write_i32(&w, 2);
    fern_write_u8(&w, 1);
    fern_write_u8(&w, 0);
    fern_write_u8(&w, 0xFF);
    fern_write_u8(&w, 0);
    fern_write_u8(&w, 2);
    fern_write_u8(&w, 0);
    CHECK_EQ(decode(&w, &p, &used), FERN_DECODE_OK);
    CHECK_EQ(used, w.len);
    CHECK_EQ(p.u.explosion.count, 2);
    CHECK_EQ((int8_t)p.u.explosion.records[2], -1);
    fern_writer_free(&w);
}
