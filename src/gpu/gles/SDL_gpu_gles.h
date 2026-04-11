/*
  SDL3 GPU backend for OpenGL ES — internal header
  MVP: device creation, swapchain, render pass clear, submit
*/
#include "SDL_internal.h"

#ifndef SDL_gpu_gles_h
#define SDL_gpu_gles_h

#ifdef SDL_GPU_GLES

#include "../SDL_sysgpu.h"

/* ======================================================================== */
/* GL type definitions (avoid pulling in GL headers)                        */
/* ======================================================================== */

typedef unsigned int GLenum;
typedef unsigned int GLuint;
typedef int GLint;
typedef int GLsizei;
typedef unsigned char GLboolean;
typedef float GLfloat;
typedef unsigned int GLbitfield;
typedef void GLvoid;
typedef signed long int GLsizeiptr;
typedef char GLchar;
typedef float GLclampf;

/* GL constants we use */
#define GL_COLOR_BUFFER_BIT     0x00004000
#define GL_DEPTH_BUFFER_BIT     0x00000100
#define GL_STENCIL_BUFFER_BIT   0x00000400
#define GL_FRAMEBUFFER          0x8D40
#define GL_TRUE                 1
#define GL_FALSE                0
#define GL_VERSION              0x1F02
#define GL_SCISSOR_TEST         0x0C11
#define GL_DEPTH_TEST           0x0B71
#define GL_BLEND                0x0BE2
#define GL_CULL_FACE            0x0B44
#define GL_NO_ERROR             0

/* ======================================================================== */
/* GL function pointer types                                               */
/* ======================================================================== */

typedef void (*PFNGLCLEARCOLORPROC)(GLclampf r, GLclampf g, GLclampf b, GLclampf a);
typedef void (*PFNGLCLEARDEPTHFPROC)(GLclampf d);
typedef void (*PFNGLCLEARSTENCILPROC)(GLint s);
typedef void (*PFNGLCLEARPROC)(GLbitfield mask);
typedef void (*PFNGLVIEWPORTPROC)(GLint x, GLint y, GLsizei w, GLsizei h);
typedef void (*PFNGLSCISSORPROC)(GLint x, GLint y, GLsizei w, GLsizei h);
typedef void (*PFNGLENABLEPROC)(GLenum cap);
typedef void (*PFNGLDISABLEPROC)(GLenum cap);
typedef void (*PFNGLFINISHPROC)(void);
typedef void (*PFNGLFLUSHPROC)(void);
typedef GLenum (*PFNGLGETERRORPROC)(void);
typedef const GLchar *(*PFNGLGETSTRINGPROC)(GLenum name);
typedef void (*PFNGLCOLORMASKPROC)(GLboolean r, GLboolean g, GLboolean b, GLboolean a);
typedef void (*PFNGLDEPTHMASKPROC)(GLboolean flag);
typedef void (*PFNGLSTENCILMASKPROC)(GLuint mask);
typedef void (*PFNGLBINDFRAMEBUFFERPROC)(GLenum target, GLuint fbo);

/* ======================================================================== */
/* Backend renderer state                                                  */
/* ======================================================================== */

typedef struct GLESRenderer
{
    /* GL function pointers */
    PFNGLCLEARCOLORPROC glClearColor;
    PFNGLCLEARDEPTHFPROC glClearDepthf;
    PFNGLCLEARSTENCILPROC glClearStencil;
    PFNGLCLEARPROC glClear;
    PFNGLVIEWPORTPROC glViewport;
    PFNGLSCISSORPROC glScissor;
    PFNGLENABLEPROC glEnable;
    PFNGLDISABLEPROC glDisable;
    PFNGLFINISHPROC glFinish;
    PFNGLFLUSHPROC glFlush;
    PFNGLGETERRORPROC glGetError;
    PFNGLGETSTRINGPROC glGetString;
    PFNGLCOLORMASKPROC glColorMask;
    PFNGLDEPTHMASKPROC glDepthMask;
    PFNGLSTENCILMASKPROC glStencilMask;
    PFNGLBINDFRAMEBUFFERPROC glBindFramebuffer;

    /* Claimed windows (simple: one window for MVP) */
    SDL_Window *claimed_window;

    /* Debug mode */
    bool debug_mode;
} GLESRenderer;

/* ======================================================================== */
/* Command buffer                                                          */
/* ======================================================================== */

typedef struct GLESCommandBuffer
{
    CommandBufferCommonHeader header;
    GLESRenderer *renderer;

    /* Swapchain texture acquired this frame */
    SDL_Window *swapchain_window;
} GLESCommandBuffer;

/* ======================================================================== */
/* Swapchain "texture" — represents the default framebuffer                */
/* ======================================================================== */

typedef struct GLESSwapchainTexture
{
    TextureCommonHeader header;
} GLESSwapchainTexture;

#endif /* SDL_GPU_GLES */
#endif /* SDL_gpu_gles_h */
