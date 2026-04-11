/*
  SDL3 video driver backed by SDL2 via dlopen.
  For aarch64 Linux handhelds with custom-patched SDL2.
*/
#include "SDL_internal.h"

#ifndef SDL_sdl2video_h
#define SDL_sdl2video_h

#ifdef SDL_VIDEO_DRIVER_SDL2

#include "../SDL_sysvideo.h"

/* SDL2 function pointer types - all SDL2 handles are opaque void* */
typedef void *SDL2_Window;
typedef void *SDL2_GLContext;

/* SDL2 type definitions we need for calling SDL2 functions */
typedef int SDL2_bool;

typedef struct SDL2_DisplayMode {
    Uint32 format;
    int w, h;
    int refresh_rate;
    void *driverdata;
} SDL2_DisplayMode;

/* SDL2 event types we need to translate */
/* SDL2 event union - we define just the parts we need */
#define SDL2_WINDOWEVENT        0x200
#define SDL2_KEYDOWN            0x300
#define SDL2_KEYUP              0x301
#define SDL2_TEXTEDITING        0x302
#define SDL2_TEXTINPUT          0x303
#define SDL2_MOUSEMOTION        0x400
#define SDL2_MOUSEBUTTONDOWN    0x401
#define SDL2_MOUSEBUTTONUP      0x402
#define SDL2_MOUSEWHEEL         0x403
#define SDL2_JOYAXISMOTION      0x600
#define SDL2_JOYBUTTONDOWN      0x603
#define SDL2_JOYBUTTONUP        0x604
#define SDL2_CONTROLLERAXISMOTION 0x650
#define SDL2_CONTROLLERBUTTONDOWN 0x651
#define SDL2_CONTROLLERBUTTONUP   0x652
#define SDL2_QUIT               0x100
#define SDL2_DROPFILE            0x1000
#define SDL2_DROPTEXT            0x1001
#define SDL2_DROPBEGIN           0x1002
#define SDL2_DROPCOMPLETE        0x1003

/* SDL2 window event sub-types */
#define SDL2_WINDOWEVENT_SHOWN          1
#define SDL2_WINDOWEVENT_HIDDEN         2
#define SDL2_WINDOWEVENT_EXPOSED        3
#define SDL2_WINDOWEVENT_MOVED          4
#define SDL2_WINDOWEVENT_RESIZED        5
#define SDL2_WINDOWEVENT_SIZE_CHANGED   6
#define SDL2_WINDOWEVENT_MINIMIZED      7
#define SDL2_WINDOWEVENT_MAXIMIZED      8
#define SDL2_WINDOWEVENT_RESTORED       9
#define SDL2_WINDOWEVENT_ENTER          10
#define SDL2_WINDOWEVENT_LEAVE          11
#define SDL2_WINDOWEVENT_FOCUS_GAINED   12
#define SDL2_WINDOWEVENT_FOCUS_LOST     13
#define SDL2_WINDOWEVENT_CLOSE          14

/* SDL2 window flags */
#define SDL2_WINDOW_FULLSCREEN          0x00000001
#define SDL2_WINDOW_OPENGL              0x00000002
#define SDL2_WINDOW_SHOWN               0x00000004
#define SDL2_WINDOW_HIDDEN              0x00000008
#define SDL2_WINDOW_BORDERLESS          0x00000010
#define SDL2_WINDOW_RESIZABLE           0x00000020
#define SDL2_WINDOW_MINIMIZED           0x00000040
#define SDL2_WINDOW_MAXIMIZED           0x00000080
#define SDL2_WINDOW_FULLSCREEN_DESKTOP  0x00001001

/* SDL2 init flags */
#define SDL2_INIT_VIDEO     0x00000020
#define SDL2_INIT_AUDIO     0x00000010

/* SDL2 keysym */
typedef struct SDL2_Keysym {
    int scancode;
    int sym;
    Uint16 mod;
    Uint32 unused;
} SDL2_Keysym;

/* SDL2 event structures */
typedef struct SDL2_WindowEvent {
    Uint32 type;
    Uint32 timestamp;
    Uint32 windowID;
    Uint8 event;
    Uint8 padding1, padding2, padding3;
    Sint32 data1;
    Sint32 data2;
} SDL2_WindowEvent;

typedef struct SDL2_KeyboardEvent {
    Uint32 type;
    Uint32 timestamp;
    Uint32 windowID;
    Uint8 state;
    Uint8 repeat;
    Uint8 padding2, padding3;
    SDL2_Keysym keysym;
} SDL2_KeyboardEvent;

typedef struct SDL2_MouseMotionEvent {
    Uint32 type;
    Uint32 timestamp;
    Uint32 windowID;
    Uint32 which;
    Uint32 state;
    Sint32 x, y;
    Sint32 xrel, yrel;
} SDL2_MouseMotionEvent;

typedef struct SDL2_MouseButtonEvent {
    Uint32 type;
    Uint32 timestamp;
    Uint32 windowID;
    Uint32 which;
    Uint8 button;
    Uint8 state;
    Uint8 clicks;
    Uint8 padding1;
    Sint32 x, y;
} SDL2_MouseButtonEvent;

typedef struct SDL2_MouseWheelEvent {
    Uint32 type;
    Uint32 timestamp;
    Uint32 windowID;
    Uint32 which;
    Sint32 x, y;
    Uint32 direction;
} SDL2_MouseWheelEvent;

typedef struct SDL2_TextInputEvent {
    Uint32 type;
    Uint32 timestamp;
    Uint32 windowID;
    char text[32];
} SDL2_TextInputEvent;

typedef struct SDL2_QuitEvent {
    Uint32 type;
    Uint32 timestamp;
} SDL2_QuitEvent;

typedef struct SDL2_DropEvent {
    Uint32 type;
    Uint32 timestamp;
    char *file;
    Uint32 windowID;
} SDL2_DropEvent;

typedef union SDL2_Event {
    Uint32 type;
    SDL2_WindowEvent window;
    SDL2_KeyboardEvent key;
    SDL2_MouseMotionEvent motion;
    SDL2_MouseButtonEvent button;
    SDL2_MouseWheelEvent wheel;
    SDL2_TextInputEvent text;
    SDL2_QuitEvent quit;
    SDL2_DropEvent drop;
    Uint8 padding[56];  /* SDL2 event union is 56 bytes */
} SDL2_Event;

/* Per-window data stored in window->internal */
typedef struct SDL_WindowData {
    void *sdl2_window;       /* SDL2 SDL_Window*, opaque */
    void *sdl2_gl_context;   /* SDL2 SDL_GLContext, opaque */
    Uint32 sdl2_window_id;   /* SDL2 window ID for event correlation */
} SDL_WindowData;

/* === SDL2 function pointers === */
/* Video / Window */
extern int (*SDL2_Init)(Uint32 flags);
extern void (*SDL2_Quit)(void);
extern void (*SDL2_QuitSubSystem)(Uint32 flags);
extern const char *(*SDL2_GetError)(void);
extern int (*SDL2_GetNumVideoDisplays)(void);
extern int (*SDL2_GetNumDisplayModes)(int displayIndex);
extern int (*SDL2_GetDisplayMode)(int displayIndex, int modeIndex, SDL2_DisplayMode *mode);
extern int (*SDL2_GetDesktopDisplayMode)(int displayIndex, SDL2_DisplayMode *mode);
extern int (*SDL2_GetDisplayBounds)(int displayIndex, SDL_Rect *rect);
extern void *(*SDL2_CreateWindow)(const char *title, int x, int y, int w, int h, Uint32 flags);
extern void (*SDL2_DestroyWindow)(void *window);
extern Uint32 (*SDL2_GetWindowID)(void *window);
extern void (*SDL2_SetWindowTitle)(void *window, const char *title);
extern void (*SDL2_SetWindowPosition)(void *window, int x, int y);
extern void (*SDL2_SetWindowSize)(void *window, int w, int h);
extern void (*SDL2_GetWindowSize)(void *window, int *w, int *h);
extern void (*SDL2_ShowWindow)(void *window);
extern void (*SDL2_HideWindow)(void *window);
extern void (*SDL2_RaiseWindow)(void *window);
extern void (*SDL2_MaximizeWindow)(void *window);
extern void (*SDL2_MinimizeWindow)(void *window);
extern void (*SDL2_RestoreWindow)(void *window);
extern int (*SDL2_SetWindowFullscreen)(void *window, Uint32 flags);
extern void (*SDL2_SetWindowGrab)(void *window, int grabbed);
extern void (*SDL2_SetWindowBordered)(void *window, int bordered);
extern void (*SDL2_SetWindowResizable)(void *window, int resizable);
extern Uint32 (*SDL2_GetWindowFlags)(void *window);

/* GL context */
extern int (*SDL2_GL_SetAttribute)(int attr, int value);
extern int (*SDL2_GL_GetAttribute)(int attr, int *value);
extern void *(*SDL2_GL_CreateContext)(void *window);
extern void (*SDL2_GL_DeleteContext)(void *context);
extern int (*SDL2_GL_MakeCurrent)(void *window, void *context);
extern void (*SDL2_GL_SwapWindow)(void *window);
extern int (*SDL2_GL_SetSwapInterval)(int interval);
extern int (*SDL2_GL_GetSwapInterval)(void);
extern void *(*SDL2_GL_GetProcAddress)(const char *proc);
extern void (*SDL2_GL_GetDrawableSize)(void *window, int *w, int *h);

/* Events */
extern int (*SDL2_PollEvent)(SDL2_Event *event);
extern void (*SDL2_PumpEvents)(void);

/* Lifecycle */
extern bool SDL2_LoadLibrary(void);
extern void SDL2_UnloadLibrary(void);

#endif /* SDL_VIDEO_DRIVER_SDL2 */

#endif /* SDL_sdl2video_h */
