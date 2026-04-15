# SDL3 with SDL2 Backend

Fork of [SDL3](https://github.com/libsdl-org/SDL) that adds video, audio, joystick, and GPU backends which delegate to an existing SDL2 installation via dlopen. This lets SDL3 applications run on embedded Linux systems (like the r36s and similar handhelds) that ship a patched SDL2 but lack the display server infrastructure SDL3 normally requires.

Upstream: [libsdl-org/SDL](https://github.com/libsdl-org/SDL)
Fork: [bmdhacks/SDL](https://github.com/bmdhacks/SDL/tree/sdl2-backend) (branch `sdl2-backend`)

## Why

SDL3's Linux video drivers require X11, Wayland, or KMS/DRM. Many embedded Linux handhelds (r36s, RG35XX, TrimUI, etc.) ship with a vendor-patched SDL2 that talks directly to the display hardware via fbdev or a custom EGL platform. This fork bridges the gap: SDL3 applications get full SDL3 API support while the actual hardware access goes through the host SDL2.

## What Works

| Subsystem | Backend | Notes |
|-----------|---------|-------|
| Video | `src/video/sdl2/` | Window management, display enumeration, GL context, event pump |
| Audio | `src/audio/sdl2/` | Device management via `SDL2_QueueAudio` |
| Joystick | `src/joystick/sdl2/` | Delegates to SDL2's evdev path, injects SDL2 gamepad mappings into SDL3 |
| GPU | `src/gpu/gles/` | GLES 3.x via SPIRV-Cross (SPIR-V to GLSL ES transpilation) |
| Everything else | Native SDL3 | Properties, IOStream, filesystem, threads, 2D renderer, etc. |

The GPU backend implements shader compilation, graphics pipelines, buffers, textures, samplers, transfer buffers, draw calls, and swapchain management. Compute pipelines, mipmap generation, blit, and some copy operations are not yet implemented.

## Building

### Prerequisites

- CMake 3.16+
- A C compiler (GCC or Clang)
- An SDL2 shared library on the target system (or build one from source)
- [SPIRV-Cross](https://github.com/KhronosGroup/SPIRV-Cross) source (for GPU shader transpilation)

### Quick Start

```bash
git clone --recursive https://github.com/bmdhacks/SDL.git -b sdl2-backend
cd SDL

# Get SPIRV-Cross if not included as a submodule
git clone https://github.com/KhronosGroup/SPIRV-Cross.git /path/to/SPIRV-Cross

mkdir build && cd build
cmake .. \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_C_FLAGS="-mcpu=cortex-a35" \
  -DSDL_SDL2_BACKEND=ON \
  -DSDL_SPIRV_CROSS_DIR=/path/to/SPIRV-Cross \
  -DSDL_X11=OFF -DSDL_WAYLAND=OFF -DSDL_KMSDRM=OFF \
  -DSDL_PIPEWIRE=OFF -DSDL_PULSEAUDIO=OFF -DSDL_ALSA=OFF \
  -DSDL_SNDIO=OFF -DSDL_OSS=OFF -DSDL_JACK=OFF \
  -DSDL_OFFSCREEN=OFF -DSDL_DUMMYVIDEO=OFF \
  -DSDL_DUMMYAUDIO=OFF -DSDL_DISKAUDIO=OFF \
  -DSDL_VULKAN=OFF -DSDL_GPU=ON -DSDL_RENDER_GPU=ON
make -j$(nproc)
```

**Important**: Set `-mcpu` to match your target SoC. Common embedded Linux handhelds:

| SoC | CPU | Flag |
|-----|-----|------|
| RK3326 (r36s, RG351P, RGB10) | Cortex-A35 | `-mcpu=cortex-a35` |
| RK3566 (RG353P, RG503) | Cortex-A55 | `-mcpu=cortex-a55` |
| Allwinner H700 (RG35XX Plus) | Cortex-A53 | `-mcpu=cortex-a53` |
| Amlogic S922X (RG552) | Cortex-A73/A53 | `-mcpu=cortex-a73.cortex-a53` |

If you build on a more powerful aarch64 host (e.g. Apple Silicon, Ampere), the compiler defaults to the host CPU's instruction set, which includes ARMv8.1+ instructions that don't exist on older cores. Always set `-mcpu` explicitly.

The key CMake options:

| Option | Default | Description |
|--------|---------|-------------|
| `SDL_SDL2_BACKEND` | `OFF` | Enable the SDL2-delegating backends for video, audio, and joystick |
| `SDL_SPIRV_CROSS_DIR` | (empty) | Path to SPIRV-Cross source tree (required when `SDL_SDL2_BACKEND=ON`) |

When `SDL_SDL2_BACKEND` is enabled, the native X11/Wayland/KMS video drivers, ALSA/PulseAudio/PipeWire audio drivers, and HIDAPI joystick driver are automatically disabled to avoid conflicts.

### Cross-Compiling for aarch64

```bash
cmake .. \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_TOOLCHAIN_FILE=/path/to/aarch64-toolchain.cmake \
  -DCMAKE_C_FLAGS="-mcpu=cortex-a35" \
  -DSDL_SDL2_BACKEND=ON \
  -DSDL_SPIRV_CROSS_DIR=/path/to/SPIRV-Cross \
  -DSDL_X11=OFF -DSDL_WAYLAND=OFF -DSDL_KMSDRM=OFF \
  -DSDL_PIPEWIRE=OFF -DSDL_PULSEAUDIO=OFF -DSDL_ALSA=OFF \
  -DSDL_SNDIO=OFF -DSDL_OSS=OFF -DSDL_JACK=OFF \
  -DSDL_OFFSCREEN=OFF -DSDL_DUMMYVIDEO=OFF \
  -DSDL_DUMMYAUDIO=OFF -DSDL_DISKAUDIO=OFF \
  -DSDL_VULKAN=OFF -DSDL_GPU=ON -DSDL_RENDER_GPU=ON
make -j$(nproc)
```

## Runtime Configuration

### Environment Variables

| Variable | Required | Description |
|----------|----------|-------------|
| `SDL3SHIM_SDL2_LIB` | Usually | Full path to the real SDL2 shared library (e.g. `/usr/lib/libSDL2-2.0.so.0`). Required when sdl2-compat provides the system `libSDL2` to avoid circular loading. If unset, falls back to `libSDL2-2.0.so.0` via normal library search. |
| `SDL3SHIM_SDL2_VIDEODRIVER` | No | Passed through to SDL2 as `SDL_VIDEODRIVER` before SDL2 init. Useful for selecting SDL2's video backend (e.g. `fbdev`, `kmsdrm`, `x11`). |
| `SDL3SHIM_SDL2_AUDIODRIVER` | No | Passed through to SDL2 as `SDL_AUDIODRIVER` before SDL2 init. Useful for selecting SDL2's audio backend (e.g. `alsa`, `pulseaudio`, `pipewire`). |
| `LD_LIBRARY_PATH` | Usually | Should include the directory containing the built `libSDL3.so` and, if built from source, the SDL2 library. |

### Example Launch Script

```bash
#!/bin/bash
# Launch an SDL3 app on an embedded system with custom SDL2
export SDL3SHIM_SDL2_LIB=/usr/lib/libSDL2-2.0.so.0
export LD_LIBRARY_PATH=/opt/sdl3/lib:$LD_LIBRARY_PATH

# Optional: tell SDL2 which video driver to use
# export SDL3SHIM_SDL2_VIDEODRIVER=fbdev

./my_sdl3_app
```

## How It Works

The SDL2 backends load SDL2 at runtime via `dlopen` and call SDL2 functions through function pointers. No SDL2 headers are included in the SDL3 build -- all SDL2 types are redefined locally as opaque pointers or compatible structs.

**Video**: Creates SDL2 windows, proxies GL context creation, and translates SDL2 events (window, keyboard, mouse) into SDL3's internal event system.

**Audio**: Opens SDL2 audio devices and feeds audio data through `SDL2_QueueAudio`.

**Joystick**: Initializes SDL2's joystick and game controller subsystems, polls SDL2's cached joystick state each frame, and pushes axis/button/hat values into SDL3 via `SDL_SendJoystickAxis`/`Button`/`Hat`. Automatically injects SDL2's game controller mappings into SDL3 so `SDL_IsGamepad()` works for devices in SDL2's controller database.

**GPU**: A standalone GLES 3.x backend (SDL3 has no built-in OpenGL/GLES GPU backend). Uses SPIRV-Cross to transpile SPIR-V shaders to GLSL ES 3.10 at runtime. Provides the full SDL3 GPU API for shader compilation, pipeline creation, buffer/texture management, and draw calls.

## Limitations

- Compute pipelines are not implemented (will assert if called)
- Some GPU copy/blit operations are not yet implemented
- The SDL2 backend uses a single static command buffer (immediate-mode GLES), so multi-threaded rendering is not supported
- The joystick driver does not support hot-plug detection of new devices after init
- No Vulkan support (use the GLES GPU backend instead)

## License

This fork is distributed under the same [zlib license](LICENSE.txt) as SDL3.
