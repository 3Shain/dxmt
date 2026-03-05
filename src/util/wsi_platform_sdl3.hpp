#pragma once

#if defined(DXMT_WSI_SDL3)

#include "wsi_monitor.hpp"

#include <SDL3/SDL.h>

namespace dxmt::wsi {

class SDL3Initializer { 
    public:
void* libsdl;
#define SDL_PROC(ret, name, params) \
  typedef ret (SDLCALL *pfn_##name) params; \
  pfn_##name name;
#include "wsi_platform_sdl3_funcs.hpp"

    SDL3Initializer();

    static SDL3Initializer get() {
        static SDL3Initializer instance;
        return instance;
    }
};

static inline SDL_DisplayID fromHmonitor(HMONITOR hMonitor) {
  return static_cast<SDL_DisplayID>(reinterpret_cast<uintptr_t>(hMonitor));
}

static inline HMONITOR toHmonitor(SDL_DisplayID displayId) {
  return reinterpret_cast<HMONITOR>(static_cast<uintptr_t>(displayId));
}

static inline bool isValidDisplay(SDL_DisplayID displayId) {
    int count = 0;
    SDL_DisplayID *displays = SDL3Initializer::get().SDL_GetDisplays(&count);
    if (!displays)
      return false;

    bool found = false;
    for (int i = 0; i < count; i++) {
      if (displays[i] == displayId) {
        found = true;
        break;
      }
    }
    SDL3Initializer::get().SDL_free(displays);
    return found;
}

static inline SDL_Window* fromHwnd(HWND hWindow) {
  return reinterpret_cast<SDL_Window*>(hWindow);
}

static inline HWND toHwnd(SDL_Window* pWindow) {
  return reinterpret_cast<HWND>(pWindow);
}

static inline uint32_t roundToNextPow2(uint32_t num) {
  if (num-- == 0)
    return 0;
  num |= num >> 1; num |= num >> 2;
  num |= num >> 4; num |= num >> 8;
  num |= num >> 16;
  return ++num;
}

static inline void convertMode(const SDL_DisplayMode& mode, WsiMode* pMode) {
  pMode->width          = uint32_t(mode.w);
  pMode->height         = uint32_t(mode.h);
  pMode->refreshRate    = WsiRational{ uint32_t(mode.refresh_rate) * 1000, 1000 };
  // BPP should always be a power of two
  // to match Windows behaviour of including padding.
  pMode->bitsPerPixel   = roundToNextPow2(SDL_BITSPERPIXEL(mode.format));
  pMode->interlaced     = false;
}

}

#endif // defined(DXMT_WSI_SDL3)
