/*
  SDL2 backend - audio driver declarations
*/
#include "SDL_internal.h"

#ifndef SDL_sdl2audio_h
#define SDL_sdl2audio_h

#ifdef SDL_AUDIO_DRIVER_SDL2

struct SDL_PrivateAudioData {
    Uint32 sdl2_dev;       /* SDL2 audio device ID */
    Uint8 *mixbuf;         /* Buffer for GetDeviceBuf */
    int mixbuf_size;       /* Size of mix buffer */
    Uint64 io_delay_ns;    /* Nanoseconds between I/O operations (fallback pacing) */
    Uint32 queue_high_water; /* Bytes: wake WaitDevice once queue drops below this */
};

#endif
#endif
