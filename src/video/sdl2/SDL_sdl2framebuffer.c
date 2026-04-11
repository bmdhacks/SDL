/*
  SDL2 backend - software framebuffer stub
  For now, we rely on GL rendering; software framebuffer is a stub.
*/
#include "SDL_internal.h"

#ifdef SDL_VIDEO_DRIVER_SDL2

#include "../SDL_sysvideo.h"
#include "SDL_sdl2framebuffer.h"

bool SDL2_CreateWindowFramebuffer(SDL_VideoDevice *_this, SDL_Window *window,
                                   SDL_PixelFormat *format, void **pixels, int *pitch)
{
    return SDL_Unsupported();
}

bool SDL2_UpdateWindowFramebuffer(SDL_VideoDevice *_this, SDL_Window *window,
                                   const SDL_Rect *rects, int numrects)
{
    return SDL_Unsupported();
}

void SDL2_DestroyWindowFramebuffer(SDL_VideoDevice *_this, SDL_Window *window)
{
}

#endif /* SDL_VIDEO_DRIVER_SDL2 */
