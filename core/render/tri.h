// tri.h: draw one colored triangle. It proves the GLES2 path of a shell.
#ifndef FERN_TRI_H
#define FERN_TRI_H

// Compile the shader. Return 0 if the shader does not compile.
int fern_tri_init(void);
// Clear the frame and draw the triangle in a viewport of w x h pixels.
void fern_tri_draw(int w, int h);

#endif
