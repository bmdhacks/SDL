/*
  SDL3 mouse/cursor driver backed by SDL2 via dlopen.
  See SDL_sdl2mouse.h for the rationale.
*/
#include "SDL_internal.h"

#ifdef SDL_VIDEO_DRIVER_SDL2

#include "SDL_sdl2mouse.h"
#include "SDL_sdl2video.h"

#include "../../events/SDL_mouse_c.h"

#include <dlfcn.h>
#include <stdlib.h>

/* SDL2's SDL_SystemCursor (SDL2 ABI) — different ordering from SDL3's enum. */
typedef enum {
    SDL2_SYSTEM_CURSOR_ARROW,
    SDL2_SYSTEM_CURSOR_IBEAM,
    SDL2_SYSTEM_CURSOR_WAIT,
    SDL2_SYSTEM_CURSOR_CROSSHAIR,
    SDL2_SYSTEM_CURSOR_WAITARROW,
    SDL2_SYSTEM_CURSOR_SIZENWSE,
    SDL2_SYSTEM_CURSOR_SIZENESW,
    SDL2_SYSTEM_CURSOR_SIZEWE,
    SDL2_SYSTEM_CURSOR_SIZENS,
    SDL2_SYSTEM_CURSOR_SIZEALL,
    SDL2_SYSTEM_CURSOR_NO,
    SDL2_SYSTEM_CURSOR_HAND
} SDL2_SystemCursor;

/* SDL3-side cursor data: just wraps SDL2's opaque SDL_Cursor*. */
struct SDL_CursorData {
    void *sdl2_cursor;
    bool owned; /* true if we created it via SDL2_CreateColorCursor / SDL2_CreateSystemCursor */
};

/* SDL2 function pointers — resolved lazily on first call. The SDL2 lib
   handle is owned by SDL_sdl2video.c and shared via SDL3SHIM_SDL2_LIB. */
static int    (*p_SDL2_ShowCursor)(int) = NULL;
static void * (*p_SDL2_CreateColorCursor)(void *surface, int hot_x, int hot_y) = NULL;
static void * (*p_SDL2_CreateSystemCursor)(int id) = NULL;
static void   (*p_SDL2_FreeCursor)(void *cursor) = NULL;
static void   (*p_SDL2_SetCursor)(void *cursor) = NULL;
static void * (*p_SDL2_GetDefaultCursor)(void) = NULL;
static void * (*p_SDL2_CreateRGBSurfaceWithFormatFrom)(void *pixels, int w, int h, int depth, int pitch, Uint32 format) = NULL;
static void   (*p_SDL2_FreeSurface)(void *surface) = NULL;
static void   (*p_SDL2_WarpMouseInWindow)(void *window, int x, int y) = NULL;
static int    (*p_SDL2_WarpMouseGlobal)(int x, int y) = NULL;
static int    (*p_SDL2_SetRelativeMouseMode)(int enabled) = NULL;
static Uint32 (*p_SDL2_GetGlobalMouseState)(int *x, int *y) = NULL;

static bool resolved_syms = false;

static bool resolve_sdl2_mouse_syms(void)
{
    if (resolved_syms) {
        return true;
    }

    /* SDL_sdl2video.c already dlopen'd libSDL2 with RTLD_LOCAL, so the
       symbols aren't in the global namespace — but they are reachable
       from any handle returned by dlopen on the same library. We open
       the same library again (refcounted in glibc) to get a handle. */
    const char *lib = getenv("SDL3SHIM_SDL2_LIB");
    if (!lib || !*lib) {
        lib = "libSDL2-2.0.so.0";
    }

    void *h = dlopen(lib, RTLD_NOW | RTLD_LOCAL);
    if (!h) {
        SDL_LogWarn(SDL_LOG_CATEGORY_VIDEO,
                    "SDL2 mouse: failed to dlopen '%s': %s", lib, dlerror());
        return false;
    }

    #define LOAD(var, name) do { \
        var = dlsym(h, name); \
        if (!var) { \
            SDL_LogWarn(SDL_LOG_CATEGORY_VIDEO, "SDL2 mouse: missing %s", name); \
        } \
    } while (0)

    LOAD(p_SDL2_ShowCursor,                     "SDL_ShowCursor");
    LOAD(p_SDL2_CreateColorCursor,              "SDL_CreateColorCursor");
    LOAD(p_SDL2_CreateSystemCursor,             "SDL_CreateSystemCursor");
    LOAD(p_SDL2_FreeCursor,                     "SDL_FreeCursor");
    LOAD(p_SDL2_SetCursor,                      "SDL_SetCursor");
    LOAD(p_SDL2_GetDefaultCursor,               "SDL_GetDefaultCursor");
    LOAD(p_SDL2_CreateRGBSurfaceWithFormatFrom, "SDL_CreateRGBSurfaceWithFormatFrom");
    LOAD(p_SDL2_FreeSurface,                    "SDL_FreeSurface");
    LOAD(p_SDL2_WarpMouseInWindow,              "SDL_WarpMouseInWindow");
    LOAD(p_SDL2_WarpMouseGlobal,                "SDL_WarpMouseGlobal");
    LOAD(p_SDL2_SetRelativeMouseMode,           "SDL_SetRelativeMouseMode");
    LOAD(p_SDL2_GetGlobalMouseState,            "SDL_GetGlobalMouseState");

    #undef LOAD

    /* Don't dlclose: we want the resolved function pointers to stay valid.
       The reference is symmetrical with SDL_sdl2video.c's open. */
    resolved_syms = true;
    return true;
}

/* === SDL3 mouse driver vtable === */

static SDL_Cursor *SDL2Mouse_CreateCursor(SDL_Surface *surface, int hot_x, int hot_y)
{
    if (!surface || !p_SDL2_CreateRGBSurfaceWithFormatFrom || !p_SDL2_CreateColorCursor) {
        return NULL;
    }

    /* SDL3 has already converted the surface to ARGB8888 (see SDL_CreateCursor). */
    void *sdl2_surface = p_SDL2_CreateRGBSurfaceWithFormatFrom(
        surface->pixels, surface->w, surface->h, 32, surface->pitch,
        SDL_PIXELFORMAT_ARGB8888);
    if (!sdl2_surface) {
        return NULL;
    }

    void *sdl2_cursor = p_SDL2_CreateColorCursor(sdl2_surface, hot_x, hot_y);
    if (p_SDL2_FreeSurface) {
        p_SDL2_FreeSurface(sdl2_surface);
    }
    if (!sdl2_cursor) {
        return NULL;
    }

    SDL_Cursor *cursor = (SDL_Cursor *)SDL_calloc(1, sizeof(*cursor));
    if (!cursor) {
        if (p_SDL2_FreeCursor) p_SDL2_FreeCursor(sdl2_cursor);
        return NULL;
    }
    cursor->internal = (SDL_CursorData *)SDL_calloc(1, sizeof(SDL_CursorData));
    if (!cursor->internal) {
        if (p_SDL2_FreeCursor) p_SDL2_FreeCursor(sdl2_cursor);
        SDL_free(cursor);
        return NULL;
    }
    cursor->internal->sdl2_cursor = sdl2_cursor;
    cursor->internal->owned = true;
    return cursor;
}

static int sdl3_to_sdl2_system_cursor(SDL_SystemCursor id)
{
    /* Map the SDL3 names that exist in SDL2. SDL3 added more entries
       (NS/EW/NESW/NWSE/POINTER/PROGRESS/...); fall back to ARROW. */
    switch (id) {
        case SDL_SYSTEM_CURSOR_DEFAULT:    return SDL2_SYSTEM_CURSOR_ARROW;
        case SDL_SYSTEM_CURSOR_TEXT:       return SDL2_SYSTEM_CURSOR_IBEAM;
        case SDL_SYSTEM_CURSOR_WAIT:       return SDL2_SYSTEM_CURSOR_WAIT;
        case SDL_SYSTEM_CURSOR_CROSSHAIR:  return SDL2_SYSTEM_CURSOR_CROSSHAIR;
        case SDL_SYSTEM_CURSOR_PROGRESS:   return SDL2_SYSTEM_CURSOR_WAITARROW;
        case SDL_SYSTEM_CURSOR_NWSE_RESIZE:return SDL2_SYSTEM_CURSOR_SIZENWSE;
        case SDL_SYSTEM_CURSOR_NESW_RESIZE:return SDL2_SYSTEM_CURSOR_SIZENESW;
        case SDL_SYSTEM_CURSOR_EW_RESIZE:  return SDL2_SYSTEM_CURSOR_SIZEWE;
        case SDL_SYSTEM_CURSOR_NS_RESIZE:  return SDL2_SYSTEM_CURSOR_SIZENS;
        case SDL_SYSTEM_CURSOR_MOVE:       return SDL2_SYSTEM_CURSOR_SIZEALL;
        case SDL_SYSTEM_CURSOR_NOT_ALLOWED:return SDL2_SYSTEM_CURSOR_NO;
        case SDL_SYSTEM_CURSOR_POINTER:    return SDL2_SYSTEM_CURSOR_HAND;
        default:                           return SDL2_SYSTEM_CURSOR_ARROW;
    }
}

static SDL_Cursor *SDL2Mouse_CreateSystemCursor(SDL_SystemCursor id)
{
    if (!p_SDL2_CreateSystemCursor) {
        return NULL;
    }
    void *sdl2_cursor = p_SDL2_CreateSystemCursor(sdl3_to_sdl2_system_cursor(id));
    if (!sdl2_cursor) {
        return NULL;
    }
    SDL_Cursor *cursor = (SDL_Cursor *)SDL_calloc(1, sizeof(*cursor));
    if (!cursor) {
        if (p_SDL2_FreeCursor) p_SDL2_FreeCursor(sdl2_cursor);
        return NULL;
    }
    cursor->internal = (SDL_CursorData *)SDL_calloc(1, sizeof(SDL_CursorData));
    if (!cursor->internal) {
        if (p_SDL2_FreeCursor) p_SDL2_FreeCursor(sdl2_cursor);
        SDL_free(cursor);
        return NULL;
    }
    cursor->internal->sdl2_cursor = sdl2_cursor;
    cursor->internal->owned = true;
    return cursor;
}

static bool SDL2Mouse_ShowCursor(SDL_Cursor *cursor)
{
    /* SDL3 contract: cursor != NULL means "show this cursor"; cursor == NULL
       means "hide". RedrawCursor passes NULL whenever cursor_visible is
       false OR when focus exists but cur_cursor is NULL. To keep the
       cursor visible by default we install a wrapper around SDL2's default
       cursor at InitMouse, so cur_cursor becomes non-NULL. */
    if (cursor && cursor->internal && cursor->internal->sdl2_cursor && p_SDL2_SetCursor) {
        p_SDL2_SetCursor(cursor->internal->sdl2_cursor);
    }
    if (p_SDL2_ShowCursor) {
        p_SDL2_ShowCursor(cursor ? 1 : 0);
    }
    return true;
}

static void SDL2Mouse_FreeCursor(SDL_Cursor *cursor)
{
    if (!cursor) return;
    if (cursor->internal) {
        if (cursor->internal->sdl2_cursor && cursor->internal->owned && p_SDL2_FreeCursor) {
            p_SDL2_FreeCursor(cursor->internal->sdl2_cursor);
        }
        SDL_free(cursor->internal);
    }
    SDL_free(cursor);
}

static bool SDL2Mouse_WarpMouse(SDL_Window *window, float x, float y)
{
    if (!p_SDL2_WarpMouseInWindow || !window || !window->internal) {
        return false;
    }
    SDL_WindowData *wd = (SDL_WindowData *)window->internal;
    p_SDL2_WarpMouseInWindow(wd->sdl2_window, (int)x, (int)y);
    return true;
}

static bool SDL2Mouse_WarpMouseGlobal(float x, float y)
{
    if (!p_SDL2_WarpMouseGlobal) return false;
    return p_SDL2_WarpMouseGlobal((int)x, (int)y) == 0;
}

static bool SDL2Mouse_SetRelativeMouseMode(bool enabled)
{
    if (!p_SDL2_SetRelativeMouseMode) return false;
    return p_SDL2_SetRelativeMouseMode(enabled ? 1 : 0) == 0;
}

static SDL_MouseButtonFlags SDL2Mouse_GetGlobalMouseState(float *x, float *y)
{
    if (!p_SDL2_GetGlobalMouseState) return 0;
    int ix = 0, iy = 0;
    Uint32 state = p_SDL2_GetGlobalMouseState(&ix, &iy);
    if (x) *x = (float)ix;
    if (y) *y = (float)iy;
    return (SDL_MouseButtonFlags)state;
}

/* === entry points === */

bool SDL2_InitMouse(SDL_VideoDevice *_this)
{
    (void)_this;

    const char *enable = getenv("SDL3SHIM_ENABLE_MOUSE");
    if (!enable || !*enable || enable[0] == '0') {
        return true; /* opt-in; not enabled — leave SDL3 mouse vtable empty */
    }

    if (!resolve_sdl2_mouse_syms()) {
        return false;
    }

    SDL_Mouse *mouse = SDL_GetMouse();
    if (!mouse) return false;

    mouse->CreateCursor         = SDL2Mouse_CreateCursor;
    mouse->CreateSystemCursor   = SDL2Mouse_CreateSystemCursor;
    mouse->ShowCursor           = SDL2Mouse_ShowCursor;
    mouse->FreeCursor           = SDL2Mouse_FreeCursor;
    mouse->WarpMouse            = SDL2Mouse_WarpMouse;
    mouse->WarpMouseGlobal      = SDL2Mouse_WarpMouseGlobal;
    mouse->SetRelativeMouseMode = SDL2Mouse_SetRelativeMouseMode;
    mouse->GetGlobalMouseState  = SDL2Mouse_GetGlobalMouseState;

    /* Wrap SDL2's default cursor as the SDL3 def_cursor so that
       RedrawCursor uses a non-NULL cur_cursor when focus is gained
       and cursor_visible is true. SetDefaultCursor also sets cur_cursor
       if it was NULL. */
    if (p_SDL2_GetDefaultCursor) {
        void *sdl2_def = p_SDL2_GetDefaultCursor();
        if (sdl2_def) {
            SDL_Cursor *def = (SDL_Cursor *)SDL_calloc(1, sizeof(*def));
            if (def) {
                def->internal = (SDL_CursorData *)SDL_calloc(1, sizeof(SDL_CursorData));
                if (def->internal) {
                    def->internal->sdl2_cursor = sdl2_def;
                    def->internal->owned = false; /* SDL2 owns its default */
                    SDL_SetDefaultCursor(def);
                } else {
                    SDL_free(def);
                }
            }
        }
    }

    SDL_LogInfo(SDL_LOG_CATEGORY_VIDEO,
                "SDL2 mouse: SDL3SHIM_ENABLE_MOUSE=1, cursor calls forwarded to native SDL2");
    return true;
}

void SDL2_QuitMouse(SDL_VideoDevice *_this)
{
    (void)_this;
    /* SDL3 frees def_cursor / cursors via mouse->FreeCursor on shutdown. */
}

#endif /* SDL_VIDEO_DRIVER_SDL2 */
