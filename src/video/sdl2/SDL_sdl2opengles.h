/*
  SDL2 backend - OpenGL ES context declarations
*/
#include "SDL_internal.h"

#ifndef SDL_sdl2opengles_h
#define SDL_sdl2opengles_h

#ifdef SDL_VIDEO_DRIVER_SDL2

extern bool SDL2_GLES_LoadLibrary(SDL_VideoDevice *_this, const char *path);
extern SDL_FunctionPointer SDL2_GLES_GetProcAddress(SDL_VideoDevice *_this, const char *proc);
extern void SDL2_GLES_UnloadLibrary(SDL_VideoDevice *_this);
extern SDL_GLContext SDL2_GLES_CreateContext(SDL_VideoDevice *_this, SDL_Window *window);
extern bool SDL2_GLES_MakeCurrent(SDL_VideoDevice *_this, SDL_Window *window, SDL_GLContext context);
extern bool SDL2_GLES_SetSwapInterval(SDL_VideoDevice *_this, int interval);
extern bool SDL2_GLES_GetSwapInterval(SDL_VideoDevice *_this, int *interval);
extern bool SDL2_GLES_SwapWindow(SDL_VideoDevice *_this, SDL_Window *window);
extern bool SDL2_GLES_DestroyContext(SDL_VideoDevice *_this, SDL_GLContext context);

#endif
#endif
