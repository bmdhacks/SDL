/*
  SDL3 mouse/cursor driver backed by SDL2 via dlopen.
  Pairs with the SDL2-delegating video driver in this directory.

  The SDL3 mouse subsystem dispatches all cursor work through a vtable on
  SDL_Mouse (CreateCursor / ShowCursor / WarpMouse / ...). When the SDL2
  video backend is selected, no driver registers those callbacks, so
  SDL_ShowCursor() silently no-ops in SDL_RedrawCursor().

  This translation layer registers a thin shim on SDL_GetMouse() that
  forwards each call to the matching SDL2 entry point (resolved with
  dlsym from the same handle SDL_sdl2video.c opens).

  Gated by SDL3SHIM_ENABLE_MOUSE=1 — opt-in until validated on target
  handhelds. When disabled the SDL2 video backend behaves as before
  (no mouse callbacks → SDL_ShowCursor is a no-op).
*/
#include "SDL_internal.h"

#ifndef SDL_sdl2mouse_h
#define SDL_sdl2mouse_h

#ifdef SDL_VIDEO_DRIVER_SDL2

#include "../SDL_sysvideo.h"

extern bool SDL2_InitMouse(SDL_VideoDevice *_this);
extern void SDL2_QuitMouse(SDL_VideoDevice *_this);

#endif /* SDL_VIDEO_DRIVER_SDL2 */

#endif /* SDL_sdl2mouse_h */
