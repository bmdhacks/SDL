/*
  SDL3 audio driver backed by SDL2 via dlopen.
  Uses SDL2's queue-based audio API for simplicity.
*/
#include "SDL_internal.h"

#ifdef SDL_AUDIO_DRIVER_SDL2

#include "../SDL_sysaudio.h"
#include "SDL_sdl2audio.h"

/* Import SDL2 function pointers from the video driver loader */
extern int (*SDL2_Init)(Uint32 flags);
extern void (*SDL2_QuitSubSystem)(Uint32 flags);
extern int (*SDL2_OpenAudioDevice_ptr)(const char *device, int iscapture,
                                        const void *desired, void *obtained,
                                        int allowed_changes);
extern void (*SDL2_CloseAudioDevice_ptr)(Uint32 dev);
extern void (*SDL2_PauseAudioDevice_ptr)(Uint32 dev, int pause_on);
extern int (*SDL2_QueueAudio_ptr)(Uint32 dev, const void *data, Uint32 len);
extern Uint32 (*SDL2_DequeueAudio_ptr)(Uint32 dev, void *data, Uint32 len);
extern Uint32 (*SDL2_GetQueuedAudioSize_ptr)(Uint32 dev);
extern void (*SDL2_ClearQueuedAudio_ptr)(Uint32 dev);

#define SDL2_INIT_AUDIO 0x00000010

/* SDL2 AudioSpec layout for passing to SDL2_OpenAudioDevice */
typedef struct SDL2_AudioSpec {
    int freq;
    Uint16 format;
    Uint8 channels;
    Uint8 silence;
    Uint16 samples;
    Uint16 padding;
    Uint32 size;
    void (*callback)(void *userdata, Uint8 *stream, int len);
    void *userdata;
} SDL2_AudioSpec;

/* SDL2 AUDIO_ALLOW_ANY_CHANGE */
#define SDL2_AUDIO_ALLOW_ANY_CHANGE 0x0F

static bool SDL2AUDIO_WaitDevice(SDL_AudioDevice *device)
{
    SDL_Delay(device->hidden->io_delay);
    return true;
}

static bool SDL2AUDIO_PlayDevice(SDL_AudioDevice *device, const Uint8 *buffer, int buflen)
{
    if (!SDL2_QueueAudio_ptr || !device->hidden) {
        return false;
    }
    int rc = SDL2_QueueAudio_ptr(device->hidden->sdl2_dev, buffer, (Uint32)buflen);
    return (rc == 0);
}

static Uint8 *SDL2AUDIO_GetDeviceBuf(SDL_AudioDevice *device, int *buffer_size)
{
    if (buffer_size) {
        *buffer_size = device->hidden->mixbuf_size;
    }
    return device->hidden->mixbuf;
}

static int SDL2AUDIO_RecordDevice(SDL_AudioDevice *device, void *buffer, int buflen)
{
    if (!SDL2_DequeueAudio_ptr || !device->hidden) {
        return 0;
    }
    Uint32 got = SDL2_DequeueAudio_ptr(device->hidden->sdl2_dev, buffer, (Uint32)buflen);
    if (got == 0) {
        /* No data yet, fill with silence */
        SDL_memset(buffer, device->silence_value, buflen);
        return buflen;
    }
    return (int)got;
}

static bool SDL2AUDIO_OpenDevice(SDL_AudioDevice *device)
{
    if (!SDL2_OpenAudioDevice_ptr) {
        return SDL_SetError("SDL2 audio not loaded");
    }

    device->hidden = (struct SDL_PrivateAudioData *)SDL_calloc(1, sizeof(*device->hidden));
    if (!device->hidden) {
        return false;
    }

    /* Build SDL2 AudioSpec from device->spec */
    SDL2_AudioSpec desired, obtained;
    SDL_zero(desired);
    SDL_zero(obtained);

    desired.freq = device->spec.freq;
    desired.format = device->spec.format;  /* SDL2/SDL3 audio format values are compatible */
    desired.channels = (Uint8)device->spec.channels;
    desired.samples = (Uint16)device->sample_frames;
    desired.callback = NULL;  /* queue-based mode */
    desired.userdata = NULL;

    Uint32 dev = (Uint32)SDL2_OpenAudioDevice_ptr(
        NULL, device->recording ? 1 : 0,
        &desired, &obtained, SDL2_AUDIO_ALLOW_ANY_CHANGE
    );

    if (dev == 0) {
        SDL_free(device->hidden);
        device->hidden = NULL;
        return SDL_SetError("SDL2_OpenAudioDevice failed");
    }

    device->hidden->sdl2_dev = dev;
    device->hidden->io_delay = ((device->sample_frames * 1000) / device->spec.freq);

    if (!device->recording) {
        device->hidden->mixbuf_size = device->buffer_size;
        device->hidden->mixbuf = (Uint8 *)SDL_malloc(device->hidden->mixbuf_size);
        if (!device->hidden->mixbuf) {
            SDL2_CloseAudioDevice_ptr(dev);
            SDL_free(device->hidden);
            device->hidden = NULL;
            return false;
        }
    }

    /* Unpause the SDL2 device */
    if (SDL2_PauseAudioDevice_ptr) {
        SDL2_PauseAudioDevice_ptr(dev, 0);
    }

    return true;
}

static void SDL2AUDIO_CloseDevice(SDL_AudioDevice *device)
{
    if (device->hidden) {
        if (device->hidden->sdl2_dev && SDL2_CloseAudioDevice_ptr) {
            SDL2_CloseAudioDevice_ptr(device->hidden->sdl2_dev);
        }
        SDL_free(device->hidden->mixbuf);
        SDL_free(device->hidden);
        device->hidden = NULL;
    }
}

static void SDL2AUDIO_Deinitialize(void)
{
    /* Don't quit SDL2 audio here -- the video driver manages SDL2 lifecycle */
}

static bool SDL2AUDIO_Init(SDL_AudioDriverImpl *impl)
{
    /* Initialize SDL2 audio subsystem */
    if (SDL2_Init) {
        SDL2_Init(SDL2_INIT_AUDIO);
    }

    impl->OpenDevice = SDL2AUDIO_OpenDevice;
    impl->CloseDevice = SDL2AUDIO_CloseDevice;
    impl->WaitDevice = SDL2AUDIO_WaitDevice;
    impl->GetDeviceBuf = SDL2AUDIO_GetDeviceBuf;
    impl->PlayDevice = SDL2AUDIO_PlayDevice;
    impl->WaitRecordingDevice = SDL2AUDIO_WaitDevice;
    impl->RecordDevice = SDL2AUDIO_RecordDevice;
    impl->Deinitialize = SDL2AUDIO_Deinitialize;

    impl->OnlyHasDefaultPlaybackDevice = true;
    impl->OnlyHasDefaultRecordingDevice = true;
    impl->HasRecordingSupport = true;

    return true;
}

AudioBootStrap SDL2AUDIO_bootstrap = {
    "sdl2", "SDL2-delegating audio driver", SDL2AUDIO_Init, false, false
};

#endif /* SDL_AUDIO_DRIVER_SDL2 */
