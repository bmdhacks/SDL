/*
  sdl3forsdl2 — SDL2/SDL3 namespace collision detection and escape hatch.

  ## Problem

  Both libSDL3.so.0 and libSDL2-2.0.so.0 export identically-named symbols
  (SDL_LockTexture, SDL_RenderPresent, ...) with INCOMPATIBLE return
  conventions (SDL2: int 0/-1; SDL3: bool true/false). If both libraries
  end up in the same process's global symbol scope, `dlsym(RTLD_DEFAULT,
  "SDL_LockTexture")` silently returns whichever was first in load order
  — typically SDL2 if a preloaded library pulled it in transitively.

  This bites on muOS, where the frontend preloads libmustage.so whose
  DT_NEEDED chain pulls in libSDL2 with RTLD_GLOBAL semantics. Any
  consumer (e.g. an override library loaded under machismo) that resolves
  SDL3 functions via RTLD_DEFAULT gets SDL2's version, which rejects the
  SDL3 SDL_Texture* with -1. The caller treats -1 as truthy ("success
  under SDL3 semantics") and dereferences a NULL pixel pointer.

  ## Fix strategy

  We attack the problem two ways:

  1. **DF_1_INTERPOSE** (via `-Wl,-z,interpose` in CMakeLists.txt). glibc
     puts interposing libraries at the front of the global symbol search
     order. The catch: glibc only sorts interposers at *load time* link-map
     construction, not when a library is promoted from RTLD_LOCAL to
     RTLD_GLOBAL via dlopen. So this only helps when libSDL3 is loaded as
     LD_PRELOAD, as a DT_NEEDED of the main executable, or via an early
     enough dlopen(RTLD_GLOBAL).

     Concretely: prepend libSDL3 to LD_PRELOAD in the launcher script —
     `LD_PRELOAD="$GAMEDIR/libs/libSDL3.so.0:${LD_PRELOAD}"` — and the
     interpose flag makes it shadow any SDL2 that the preload chain brings
     in. No consumer code changes needed.

  2. **SDL3SHIM_dlsym** (exported below) — escape hatch for consumers who
     can't control LD_PRELOAD. Resolves against libSDL3's own handle,
     bypassing the global scope entirely:

         void *(*get)(const char *) = dlsym(RTLD_DEFAULT, "SDL3SHIM_dlsym");
         p_SDL_LockTexture = get ? get("SDL_LockTexture")
                                 : dlsym(RTLD_DEFAULT, "SDL_LockTexture");

     SDL3SHIM_dlsym is itself a unique name with no SDL2 equivalent, so
     RTLD_DEFAULT will always find ours.

  We also log a diagnostic at constructor time when libSDL2 is detected in
  the same process — so when the collision is happening, operators see it
  in stderr with an actionable hint instead of debugging from a segfault.
*/

#include "SDL_internal.h"

#ifdef SDL_VIDEO_DRIVER_SDL2

#include <dlfcn.h>
#include <stdio.h>

#define SDL3SHIM_SONAME "libSDL3.so.0"
#define SDL2_SONAME     "libSDL2-2.0.so.0"

/* Cached handle to ourselves, resolved on first SDL3SHIM_dlsym call. */
static void *self_handle = NULL;

/*
  Public escape-hatch helper. Consumers who use dlsym to resolve SDL3
  functions should prefer this over dlsym(RTLD_DEFAULT, ...) when they
  cannot guarantee libSDL3 is the first SDL in the global scope.

  Returns the libSDL3 export for `name`, or NULL if not found. Unlike
  RTLD_DEFAULT, this never returns an SDL2 function with the same name.
*/
/* NOTE on visibility: SDL_internal.h #undefs SDL_DECLSPEC for the rest
 * of the SDL3 build (internal-only symbols), so the version-script
 * approach plus default visibility is the right way to expose this. */
__attribute__((visibility("default")))
void *SDL3SHIM_dlsym(const char *name)
{
    if (!self_handle) {
        self_handle = dlopen(SDL3SHIM_SONAME, RTLD_NOW | RTLD_NOLOAD);
    }
    if (!self_handle || !name) {
        return NULL;
    }
    return dlsym(self_handle, name);
}

__attribute__((constructor))
static void sdl3shim_namespace_init(void)
{
    /* Best-effort self-promotion. Doesn't help interpose (glibc won't
     * re-sort after load), but does let RTLD_DEFAULT find SDL3-only
     * symbols like SDL_SetRenderVSync without an explicit handle. Cheap. */
    (void)dlopen(SDL3SHIM_SONAME, RTLD_NOW | RTLD_GLOBAL | RTLD_NOLOAD);

    /* Detect external libSDL2. RTLD_NOLOAD returns the existing handle
     * or NULL — it never loads. dlclose to leave the refcount untouched. */
    void *sdl2 = dlopen(SDL2_SONAME, RTLD_NOW | RTLD_NOLOAD);
    if (sdl2) {
        fprintf(stderr,
                "[sdl3forsdl2] WARN: %s is loaded alongside %s in this process.\n"
                "  Both export SDL_* with incompatible return conventions.\n"
                "  dlsym(RTLD_DEFAULT, \"SDL_*\") in consumer libraries may\n"
                "  resolve to SDL2 instead of SDL3 and crash.\n"
                "  Mitigations:\n"
                "    - Add %s to LD_PRELOAD (DF_1_INTERPOSE will shadow SDL2)\n"
                "    - Or consumers can call SDL3SHIM_dlsym(\"SDL_*\") instead\n"
                "      of dlsym(RTLD_DEFAULT, \"SDL_*\").\n",
                SDL2_SONAME, SDL3SHIM_SONAME, SDL3SHIM_SONAME);
        dlclose(sdl2);
    }
}

#endif /* SDL_VIDEO_DRIVER_SDL2 */
