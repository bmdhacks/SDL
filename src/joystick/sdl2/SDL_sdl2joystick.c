/*
  SDL3 joystick driver backed by SDL2 via dlopen.
  Delegates joystick/gamepad detection and input to the host SDL2,
  which on r36s handhelds has patched evdev/gptokeyb support.
*/
#include "SDL_internal.h"

#ifdef SDL_JOYSTICK_SDL2

#include "../SDL_sysjoystick.h"
#include "../SDL_joystick_c.h"
#include "SDL3/SDL_hints.h"

/* SDL2 init flags */
#define SDL2_INIT_JOYSTICK        0x00000200
#define SDL2_INIT_GAMECONTROLLER  0x00002000

/* SDL2 joystick types */
typedef Sint32 SDL2_JoystickID;

/* Maximum joysticks we track */
#define SDL2_MAX_JOYSTICKS 16

/* Per-joystick data */
typedef struct SDL2_JoystickData {
    SDL2_JoystickID sdl2_id;     /* SDL2 instance ID */
    int sdl2_index;              /* SDL2 device index at add time */
    SDL_JoystickID sdl3_id;     /* SDL3 instance ID */
    char name[256];
    SDL_GUID guid;
    int naxes, nbuttons, nhats;
    bool present;
    void *sdl2_joystick;        /* SDL2 SDL_Joystick* handle, opened by us */
} SDL2_JoystickData;

static SDL2_JoystickData s_joysticks[SDL2_MAX_JOYSTICKS];
static int s_num_joysticks = 0;
static SDL_JoystickID s_next_instance_id = 1;

/* SDL2 function pointers — prefixed sdl2joy_ to avoid colliding with driver functions */
static int (*sdl2joy_NumJoysticks)(void);
static const char *(*sdl2joy_JoystickNameForIndex)(int device_index);
static void *(*sdl2joy_JoystickOpen)(int device_index);
static void (*sdl2joy_JoystickClose)(void *joystick);
static SDL2_JoystickID (*sdl2joy_JoystickInstanceID)(void *joystick);
static int (*sdl2joy_JoystickNumAxes)(void *joystick);
static int (*sdl2joy_JoystickNumButtons)(void *joystick);
static int (*sdl2joy_JoystickNumHats)(void *joystick);
static Sint16 (*sdl2joy_JoystickGetAxis)(void *joystick, int axis);
static Uint8 (*sdl2joy_JoystickGetButton)(void *joystick, int button);
static Uint8 (*sdl2joy_JoystickGetHat)(void *joystick, int hat);
static void (*sdl2joy_JoystickUpdate)(void);
static SDL_GUID (*sdl2joy_JoystickGetGUID)(void *joystick);
static Uint16 (*sdl2joy_JoystickGetVendor)(void *joystick);
static Uint16 (*sdl2joy_JoystickGetProduct)(void *joystick);
static int (*sdl2joy_JoystickRumble)(void *joystick, Uint16 low, Uint16 high, Uint32 duration_ms);

/* SDL2 game controller (for mapping passthrough) */
static int (*sdl2joy_IsGameController)(int joystick_index);
static char *(*sdl2joy_GameControllerMappingForDeviceIndex)(int joystick_index);
static void (*sdl2joy_free)(void *mem);

/* SDL2 lifecycle — loaded by us, not borrowed from the video backend */
static int (*sdl2joy_Init)(Uint32 flags);
static void (*sdl2joy_QuitSubSystem)(Uint32 flags);
static const char *(*sdl2joy_GetError)(void);

#define LOAD_SDL2_JOY(var, name) do { \
    var = (typeof(var))SDL_LoadFunction(s_sdl2_handle, #name); \
} while (0)

static void *s_sdl2_handle = NULL;

static bool SDL2Joy_LoadFunctions(void)
{
    const char *sdl2_lib = SDL_getenv("SDL3SHIM_SDL2_LIB");
    if (!sdl2_lib || !*sdl2_lib) {
        sdl2_lib = "libSDL2-2.0.so.0";
    }
    s_sdl2_handle = SDL_LoadObject(sdl2_lib);
    if (!s_sdl2_handle) {
        return false;
    }

    LOAD_SDL2_JOY(sdl2joy_NumJoysticks, SDL_NumJoysticks);
    LOAD_SDL2_JOY(sdl2joy_JoystickNameForIndex, SDL_JoystickNameForIndex);
    LOAD_SDL2_JOY(sdl2joy_JoystickOpen, SDL_JoystickOpen);
    LOAD_SDL2_JOY(sdl2joy_JoystickClose, SDL_JoystickClose);
    LOAD_SDL2_JOY(sdl2joy_JoystickInstanceID, SDL_JoystickInstanceID);
    LOAD_SDL2_JOY(sdl2joy_JoystickNumAxes, SDL_JoystickNumAxes);
    LOAD_SDL2_JOY(sdl2joy_JoystickNumButtons, SDL_JoystickNumButtons);
    LOAD_SDL2_JOY(sdl2joy_JoystickNumHats, SDL_JoystickNumHats);
    LOAD_SDL2_JOY(sdl2joy_JoystickGetAxis, SDL_JoystickGetAxis);
    LOAD_SDL2_JOY(sdl2joy_JoystickGetButton, SDL_JoystickGetButton);
    LOAD_SDL2_JOY(sdl2joy_JoystickGetHat, SDL_JoystickGetHat);
    LOAD_SDL2_JOY(sdl2joy_JoystickUpdate, SDL_JoystickUpdate);
    LOAD_SDL2_JOY(sdl2joy_JoystickGetGUID, SDL_JoystickGetGUID);
    LOAD_SDL2_JOY(sdl2joy_JoystickGetVendor, SDL_JoystickGetVendor);
    LOAD_SDL2_JOY(sdl2joy_JoystickGetProduct, SDL_JoystickGetProduct);
    LOAD_SDL2_JOY(sdl2joy_JoystickRumble, SDL_JoystickRumble);
    LOAD_SDL2_JOY(sdl2joy_Init, SDL_Init);
    LOAD_SDL2_JOY(sdl2joy_QuitSubSystem, SDL_QuitSubSystem);
    LOAD_SDL2_JOY(sdl2joy_GetError, SDL_GetError);
    LOAD_SDL2_JOY(sdl2joy_IsGameController, SDL_IsGameController);
    LOAD_SDL2_JOY(sdl2joy_GameControllerMappingForDeviceIndex, SDL_GameControllerMappingForDeviceIndex);
    LOAD_SDL2_JOY(sdl2joy_free, SDL_free);

    if (!sdl2joy_NumJoysticks || !sdl2joy_JoystickOpen || !sdl2joy_JoystickClose ||
        !sdl2joy_JoystickGetAxis || !sdl2joy_JoystickGetButton || !sdl2joy_Init) {
        SDL_UnloadObject(s_sdl2_handle);
        s_sdl2_handle = NULL;
        return false;
    }

    return true;
}

/* Helper to populate a joystick data entry from an opened SDL2 joystick */
static void SDL2Joy_PopulateDevice(SDL2_JoystickData *jd, int sdl2_index, void *js)
{
    jd->sdl2_index = sdl2_index;
    jd->sdl2_id = sdl2joy_JoystickInstanceID ? sdl2joy_JoystickInstanceID(js) : sdl2_index;
    jd->sdl3_id = s_next_instance_id++;
    jd->present = true;
    jd->sdl2_joystick = js;

    const char *name = sdl2joy_JoystickNameForIndex ? sdl2joy_JoystickNameForIndex(sdl2_index) : NULL;
    if (name) {
        SDL_strlcpy(jd->name, name, sizeof(jd->name));
    } else {
        SDL_snprintf(jd->name, sizeof(jd->name), "Joystick %d", sdl2_index);
    }

    jd->naxes = sdl2joy_JoystickNumAxes ? sdl2joy_JoystickNumAxes(js) : 0;
    jd->nbuttons = sdl2joy_JoystickNumButtons ? sdl2joy_JoystickNumButtons(js) : 0;
    jd->nhats = sdl2joy_JoystickNumHats ? sdl2joy_JoystickNumHats(js) : 0;

    if (sdl2joy_JoystickGetGUID) {
        jd->guid = sdl2joy_JoystickGetGUID(js);
    }
}

/* === Driver interface === */

static bool SDL2_JoystickInit(void)
{
    if (!SDL2Joy_LoadFunctions()) {
        return SDL_SetError("SDL2 joystick: failed to load SDL2 symbols");
    }

    /* The SDL2 backend runs on embedded devices without real window focus
     * management.  SDL3 silently drops ALL joystick input when keyboard focus
     * is NULL (SDL_PrivateJoystickShouldIgnoreEvent).  Bypass that check. */
    SDL_SetHint(SDL_HINT_JOYSTICK_ALLOW_BACKGROUND_EVENTS, "1");

    /* Init SDL2 joystick subsystem using our own handle */
    if (sdl2joy_Init(SDL2_INIT_JOYSTICK | SDL2_INIT_GAMECONTROLLER) < 0) {
        return SDL_SetError("SDL2 joystick: SDL2_Init failed: %s",
                            sdl2joy_GetError ? sdl2joy_GetError() : "unknown");
    }

    SDL_zero(s_joysticks);
    s_num_joysticks = 0;

    /* Enumerate existing joysticks */
    int count = sdl2joy_NumJoysticks();
    for (int i = 0; i < count && s_num_joysticks < SDL2_MAX_JOYSTICKS; i++) {
        void *js = sdl2joy_JoystickOpen(i);
        if (!js) continue;

        SDL2Joy_PopulateDevice(&s_joysticks[s_num_joysticks], i, js);
        s_num_joysticks++;
        SDL_PrivateJoystickAdded(s_joysticks[s_num_joysticks - 1].sdl3_id);
    }

    /* Inject SDL2's game controller mappings into SDL3.
     * This makes SDL_IsGamepad() return true for devices that SDL2 recognizes,
     * so games that only accept gamepads (not raw joysticks) will work. */
    if (sdl2joy_IsGameController && sdl2joy_GameControllerMappingForDeviceIndex) {
        for (int i = 0; i < count; i++) {
            if (sdl2joy_IsGameController(i)) {
                char *mapping = sdl2joy_GameControllerMappingForDeviceIndex(i);
                if (mapping) {
                    SDL_AddGamepadMapping(mapping);
                    if (sdl2joy_free) sdl2joy_free(mapping);
                }
            }
        }
    }

    for (int i = 0; i < s_num_joysticks; i++) {
        char guid_str[64];
        SDL_GUIDToString(s_joysticks[i].guid, guid_str, sizeof(guid_str));
        SDL_Log("SDL2 joystick dev%d: name='%s' guid=%s axes=%d btn=%d hat=%d gamepad=%d",
                i, s_joysticks[i].name, guid_str,
                s_joysticks[i].naxes, s_joysticks[i].nbuttons, s_joysticks[i].nhats,
                SDL_IsGamepad(s_joysticks[i].sdl3_id));
    }

    return true;
}

static int SDL2_JoystickGetCount(void)
{
    return s_num_joysticks;
}

static void SDL2_JoystickDetect(void)
{
    if (!sdl2joy_NumJoysticks) return;

    int sdl2_count = sdl2joy_NumJoysticks();

    if (sdl2_count > s_num_joysticks) {
        for (int i = s_num_joysticks; i < sdl2_count && i < SDL2_MAX_JOYSTICKS; i++) {
            void *js = sdl2joy_JoystickOpen(i);
            if (!js) continue;

            SDL2Joy_PopulateDevice(&s_joysticks[s_num_joysticks], i, js);
            s_num_joysticks++;
            SDL_PrivateJoystickAdded(s_joysticks[s_num_joysticks - 1].sdl3_id);
        }
    }
}

static bool SDL2_JoystickIsDevicePresent(Uint16 vendor_id, Uint16 product_id, Uint16 version, const char *name)
{
    for (int i = 0; i < s_num_joysticks; i++) {
        if (!s_joysticks[i].present) continue;
        if (sdl2joy_JoystickGetVendor && sdl2joy_JoystickGetProduct && s_joysticks[i].sdl2_joystick) {
            Uint16 v = sdl2joy_JoystickGetVendor(s_joysticks[i].sdl2_joystick);
            Uint16 p = sdl2joy_JoystickGetProduct(s_joysticks[i].sdl2_joystick);
            if (v == vendor_id && p == product_id) {
                return true;
            }
        }
    }
    return false;
}

static const char *SDL2_JoystickGetDeviceName(int device_index)
{
    if (device_index < 0 || device_index >= s_num_joysticks) return NULL;
    return s_joysticks[device_index].name;
}

static const char *SDL2_JoystickGetDevicePath(int device_index)
{
    return NULL;
}

static int SDL2_JoystickGetDeviceSteamVirtualGamepadSlot(int device_index)
{
    return -1;
}

static int SDL2_JoystickGetDevicePlayerIndex(int device_index)
{
    return -1;
}

static void SDL2_JoystickSetDevicePlayerIndex(int device_index, int player_index)
{
}

static SDL_GUID SDL2_JoystickGetDeviceGUID(int device_index)
{
    if (device_index >= 0 && device_index < s_num_joysticks) {
        return s_joysticks[device_index].guid;
    }
    SDL_GUID guid;
    SDL_zero(guid);
    return guid;
}

static SDL_JoystickID SDL2_JoystickGetDeviceInstanceID(int device_index)
{
    if (device_index >= 0 && device_index < s_num_joysticks) {
        return s_joysticks[device_index].sdl3_id;
    }
    return 0;
}

static bool SDL2_JoystickOpen(SDL_Joystick *joystick, int device_index)
{
    if (device_index < 0 || device_index >= s_num_joysticks) {
        return SDL_SetError("Invalid device index %d", device_index);
    }

    SDL2_JoystickData *jd = &s_joysticks[device_index];

    if (!jd->sdl2_joystick) {
        return SDL_SetError("SDL2 joystick not open");
    }

    joystick->naxes = jd->naxes;
    joystick->nbuttons = jd->nbuttons;
    joystick->nhats = jd->nhats;
    joystick->hwdata = (struct joystick_hwdata *)(uintptr_t)device_index;

    return true;
}

static bool SDL2_JoystickRumbleFunc(SDL_Joystick *joystick, Uint16 low_frequency_rumble, Uint16 high_frequency_rumble)
{
    if (!sdl2joy_JoystickRumble) return SDL_Unsupported();

    int idx = (int)(uintptr_t)joystick->hwdata;
    if (idx < 0 || idx >= s_num_joysticks || !s_joysticks[idx].sdl2_joystick) {
        return SDL_Unsupported();
    }

    int result = sdl2joy_JoystickRumble(s_joysticks[idx].sdl2_joystick,
                                         low_frequency_rumble, high_frequency_rumble, 0);
    return (result == 0);
}

static bool SDL2_JoystickRumbleTriggers(SDL_Joystick *joystick, Uint16 left_rumble, Uint16 right_rumble)
{
    return SDL_Unsupported();
}

static bool SDL2_JoystickSetLED(SDL_Joystick *joystick, Uint8 red, Uint8 green, Uint8 blue)
{
    return SDL_Unsupported();
}

static bool SDL2_JoystickSendEffect(SDL_Joystick *joystick, const void *data, int size)
{
    return SDL_Unsupported();
}

static bool SDL2_JoystickSetSensorsEnabled(SDL_Joystick *joystick, bool enabled)
{
    return SDL_Unsupported();
}

static void SDL2_JoystickUpdate(SDL_Joystick *joystick)
{
    int idx = (int)(uintptr_t)joystick->hwdata;
    if (idx < 0 || idx >= s_num_joysticks) return;

    SDL2_JoystickData *jd = &s_joysticks[idx];
    if (!jd->sdl2_joystick) return;

    /* Tell SDL2 to update its internal joystick state */
    if (sdl2joy_JoystickUpdate) {
        sdl2joy_JoystickUpdate();
    }

    Uint64 timestamp = SDL_GetTicksNS();

    /* Read axes */
    for (int i = 0; i < jd->naxes; i++) {
        Sint16 value = sdl2joy_JoystickGetAxis(jd->sdl2_joystick, i);
        SDL_SendJoystickAxis(timestamp, joystick, (Uint8)i, value);
    }

    /* Read buttons */
    for (int i = 0; i < jd->nbuttons; i++) {
        Uint8 value = sdl2joy_JoystickGetButton(jd->sdl2_joystick, i);
        SDL_SendJoystickButton(timestamp, joystick, (Uint8)i, value != 0);
    }

    /* Read hats */
    if (sdl2joy_JoystickGetHat) {
        for (int i = 0; i < jd->nhats; i++) {
            Uint8 value = sdl2joy_JoystickGetHat(jd->sdl2_joystick, i);
            SDL_SendJoystickHat(timestamp, joystick, (Uint8)i, value);
        }
    }
}

static void SDL2_JoystickClose(SDL_Joystick *joystick)
{
    joystick->hwdata = NULL;
}

static void SDL2_JoystickQuit(void)
{
    for (int i = 0; i < s_num_joysticks; i++) {
        if (s_joysticks[i].sdl2_joystick && sdl2joy_JoystickClose) {
            sdl2joy_JoystickClose(s_joysticks[i].sdl2_joystick);
            s_joysticks[i].sdl2_joystick = NULL;
        }
    }
    s_num_joysticks = 0;

    if (sdl2joy_QuitSubSystem) {
        sdl2joy_QuitSubSystem(SDL2_INIT_JOYSTICK | SDL2_INIT_GAMECONTROLLER);
    }

    if (s_sdl2_handle) {
        SDL_UnloadObject(s_sdl2_handle);
        s_sdl2_handle = NULL;
    }
}

static bool SDL2_JoystickGetGamepadMapping(int device_index, SDL_GamepadMapping *out)
{
    return false;
}

SDL_JoystickDriver SDL_SDL2_JoystickDriver = {
    SDL2_JoystickInit,
    SDL2_JoystickGetCount,
    SDL2_JoystickDetect,
    SDL2_JoystickIsDevicePresent,
    SDL2_JoystickGetDeviceName,
    SDL2_JoystickGetDevicePath,
    SDL2_JoystickGetDeviceSteamVirtualGamepadSlot,
    SDL2_JoystickGetDevicePlayerIndex,
    SDL2_JoystickSetDevicePlayerIndex,
    SDL2_JoystickGetDeviceGUID,
    SDL2_JoystickGetDeviceInstanceID,
    SDL2_JoystickOpen,
    SDL2_JoystickRumbleFunc,
    SDL2_JoystickRumbleTriggers,
    SDL2_JoystickSetLED,
    SDL2_JoystickSendEffect,
    SDL2_JoystickSetSensorsEnabled,
    SDL2_JoystickUpdate,
    SDL2_JoystickClose,
    SDL2_JoystickQuit,
    SDL2_JoystickGetGamepadMapping,
};

#endif /* SDL_JOYSTICK_SDL2 */
