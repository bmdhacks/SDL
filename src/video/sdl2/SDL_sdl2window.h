/*
  SDL2 backend - window management declarations
*/
#include "SDL_internal.h"

#ifndef SDL_sdl2window_h
#define SDL_sdl2window_h

#ifdef SDL_VIDEO_DRIVER_SDL2

extern bool SDL2_CreateSDLWindow(SDL_VideoDevice *_this, SDL_Window *window, SDL_PropertiesID create_props);
extern void SDL2_DestroySDLWindow(SDL_VideoDevice *_this, SDL_Window *window);
extern void SDL2_SetSDLWindowTitle(SDL_VideoDevice *_this, SDL_Window *window);
extern bool SDL2_SetSDLWindowPosition(SDL_VideoDevice *_this, SDL_Window *window);
extern void SDL2_SetSDLWindowSize(SDL_VideoDevice *_this, SDL_Window *window);
extern void SDL2_ShowSDLWindow(SDL_VideoDevice *_this, SDL_Window *window);
extern void SDL2_HideSDLWindow(SDL_VideoDevice *_this, SDL_Window *window);
extern void SDL2_RaiseSDLWindow(SDL_VideoDevice *_this, SDL_Window *window);
extern void SDL2_MaximizeSDLWindow(SDL_VideoDevice *_this, SDL_Window *window);
extern void SDL2_MinimizeSDLWindow(SDL_VideoDevice *_this, SDL_Window *window);
extern void SDL2_RestoreSDLWindow(SDL_VideoDevice *_this, SDL_Window *window);
extern SDL_FullscreenResult SDL2_SetSDLWindowFullscreen(SDL_VideoDevice *_this, SDL_Window *window, SDL_VideoDisplay *display, SDL_FullscreenOp fullscreen);
extern void SDL2_GetSDLWindowSizeInPixels(SDL_VideoDevice *_this, SDL_Window *window, int *w, int *h);

#endif
#endif
