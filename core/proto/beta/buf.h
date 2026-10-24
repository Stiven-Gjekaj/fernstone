// buf.h: read and write the big-endian fields of the pre-netty protocol.
// A reader never reads past its end. On a short read it sets "short_read",
// and each later read gives zero. The caller then waits for more bytes.
#ifndef FERN_BUF_H
#define FERN_BUF_H

#include <stddef.h>
#include <stdint.h>

typedef struct {
    const uint8_t *data;
    size_t len;
    size_t pos;
    int short_read; // The data stops before the field ends.
    int bad;        // The data is not valid, for example a negative length.
} FernReader;

typedef struct {
    uint8_t *data;
    size_t len;
    size_t cap;
} FernWriter;

void fern_reader_init(FernReader *r, const uint8_t *data, size_t len);
int fern_reader_ok(const FernReader *r);
uint8_t fern_read_u8(FernReader *r);
int8_t fern_read_i8(FernReader *r);
int16_t fern_read_i16(FernReader *r);
uint16_t fern_read_u16(FernReader *r);
int32_t fern_read_i32(FernReader *r);
int64_t fern_read_i64(FernReader *r);
float fern_read_f32(FernReader *r);
double fern_read_f64(FernReader *r);
// Point at n bytes in the data, and skip them. Give NULL on a short read.
const uint8_t *fern_read_bytes(FernReader *r, size_t n);
// Read a UTF-16BE string with a 16-bit count of code units (b1.5 and later).
// Write it as UTF-8 with a NUL into out. Cut it when out is full.
void fern_read_str16(FernReader *r, char *out, size_t cap);
// Read a modified UTF-8 string with a 16-bit count of bytes (writeUTF).
void fern_read_str8(FernReader *r, char *out, size_t cap);

void fern_writer_init(FernWriter *w);
void fern_writer_free(FernWriter *w);
void fern_writer_reset(FernWriter *w);
void fern_write_u8(FernWriter *w, uint8_t v);
void fern_write_i16(FernWriter *w, int16_t v);
void fern_write_i32(FernWriter *w, int32_t v);
void fern_write_i64(FernWriter *w, int64_t v);
void fern_write_f32(FernWriter *w, float v);
void fern_write_f64(FernWriter *w, double v);
void fern_write_bytes(FernWriter *w, const void *data, size_t n);
// Write a UTF-8 string as UTF-16BE with a 16-bit count of code units.
void fern_write_str16(FernWriter *w, const char *utf8);
// Write a UTF-8 string as modified UTF-8 with a 16-bit count of bytes.
void fern_write_str8(FernWriter *w, const char *utf8);

#endif
