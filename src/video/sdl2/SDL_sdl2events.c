/*
  SDL2 backend - event pump
  Translates SDL2 events to SDL3 internal event functions.
*/
#include "SDL_internal.h"

#ifdef SDL_VIDEO_DRIVER_SDL2

#include "../SDL_sysvideo.h"
#include "../../events/SDL_events_c.h"
#include "../../events/SDL_keyboard_c.h"
#include "../../events/SDL_mouse_c.h"
#include "../../events/SDL_windowevents_c.h"
#include "SDL_sdl2video.h"
#include "SDL_sdl2events.h"

/* Find SDL3 window by SDL2 window ID */
static SDL_Window *FindWindowBySDL2ID(Uint32 sdl2_id)
{
    SDL_VideoDevice *_this = SDL_GetVideoDevice();
    if (!_this) {
        return NULL;
    }

    for (SDL_Window *w = _this->windows; w; w = w->next) {
        SDL_WindowData *data = w->internal;
        if (data && data->sdl2_window_id == sdl2_id) {
            return w;
        }
    }
    return NULL;
}

void SDL2_PumpEvents_Impl(SDL_VideoDevice *_this)
{
    if (!SDL2_PollEvent) {
        return;
    }

    SDL2_Event e;
    while (SDL2_PollEvent(&e)) {
        Uint64 timestamp = (Uint64)e.window.timestamp * SDL_NS_PER_MS;

        switch (e.type) {
        case SDL2_WINDOWEVENT: {
            SDL_Window *win = FindWindowBySDL2ID(e.window.windowID);
            if (!win) break;

            switch (e.window.event) {
            case SDL2_WINDOWEVENT_SHOWN:
                SDL_SendWindowEvent(win, SDL_EVENT_WINDOW_SHOWN, 0, 0);
                break;
            case SDL2_WINDOWEVENT_HIDDEN:
                SDL_SendWindowEvent(win, SDL_EVENT_WINDOW_HIDDEN, 0, 0);
                break;
            case SDL2_WINDOWEVENT_EXPOSED:
                SDL_SendWindowEvent(win, SDL_EVENT_WINDOW_EXPOSED, 0, 0);
                break;
            case SDL2_WINDOWEVENT_MOVED:
                SDL_SendWindowEvent(win, SDL_EVENT_WINDOW_MOVED,
                                    e.window.data1, e.window.data2);
                break;
            case SDL2_WINDOWEVENT_RESIZED:
            case SDL2_WINDOWEVENT_SIZE_CHANGED:
                SDL_SendWindowEvent(win, SDL_EVENT_WINDOW_RESIZED,
                                    e.window.data1, e.window.data2);
                break;
            case SDL2_WINDOWEVENT_MINIMIZED:
                SDL_SendWindowEvent(win, SDL_EVENT_WINDOW_MINIMIZED, 0, 0);
                break;
            case SDL2_WINDOWEVENT_MAXIMIZED:
                SDL_SendWindowEvent(win, SDL_EVENT_WINDOW_MAXIMIZED, 0, 0);
                break;
            case SDL2_WINDOWEVENT_RESTORED:
                SDL_SendWindowEvent(win, SDL_EVENT_WINDOW_RESTORED, 0, 0);
                break;
            case SDL2_WINDOWEVENT_ENTER:
                SDL_SendWindowEvent(win, SDL_EVENT_WINDOW_MOUSE_ENTER, 0, 0);
                break;
            case SDL2_WINDOWEVENT_LEAVE:
                SDL_SendWindowEvent(win, SDL_EVENT_WINDOW_MOUSE_LEAVE, 0, 0);
                break;
            case SDL2_WINDOWEVENT_FOCUS_GAINED:
                SDL_SendWindowEvent(win, SDL_EVENT_WINDOW_FOCUS_GAINED, 0, 0);
                break;
            case SDL2_WINDOWEVENT_FOCUS_LOST:
                SDL_SendWindowEvent(win, SDL_EVENT_WINDOW_FOCUS_LOST, 0, 0);
                break;
            case SDL2_WINDOWEVENT_CLOSE:
                SDL_SendWindowEvent(win, SDL_EVENT_WINDOW_CLOSE_REQUESTED, 0, 0);
                break;
            }
            break;
        }

        case SDL2_KEYDOWN:
        case SDL2_KEYUP: {
            SDL_Window *win = FindWindowBySDL2ID(e.key.windowID);
            /* SDL2 scancodes are mostly compatible with SDL3 scancodes */
            SDL_Scancode scancode = (SDL_Scancode)e.key.keysym.scancode;
            bool down = (e.type == SDL2_KEYDOWN);
            SDL_SendKeyboardKey(timestamp, SDL_GLOBAL_KEYBOARD_ID,
                                e.key.keysym.scancode, scancode, down);
            (void)win;
            break;
        }

        case SDL2_TEXTINPUT: {
            SDL_SendKeyboardText(e.text.text);
            break;
        }

        case SDL2_MOUSEMOTION: {
            SDL_Window *win = FindWindowBySDL2ID(e.motion.windowID);
            if (win) {
                bool relative = (e.motion.which != 0); /* rough check */
                SDL_SendMouseMotion(timestamp, win, SDL_GLOBAL_MOUSE_ID,
                                    relative,
                                    (float)e.motion.x, (float)e.motion.y);
            }
            break;
        }

        case SDL2_MOUSEBUTTONDOWN:
        case SDL2_MOUSEBUTTONUP: {
            SDL_Window *win = FindWindowBySDL2ID(e.button.windowID);
            if (win) {
                bool down = (e.type == SDL2_MOUSEBUTTONDOWN);
                SDL_SendMouseButton(timestamp, win, SDL_GLOBAL_MOUSE_ID,
                                    e.button.button, down);
            }
            break;
        }

        case SDL2_MOUSEWHEEL: {
            SDL_Window *win = FindWindowBySDL2ID(e.wheel.windowID);
            if (win) {
                float wx = (float)e.wheel.x;
                float wy = (float)e.wheel.y;
                if (e.wheel.direction == 1) { /* SDL_MOUSEWHEEL_FLIPPED */
                    wx = -wx;
                    wy = -wy;
                }
                SDL_SendMouseWheel(timestamp, win, SDL_GLOBAL_MOUSE_ID,
                                   wx, wy, SDL_MOUSEWHEEL_NORMAL);
            }
            break;
        }

        case SDL2_QUIT:
            SDL_SendQuit();
            break;

        default:
            break;
        }
    }

    /* Ensure keyboard focus after processing events.  Must be AFTER the loop
     * because FOCUS_LOST events (common on embedded/no-WM setups) clear it. */
    if (_this->windows && !SDL_GetKeyboardFocus()) {
        SDL_SetKeyboardFocus(_this->windows);
    }
}

#endif /* SDL_VIDEO_DRIVER_SDL2 */
