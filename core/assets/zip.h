// zip.h: read files from a jar (a zip file) that the user owns.
#ifndef FERN_ZIP_H
#define FERN_ZIP_H

#include <stddef.h>
#include <stdint.h>

typedef struct FernZip FernZip;

FernZip *fern_zip_open(const char *path);
// Read a file of the archive into a new buffer. Give NULL if it is not there.
uint8_t *fern_zip_read(FernZip *z, const char *name, size_t *len);
void fern_zip_close(FernZip *z);

#endif
