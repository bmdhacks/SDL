/*
  SDL2 backend - event pump declarations
*/
#include "SDL_internal.h"

#ifndef SDL_sdl2events_h
#define SDL_sdl2events_h

#ifdef SDL_VIDEO_DRIVER_SDL2

extern void SDL2_PumpEvents_Impl(SDL_VideoDevice *_this);

#endif
#endif
