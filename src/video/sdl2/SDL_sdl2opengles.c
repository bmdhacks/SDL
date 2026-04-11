/*
  SDL2 backend - OpenGL ES context management
  All GL operations go through SDL2, preserving its patched EGL/Mali init.
*/
#include "SDL_internal.h"

#ifdef SDL_VIDEO_DRIVER_SDL2

#include "../SDL_sysvideo.h"
#include "SDL_sdl2video.h"
#include "SDL_sdl2opengles.h"

/* SDL2 GL attribute constants (must match SDL2's values) */
#define SDL2_GL_CONTEXT_PROFILE_MASK  21
#define SDL2_GL_CONTEXT_MAJOR_VERSION 17
#define SDL2_GL_CONTEXT_MINOR_VERSION 18
#define SDL2_GL_CONTEXT_PROFILE_ES    0x0004
#define SDL2_GL_RED_SIZE              0
#define SDL2_GL_GREEN_SIZE            1
#define SDL2_GL_BLUE_SIZE             2
#define SDL2_GL_ALPHA_SIZE            3
#define SDL2_GL_DEPTH_SIZE            6
#define SDL2_GL_STENCIL_SIZE          7
#define SDL2_GL_DOUBLEBUFFER          5

bool SDL2_GLES_LoadLibrary(SDL_VideoDevice *_this, const char *path)
{
    if (!SDL2_GL_SetAttribute) {
        return SDL_SetError("SDL2 GL functions not available");
    }

    /* Configure SDL2 for the GL context the app wants */
    SDL2_GL_SetAttribute(SDL2_GL_CONTEXT_PROFILE_MASK, SDL2_GL_CONTEXT_PROFILE_ES);
    SDL2_GL_SetAttribute(SDL2_GL_CONTEXT_MAJOR_VERSION, _this->gl_config.major_version);
    SDL2_GL_SetAttribute(SDL2_GL_CONTEXT_MINOR_VERSION, _this->gl_config.minor_version);
    SDL2_GL_SetAttribute(SDL2_GL_RED_SIZE, _this->gl_config.red_size);
    SDL2_GL_SetAttribute(SDL2_GL_GREEN_SIZE, _this->gl_config.green_size);
    SDL2_GL_SetAttribute(SDL2_GL_BLUE_SIZE, _this->gl_config.blue_size);
    SDL2_GL_SetAttribute(SDL2_GL_ALPHA_SIZE, _this->gl_config.alpha_size);
    SDL2_GL_SetAttribute(SDL2_GL_DEPTH_SIZE, _this->gl_config.depth_size);
    SDL2_GL_SetAttribute(SDL2_GL_STENCIL_SIZE, _this->gl_config.stencil_size);
    SDL2_GL_SetAttribute(SDL2_GL_DOUBLEBUFFER, _this->gl_config.double_buffer);

    /* SDL2 manages the actual library loading */
    _this->gl_config.driver_loaded = 1;
    return true;
}

SDL_FunctionPointer SDL2_GLES_GetProcAddress(SDL_VideoDevice *_this, const char *proc)
{
    if (!SDL2_GL_GetProcAddress) {
        return NULL;
    }
    return (SDL_FunctionPointer)SDL2_GL_GetProcAddress(proc);
}

void SDL2_GLES_UnloadLibrary(SDL_VideoDevice *_this)
{
    _this->gl_config.driver_loaded = 0;
}

SDL_GLContext SDL2_GLES_CreateContext(SDL_VideoDevice *_this, SDL_Window *window)
{
    SDL_WindowData *data = window->internal;
    if (!data || !data->sdl2_window || !SDL2_GL_CreateContext) {
        SDL_SetError("No SDL2 window for GL context creation");
        return NULL;
    }

    /* Set GL attributes before context creation */
    if (SDL2_GL_SetAttribute) {
        SDL2_GL_SetAttribute(SDL2_GL_CONTEXT_PROFILE_MASK, SDL2_GL_CONTEXT_PROFILE_ES);
        SDL2_GL_SetAttribute(SDL2_GL_CONTEXT_MAJOR_VERSION, _this->gl_config.major_version);
        SDL2_GL_SetAttribute(SDL2_GL_CONTEXT_MINOR_VERSION, _this->gl_config.minor_version);
    }

    void *ctx = SDL2_GL_CreateContext(data->sdl2_window);
    if (!ctx) {
        SDL_SetError("SDL2_GL_CreateContext failed: %s",
                     SDL2_GetError ? SDL2_GetError() : "unknown");
        return NULL;
    }

    data->sdl2_gl_context = ctx;
    return (SDL_GLContext)ctx;
}

bool SDL2_GLES_MakeCurrent(SDL_VideoDevice *_this, SDL_Window *window, SDL_GLContext context)
{
    if (!SDL2_GL_MakeCurrent) {
        return SDL_SetError("SDL2_GL_MakeCurrent not available");
    }

    void *sdl2_window = NULL;
    if (window) {
        SDL_WindowData *data = window->internal;
        if (data) {
            sdl2_window = data->sdl2_window;
        }
    }

    int result = SDL2_GL_MakeCurrent(sdl2_window, (void *)context);
    if (result != 0) {
        return SDL_SetError("SDL2_GL_MakeCurrent failed: %s",
                            SDL2_GetError ? SDL2_GetError() : "unknown");
    }
    return true;
}

bool SDL2_GLES_SetSwapInterval(SDL_VideoDevice *_this, int interval)
{
    if (!SDL2_GL_SetSwapInterval) {
        return SDL_SetError("SDL2_GL_SetSwapInterval not available");
    }
    int result = SDL2_GL_SetSwapInterval(interval);
    return (result == 0);
}

bool SDL2_GLES_GetSwapInterval(SDL_VideoDevice *_this, int *interval)
{
    if (!SDL2_GL_GetSwapInterval) {
        return SDL_SetError("SDL2_GL_GetSwapInterval not available");
    }
    if (interval) {
        *interval = SDL2_GL_GetSwapInterval();
    }
    return true;
}

bool SDL2_GLES_SwapWindow(SDL_VideoDevice *_this, SDL_Window *window)
{
    SDL_WindowData *data = window->internal;
    if (!data || !data->sdl2_window || !SDL2_GL_SwapWindow) {
        return SDL_SetError("Cannot swap: no SDL2 window");
    }
    SDL2_GL_SwapWindow(data->sdl2_window);
    return true;
}

bool SDL2_GLES_DestroyContext(SDL_VideoDevice *_this, SDL_GLContext context)
{
    if (SDL2_GL_DeleteContext) {
        SDL2_GL_DeleteContext((void *)context);
    }
    return true;
}

#endif /* SDL_VIDEO_DRIVER_SDL2 */
