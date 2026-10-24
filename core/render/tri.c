#include "tri.h"
#include "gl.h"
#include <stdio.h>

static GLuint program;

static const char *vertex_src =
    "attribute vec2 a_pos;\n"
    "attribute vec3 a_color;\n"
    "varying vec3 v_color;\n"
    "void main() { v_color = a_color; gl_Position = vec4(a_pos, 0.0, 1.0); }\n";

static const char *fragment_src =
    "precision mediump float;\n"
    "varying vec3 v_color;\n"
    "void main() { gl_FragColor = vec4(v_color, 1.0); }\n";

static GLuint compile(GLenum type, const char *src) {
    GLuint shader = glCreateShader(type);
    GLint ok = 0;
    glShaderSource(shader, 1, &src, NULL);
    glCompileShader(shader);
    glGetShaderiv(shader, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        char log[512];
        glGetShaderInfoLog(shader, sizeof(log), NULL, log);
        fprintf(stderr, "tri: shader: %s\n", log);
        glDeleteShader(shader);
        return 0;
    }
    return shader;
}

int fern_tri_init(void) {
    GLuint vs = compile(GL_VERTEX_SHADER, vertex_src);
    GLuint fs = compile(GL_FRAGMENT_SHADER, fragment_src);
    GLint ok = 0;
    if (!vs || !fs) return 0;
    program = glCreateProgram();
    glAttachShader(program, vs);
    glAttachShader(program, fs);
    glBindAttribLocation(program, 0, "a_pos");
    glBindAttribLocation(program, 1, "a_color");
    glLinkProgram(program);
    glGetProgramiv(program, GL_LINK_STATUS, &ok);
    return ok != 0;
}

void fern_tri_draw(int w, int h) {
    static const GLfloat pos[] = {0.0f, 0.7f, -0.7f, -0.6f, 0.7f, -0.6f};
    static const GLfloat color[] = {0.42f, 0.62f, 0.26f, 0.48f, 0.48f, 0.48f, 0.30f, 0.45f, 0.20f};
    glViewport(0, 0, w, h);
    glClearColor(0.53f, 0.70f, 0.94f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    glUseProgram(program);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, pos);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 0, color);
    glEnableVertexAttribArray(0);
    glEnableVertexAttribArray(1);
    glDrawArrays(GL_TRIANGLES, 0, 3);
}
