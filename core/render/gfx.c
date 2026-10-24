#include "gfx.h"
#include <stddef.h>
#include "log.h"

static GLuint compile(GLenum type, const char *src) {
    GLuint s = glCreateShader(type);
    GLint ok = 0;
    glShaderSource(s, 1, &src, NULL);
    glCompileShader(s);
    glGetShaderiv(s, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        char log[1024];
        glGetShaderInfoLog(s, sizeof(log), NULL, log);
        fern_log("gfx: the shader does not compile: %s", log);
        glDeleteShader(s);
        return 0;
    }
    return s;
}

GLuint fern_gl_program(const char *vs, const char *fs, const char *const *attribs) {
    GLuint v = compile(GL_VERTEX_SHADER, vs), f = compile(GL_FRAGMENT_SHADER, fs), p;
    GLint ok = 0;
    int i;
    if (!v || !f) return 0;
    p = glCreateProgram();
    glAttachShader(p, v);
    glAttachShader(p, f);
    for (i = 0; attribs[i]; i++) glBindAttribLocation(p, (GLuint)i, attribs[i]);
    glLinkProgram(p);
    glGetProgramiv(p, GL_LINK_STATUS, &ok);
    glDeleteShader(v);
    glDeleteShader(f);
    if (!ok) {
        char log[1024];
        glGetProgramInfoLog(p, sizeof(log), NULL, log);
        fern_log("gfx: the program does not link: %s", log);
        return 0;
    }
    return p;
}

GLuint fern_gl_texture(const uint8_t *rgba, int w, int h) {
    GLuint t;
    glGenTextures(1, &t);
    glBindTexture(GL_TEXTURE_2D, t);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, rgba);
    return t;
}
