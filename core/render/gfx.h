// gfx.h: small helpers for GLES2 shaders and textures.
#ifndef FERN_GFX_H
#define FERN_GFX_H

#include <stdint.h>
#include "gl.h"

// Compile and link a program. attribs is a NULL-terminated list of the
// attribute names, bound to the locations 0, 1, 2, and on. Give 0 on an error.
GLuint fern_gl_program(const char *vs, const char *fs, const char *const *attribs);
// Make a texture with nearest filtering from RGBA pixels.
GLuint fern_gl_texture(const uint8_t *rgba, int w, int h);

#endif
