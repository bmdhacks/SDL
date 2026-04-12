/*
  SDL3 video driver backed by SDL2 via dlopen.
  For aarch64 Linux handhelds with custom-patched SDL2.
*/
#include "SDL_internal.h"

#ifdef SDL_VIDEO_DRIVER_SDL2

#include "SDL_sdl2video.h"
#include "SDL_sdl2window.h"
#include "SDL_sdl2events.h"
#include "SDL_sdl2opengles.h"
#include "SDL_sdl2framebuffer.h"

#include <dlfcn.h>
#include <stdlib.h>  /* for getenv() */

#define SDL2VID_DRIVER_NAME "sdl2"

/* === SDL2 dynamic library handle === */
static void *sdl2_handle = NULL;

/* === SDL2 function pointers === */
int (*SDL2_Init)(Uint32 flags) = NULL;
void (*SDL2_Quit)(void) = NULL;
void (*SDL2_QuitSubSystem)(Uint32 flags) = NULL;
const char *(*SDL2_GetError)(void) = NULL;
int (*SDL2_GetNumVideoDisplays)(void) = NULL;
int (*SDL2_GetNumDisplayModes)(int displayIndex) = NULL;
int (*SDL2_GetDisplayMode)(int displayIndex, int modeIndex, SDL2_DisplayMode *mode) = NULL;
int (*SDL2_GetDesktopDisplayMode)(int displayIndex, SDL2_DisplayMode *mode) = NULL;
int (*SDL2_GetDisplayBounds)(int displayIndex, SDL_Rect *rect) = NULL;
void *(*SDL2_CreateWindow)(const char *title, int x, int y, int w, int h, Uint32 flags) = NULL;
void (*SDL2_DestroyWindow)(void *window) = NULL;
Uint32 (*SDL2_GetWindowID)(void *window) = NULL;
void (*SDL2_SetWindowTitle)(void *window, const char *title) = NULL;
void (*SDL2_SetWindowPosition)(void *window, int x, int y) = NULL;
void (*SDL2_SetWindowSize)(void *window, int w, int h) = NULL;
void (*SDL2_GetWindowSize)(void *window, int *w, int *h) = NULL;
void (*SDL2_ShowWindow)(void *window) = NULL;
void (*SDL2_HideWindow)(void *window) = NULL;
void (*SDL2_RaiseWindow)(void *window) = NULL;
void (*SDL2_MaximizeWindow)(void *window) = NULL;
void (*SDL2_MinimizeWindow)(void *window) = NULL;
void (*SDL2_RestoreWindow)(void *window) = NULL;
int (*SDL2_SetWindowFullscreen)(void *window, Uint32 flags) = NULL;
void (*SDL2_SetWindowGrab)(void *window, int grabbed) = NULL;
void (*SDL2_SetWindowBordered)(void *window, int bordered) = NULL;
void (*SDL2_SetWindowResizable)(void *window, int resizable) = NULL;
Uint32 (*SDL2_GetWindowFlags)(void *window) = NULL;

int (*SDL2_GL_SetAttribute)(int attr, int value) = NULL;
int (*SDL2_GL_GetAttribute)(int attr, int *value) = NULL;
void *(*SDL2_GL_CreateContext)(void *window) = NULL;
void (*SDL2_GL_DeleteContext)(void *context) = NULL;
int (*SDL2_GL_MakeCurrent)(void *window, void *context) = NULL;
void (*SDL2_GL_SwapWindow)(void *window) = NULL;
int (*SDL2_GL_SetSwapInterval)(int interval) = NULL;
int (*SDL2_GL_GetSwapInterval)(void) = NULL;
void *(*SDL2_GL_GetProcAddress)(const char *proc) = NULL;
void (*SDL2_GL_GetDrawableSize)(void *window, int *w, int *h) = NULL;

int (*SDL2_PollEvent)(SDL2_Event *event) = NULL;
void (*SDL2_PumpEvents)(void) = NULL;

/* Audio function pointers (shared with audio driver) */
int (*SDL2_OpenAudioDevice_ptr)(const char *device, int iscapture,
                                const void *desired, void *obtained,
                                int allowed_changes) = NULL;
void (*SDL2_CloseAudioDevice_ptr)(Uint32 dev) = NULL;
void (*SDL2_PauseAudioDevice_ptr)(Uint32 dev, int pause_on) = NULL;
int (*SDL2_QueueAudio_ptr)(Uint32 dev, const void *data, Uint32 len) = NULL;
Uint32 (*SDL2_DequeueAudio_ptr)(Uint32 dev, void *data, Uint32 len) = NULL;
Uint32 (*SDL2_GetQueuedAudioSize_ptr)(Uint32 dev) = NULL;
void (*SDL2_ClearQueuedAudio_ptr)(Uint32 dev) = NULL;

/* === SDL2 library loading === */

#define LOAD_SDL2_SYM(var, name) do { \
    var = dlsym(sdl2_handle, #name); \
    if (!var) { \
        SDL_LogWarn(SDL_LOG_CATEGORY_VIDEO, "SDL2 backend: missing symbol %s", #name); \
    } \
} while (0)

bool SDL2_LoadLibrary(void)
{
    if (sdl2_handle) {
        return true;
    }

    const char *lib = getenv("SDL3SHIM_SDL2_LIB");
    if (!lib || !*lib) {
        lib = "libSDL2-2.0.so.0";
    }

    sdl2_handle = dlopen(lib, RTLD_NOW | RTLD_LOCAL);
    if (!sdl2_handle) {
        return SDL_SetError("SDL2 backend: Failed to load '%s': %s", lib, dlerror());
    }

    /* Core */
    LOAD_SDL2_SYM(SDL2_Init, SDL_Init);
    LOAD_SDL2_SYM(SDL2_Quit, SDL_Quit);
    LOAD_SDL2_SYM(SDL2_QuitSubSystem, SDL_QuitSubSystem);
    LOAD_SDL2_SYM(SDL2_GetError, SDL_GetError);

    /* Video / Display */
    LOAD_SDL2_SYM(SDL2_GetNumVideoDisplays, SDL_GetNumVideoDisplays);
    LOAD_SDL2_SYM(SDL2_GetNumDisplayModes, SDL_GetNumDisplayModes);
    LOAD_SDL2_SYM(SDL2_GetDisplayMode, SDL_GetDisplayMode);
    LOAD_SDL2_SYM(SDL2_GetDesktopDisplayMode, SDL_GetDesktopDisplayMode);
    LOAD_SDL2_SYM(SDL2_GetDisplayBounds, SDL_GetDisplayBounds);

    /* Window */
    LOAD_SDL2_SYM(SDL2_CreateWindow, SDL_CreateWindow);
    LOAD_SDL2_SYM(SDL2_DestroyWindow, SDL_DestroyWindow);
    LOAD_SDL2_SYM(SDL2_GetWindowID, SDL_GetWindowID);
    LOAD_SDL2_SYM(SDL2_SetWindowTitle, SDL_SetWindowTitle);
    LOAD_SDL2_SYM(SDL2_SetWindowPosition, SDL_SetWindowPosition);
    LOAD_SDL2_SYM(SDL2_SetWindowSize, SDL_SetWindowSize);
    LOAD_SDL2_SYM(SDL2_GetWindowSize, SDL_GetWindowSize);
    LOAD_SDL2_SYM(SDL2_ShowWindow, SDL_ShowWindow);
    LOAD_SDL2_SYM(SDL2_HideWindow, SDL_HideWindow);
    LOAD_SDL2_SYM(SDL2_RaiseWindow, SDL_RaiseWindow);
    LOAD_SDL2_SYM(SDL2_MaximizeWindow, SDL_MaximizeWindow);
    LOAD_SDL2_SYM(SDL2_MinimizeWindow, SDL_MinimizeWindow);
    LOAD_SDL2_SYM(SDL2_RestoreWindow, SDL_RestoreWindow);
    LOAD_SDL2_SYM(SDL2_SetWindowFullscreen, SDL_SetWindowFullscreen);
    LOAD_SDL2_SYM(SDL2_SetWindowGrab, SDL_SetWindowGrab);
    LOAD_SDL2_SYM(SDL2_SetWindowBordered, SDL_SetWindowBordered);
    LOAD_SDL2_SYM(SDL2_SetWindowResizable, SDL_SetWindowResizable);
    LOAD_SDL2_SYM(SDL2_GetWindowFlags, SDL_GetWindowFlags);

    /* GL */
    LOAD_SDL2_SYM(SDL2_GL_SetAttribute, SDL_GL_SetAttribute);
    LOAD_SDL2_SYM(SDL2_GL_GetAttribute, SDL_GL_GetAttribute);
    LOAD_SDL2_SYM(SDL2_GL_CreateContext, SDL_GL_CreateContext);
    LOAD_SDL2_SYM(SDL2_GL_DeleteContext, SDL_GL_DeleteContext);
    LOAD_SDL2_SYM(SDL2_GL_MakeCurrent, SDL_GL_MakeCurrent);
    LOAD_SDL2_SYM(SDL2_GL_SwapWindow, SDL_GL_SwapWindow);
    LOAD_SDL2_SYM(SDL2_GL_SetSwapInterval, SDL_GL_SetSwapInterval);
    LOAD_SDL2_SYM(SDL2_GL_GetSwapInterval, SDL_GL_GetSwapInterval);
    LOAD_SDL2_SYM(SDL2_GL_GetProcAddress, SDL_GL_GetProcAddress);
    LOAD_SDL2_SYM(SDL2_GL_GetDrawableSize, SDL_GL_GetDrawableSize);

    /* Events */
    LOAD_SDL2_SYM(SDL2_PollEvent, SDL_PollEvent);
    LOAD_SDL2_SYM(SDL2_PumpEvents, SDL_PumpEvents);

    /* Audio (loaded here, used by audio driver) */
    LOAD_SDL2_SYM(SDL2_OpenAudioDevice_ptr, SDL_OpenAudioDevice);
    LOAD_SDL2_SYM(SDL2_CloseAudioDevice_ptr, SDL_CloseAudioDevice);
    LOAD_SDL2_SYM(SDL2_PauseAudioDevice_ptr, SDL_PauseAudioDevice);
    LOAD_SDL2_SYM(SDL2_QueueAudio_ptr, SDL_QueueAudio);
    LOAD_SDL2_SYM(SDL2_DequeueAudio_ptr, SDL_DequeueAudio);
    LOAD_SDL2_SYM(SDL2_GetQueuedAudioSize_ptr, SDL_GetQueuedAudioSize);
    LOAD_SDL2_SYM(SDL2_ClearQueuedAudio_ptr, SDL_ClearQueuedAudio);

    return true;
}

void SDL2_UnloadLibrary(void)
{
    if (sdl2_handle) {
        dlclose(sdl2_handle);
        sdl2_handle = NULL;
    }
}

/* === Video driver lifecycle === */

static bool SDL2_VideoInit(SDL_VideoDevice *_this);
static bool SDL2_SetDisplayMode(SDL_VideoDevice *_this, SDL_VideoDisplay *display, SDL_DisplayMode *mode);
static void SDL2_VideoQuit(SDL_VideoDevice *_this);

static void SDL2_DeleteDevice(SDL_VideoDevice *device)
{
    SDL2_UnloadLibrary();
    SDL_free(device);
}

static SDL_VideoDevice *SDL2_CreateDevice(void)
{
    SDL_VideoDevice *device;

    if (!SDL2_LoadLibrary()) {
        return NULL;
    }

    device = (SDL_VideoDevice *)SDL_calloc(1, sizeof(SDL_VideoDevice));
    if (!device) {
        return NULL;
    }

    /* Core lifecycle */
    device->VideoInit = SDL2_VideoInit;
    device->VideoQuit = SDL2_VideoQuit;
    device->SetDisplayMode = SDL2_SetDisplayMode;
    device->PumpEvents = SDL2_PumpEvents_Impl;
    device->free = SDL2_DeleteDevice;

    /* Window management */
    device->CreateSDLWindow = SDL2_CreateSDLWindow;
    device->DestroyWindow = SDL2_DestroySDLWindow;
    device->SetWindowTitle = SDL2_SetSDLWindowTitle;
    device->SetWindowPosition = SDL2_SetSDLWindowPosition;
    device->SetWindowSize = SDL2_SetSDLWindowSize;
    device->ShowWindow = SDL2_ShowSDLWindow;
    device->HideWindow = SDL2_HideSDLWindow;
    device->RaiseWindow = SDL2_RaiseSDLWindow;
    device->MaximizeWindow = SDL2_MaximizeSDLWindow;
    device->MinimizeWindow = SDL2_MinimizeSDLWindow;
    device->RestoreWindow = SDL2_RestoreSDLWindow;
    device->SetWindowFullscreen = SDL2_SetSDLWindowFullscreen;
    device->GetWindowSizeInPixels = SDL2_GetSDLWindowSizeInPixels;

    /* GL context - delegated to SDL2 */
    device->GL_LoadLibrary = SDL2_GLES_LoadLibrary;
    device->GL_GetProcAddress = SDL2_GLES_GetProcAddress;
    device->GL_UnloadLibrary = SDL2_GLES_UnloadLibrary;
    device->GL_CreateContext = SDL2_GLES_CreateContext;
    device->GL_MakeCurrent = SDL2_GLES_MakeCurrent;
    device->GL_SetSwapInterval = SDL2_GLES_SetSwapInterval;
    device->GL_GetSwapInterval = SDL2_GLES_GetSwapInterval;
    device->GL_SwapWindow = SDL2_GLES_SwapWindow;
    device->GL_DestroyContext = SDL2_GLES_DestroyContext;

    /* Framebuffer */
    device->CreateWindowFramebuffer = SDL2_CreateWindowFramebuffer;
    device->UpdateWindowFramebuffer = SDL2_UpdateWindowFramebuffer;
    device->DestroyWindowFramebuffer = SDL2_DestroyWindowFramebuffer;

    return device;
}

VideoBootStrap SDL2_bootstrap = {
    SDL2VID_DRIVER_NAME, "SDL2-delegating video driver",
    SDL2_CreateDevice,
    NULL,   /* no ShowMessageBox */
    false   /* not preferred - allows explicit SDL_VIDEO_DRIVER=sdl2 selection */
};

static bool SDL2_VideoInit(SDL_VideoDevice *_this)
{
    /* Initialize SDL2 video subsystem */
    if (!SDL2_Init) {
        return SDL_SetError("SDL2 not loaded");
    }

    /* Pass through video driver hint to SDL2 */
    const char *sdl2_videodriver = getenv("SDL3SHIM_SDL2_VIDEODRIVER");
    if (sdl2_videodriver && *sdl2_videodriver) {
        setenv("SDL_VIDEODRIVER", sdl2_videodriver, 1);
        SDL_Log("SDL2 backend: setting SDL_VIDEODRIVER=%s", sdl2_videodriver);
    }

    if (SDL2_Init(SDL2_INIT_VIDEO) < 0) {
        return SDL_SetError("SDL2_Init(VIDEO) failed: %s",
                            SDL2_GetError ? SDL2_GetError() : "unknown");
    }

    /* Query displays from SDL2 and register them with SDL3 */
    int num_displays = SDL2_GetNumVideoDisplays ? SDL2_GetNumVideoDisplays() : 1;
    if (num_displays < 1) {
        num_displays = 1;
    }

    for (int i = 0; i < num_displays; i++) {
        SDL_DisplayMode mode;
        SDL_zero(mode);

        if (SDL2_GetDesktopDisplayMode) {
            SDL2_DisplayMode sdl2_mode;
            if (SDL2_GetDesktopDisplayMode(i, &sdl2_mode) == 0) {
                mode.w = sdl2_mode.w;
                mode.h = sdl2_mode.h;
                mode.refresh_rate = (float)sdl2_mode.refresh_rate;
                mode.format = sdl2_mode.format;
            }
        }

        /* Fallback defaults */
        if (mode.w == 0 || mode.h == 0) {
            mode.w = 640;
            mode.h = 480;
            mode.format = SDL_PIXELFORMAT_XRGB8888;
        }
        if (mode.refresh_rate == 0.0f) {
            mode.refresh_rate = 60.0f;
        }

        SDL_DisplayID display_id = SDL_AddBasicVideoDisplay(&mode);
        if (display_id == 0) {
            continue;
        }

        /* Add all available modes for this display */
        if (SDL2_GetNumDisplayModes && SDL2_GetDisplayMode) {
            int num_modes = SDL2_GetNumDisplayModes(i);
            for (int j = 0; j < num_modes; j++) {
                SDL2_DisplayMode sdl2_m;
                if (SDL2_GetDisplayMode(i, j, &sdl2_m) == 0) {
                    SDL_DisplayMode m;
                    SDL_zero(m);
                    m.w = sdl2_m.w;
                    m.h = sdl2_m.h;
                    m.refresh_rate = (float)sdl2_m.refresh_rate;
                    m.format = sdl2_m.format;

                    SDL_VideoDisplay *vdisplay = SDL_GetVideoDisplay(display_id);
                    if (vdisplay) {
                        SDL_AddFullscreenDisplayMode(vdisplay, &m);
                    }
                }
            }
        }
    }

    return true;
}

static bool SDL2_SetDisplayMode(SDL_VideoDevice *_this, SDL_VideoDisplay *display, SDL_DisplayMode *mode)
{
    /* On the r36s, we don't change display modes */
    return true;
}

static void SDL2_VideoQuit(SDL_VideoDevice *_this)
{
    if (SDL2_QuitSubSystem) {
        SDL2_QuitSubSystem(SDL2_INIT_VIDEO);
    }
}

#endif /* SDL_VIDEO_DRIVER_SDL2 */
