/*
  SDL2 backend - window management
*/
#include "SDL_internal.h"

#ifdef SDL_VIDEO_DRIVER_SDL2

#include "../SDL_sysvideo.h"
#include "../../events/SDL_windowevents_c.h"
#include "SDL_sdl2video.h"
#include "SDL_sdl2window.h"

/* SDL2 window position constants */
#define SDL2_WINDOWPOS_CENTERED  0x2FFF0000

bool SDL2_CreateSDLWindow(SDL_VideoDevice *_this, SDL_Window *window, SDL_PropertiesID create_props)
{
    SDL_WindowData *data = (SDL_WindowData *)SDL_calloc(1, sizeof(SDL_WindowData));
    if (!data) {
        return false;
    }
    window->internal = data;

    /* Map SDL3 window flags to SDL2 flags.
     * Always add OPENGL because our GLES GPU backend needs GL contexts,
     * and on the r36s EGL is always available through libmali. */
    Uint32 sdl2_flags = SDL2_WINDOW_SHOWN | SDL2_WINDOW_OPENGL;
    if (window->flags & SDL_WINDOW_BORDERLESS) {
        sdl2_flags |= SDL2_WINDOW_BORDERLESS;
    }
    if (window->flags & SDL_WINDOW_RESIZABLE) {
        sdl2_flags |= SDL2_WINDOW_RESIZABLE;
    }
    if (window->flags & SDL_WINDOW_MINIMIZED) {
        sdl2_flags |= SDL2_WINDOW_MINIMIZED;
    }
    if (window->flags & SDL_WINDOW_MAXIMIZED) {
        sdl2_flags |= SDL2_WINDOW_MAXIMIZED;
    }
    if (window->flags & SDL_WINDOW_FULLSCREEN) {
        sdl2_flags |= SDL2_WINDOW_FULLSCREEN_DESKTOP;
    }

    int x = (window->x == SDL_WINDOWPOS_UNDEFINED || window->x == SDL_WINDOWPOS_CENTERED)
            ? SDL2_WINDOWPOS_CENTERED : window->x;
    int y = (window->y == SDL_WINDOWPOS_UNDEFINED || window->y == SDL_WINDOWPOS_CENTERED)
            ? SDL2_WINDOWPOS_CENTERED : window->y;

    if (!SDL2_CreateWindow) {
        return SDL_SetError("SDL2_CreateWindow not available");
    }

    data->sdl2_window = SDL2_CreateWindow(
        window->title ? window->title : "",
        x, y, window->w, window->h, sdl2_flags
    );

    if (!data->sdl2_window) {
        SDL_free(data);
        window->internal = NULL;
        return SDL_SetError("SDL2_CreateWindow failed: %s",
                            SDL2_GetError ? SDL2_GetError() : "unknown");
    }

    data->sdl2_window_id = SDL2_GetWindowID ? SDL2_GetWindowID(data->sdl2_window) : 0;

    return true;
}

void SDL2_DestroySDLWindow(SDL_VideoDevice *_this, SDL_Window *window)
{
    SDL_WindowData *data = window->internal;
    if (data) {
        if (data->sdl2_gl_context && SDL2_GL_DeleteContext) {
            SDL2_GL_DeleteContext(data->sdl2_gl_context);
        }
        if (data->sdl2_window && SDL2_DestroyWindow) {
            SDL2_DestroyWindow(data->sdl2_window);
        }
        SDL_free(data);
    }
    window->internal = NULL;
}

void SDL2_SetSDLWindowTitle(SDL_VideoDevice *_this, SDL_Window *window)
{
    SDL_WindowData *data = window->internal;
    if (data && data->sdl2_window && SDL2_SetWindowTitle) {
        SDL2_SetWindowTitle(data->sdl2_window, window->title ? window->title : "");
    }
}

bool SDL2_SetSDLWindowPosition(SDL_VideoDevice *_this, SDL_Window *window)
{
    SDL_WindowData *data = window->internal;
    if (data && data->sdl2_window && SDL2_SetWindowPosition) {
        SDL2_SetWindowPosition(data->sdl2_window, window->pending.x, window->pending.y);
    }
    return true;
}

void SDL2_SetSDLWindowSize(SDL_VideoDevice *_this, SDL_Window *window)
{
    SDL_WindowData *data = window->internal;
    if (data && data->sdl2_window && SDL2_SetWindowSize) {
        SDL2_SetWindowSize(data->sdl2_window, window->pending.w, window->pending.h);
        SDL_SendWindowEvent(window, SDL_EVENT_WINDOW_RESIZED, window->pending.w, window->pending.h);
    }
}

void SDL2_ShowSDLWindow(SDL_VideoDevice *_this, SDL_Window *window)
{
    SDL_WindowData *data = window->internal;
    if (data && data->sdl2_window && SDL2_ShowWindow) {
        SDL2_ShowWindow(data->sdl2_window);
    }
}

void SDL2_HideSDLWindow(SDL_VideoDevice *_this, SDL_Window *window)
{
    SDL_WindowData *data = window->internal;
    if (data && data->sdl2_window && SDL2_HideWindow) {
        SDL2_HideWindow(data->sdl2_window);
    }
}

void SDL2_RaiseSDLWindow(SDL_VideoDevice *_this, SDL_Window *window)
{
    SDL_WindowData *data = window->internal;
    if (data && data->sdl2_window && SDL2_RaiseWindow) {
        SDL2_RaiseWindow(data->sdl2_window);
    }
}

void SDL2_MaximizeSDLWindow(SDL_VideoDevice *_this, SDL_Window *window)
{
    SDL_WindowData *data = window->internal;
    if (data && data->sdl2_window && SDL2_MaximizeWindow) {
        SDL2_MaximizeWindow(data->sdl2_window);
    }
}

void SDL2_MinimizeSDLWindow(SDL_VideoDevice *_this, SDL_Window *window)
{
    SDL_WindowData *data = window->internal;
    if (data && data->sdl2_window && SDL2_MinimizeWindow) {
        SDL2_MinimizeWindow(data->sdl2_window);
    }
}

void SDL2_RestoreSDLWindow(SDL_VideoDevice *_this, SDL_Window *window)
{
    SDL_WindowData *data = window->internal;
    if (data && data->sdl2_window && SDL2_RestoreWindow) {
        SDL2_RestoreWindow(data->sdl2_window);
    }
}

SDL_FullscreenResult SDL2_SetSDLWindowFullscreen(SDL_VideoDevice *_this, SDL_Window *window,
                                                  SDL_VideoDisplay *display, SDL_FullscreenOp fullscreen)
{
    SDL_WindowData *data = window->internal;
    if (data && data->sdl2_window && SDL2_SetWindowFullscreen) {
        Uint32 flags = (fullscreen == SDL_FULLSCREEN_OP_ENTER) ? SDL2_WINDOW_FULLSCREEN_DESKTOP : 0;
        SDL2_SetWindowFullscreen(data->sdl2_window, flags);
    }
    return SDL_FULLSCREEN_SUCCEEDED;
}

void SDL2_GetSDLWindowSizeInPixels(SDL_VideoDevice *_this, SDL_Window *window, int *w, int *h)
{
    SDL_WindowData *data = window->internal;
    if (data && data->sdl2_window && SDL2_GL_GetDrawableSize) {
        SDL2_GL_GetDrawableSize(data->sdl2_window, w, h);
    } else if (data && data->sdl2_window && SDL2_GetWindowSize) {
        SDL2_GetWindowSize(data->sdl2_window, w, h);
    }
}

#endif /* SDL_VIDEO_DRIVER_SDL2 */
