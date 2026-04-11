/*
  SDL2 backend - framebuffer declarations
*/
#include "SDL_internal.h"

#ifndef SDL_sdl2framebuffer_h
#define SDL_sdl2framebuffer_h

#ifdef SDL_VIDEO_DRIVER_SDL2

extern bool SDL2_CreateWindowFramebuffer(SDL_VideoDevice *_this, SDL_Window *window, SDL_PixelFormat *format, void **pixels, int *pitch);
extern bool SDL2_UpdateWindowFramebuffer(SDL_VideoDevice *_this, SDL_Window *window, const SDL_Rect *rects, int numrects);
extern void SDL2_DestroyWindowFramebuffer(SDL_VideoDevice *_this, SDL_Window *window);

#endif
#endif
