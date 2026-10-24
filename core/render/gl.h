// gl.h: include the GLES2 header of the platform.
// iOS 6 uses the system OpenGL ES. The iOS and macOS shells use MetalANGLE.
#ifndef FERN_GL_H
#define FERN_GL_H

#if defined(FERN_ANGLE)
#include <GLES2/gl2.h>
#elif defined(__APPLE__)
#include <TargetConditionals.h>
#if TARGET_OS_IPHONE
#include <OpenGLES/ES2/gl.h>
#else
#error "macOS needs MetalANGLE: define FERN_ANGLE"
#endif
#else
#include <GLES2/gl2.h>
#endif

#endif
