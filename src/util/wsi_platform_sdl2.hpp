#pragma once

#if defined(DXMT_WSI_SDL2)

#include "wsi_monitor.hpp"

#include <SDL2/SDL.h>

namespace dxmt::wsi {

class SDL2Initializer { // using this to force loadlibrary sdl2 at startup for now
    public:
void* libsdl;
#define SDL_PROC(ret, name, params) \
  typedef ret (SDLCALL *pfn_##name) params; \
  pfn_##name name;
#include "wsi_platform_sdl2_funcs.h"

    SDL2Initializer();

    static SDL2Initializer get() {
        static SDL2Initializer instance;
        return instance;
    }
};

static inline int32_t fromHmonitor(HMONITOR hMonitor) {
  return static_cast<int32_t>(reinterpret_cast<intptr_t>(hMonitor)) - 1;
}

static inline HMONITOR toHmonitor(int32_t displayId) {
  return reinterpret_cast<HMONITOR>(static_cast<intptr_t>(displayId + 1));
}

static inline bool isValidDisplay(int32_t display_id) {
    const int32_t display_cnt = SDL2Initializer::get().SDL_GetNumVideoDisplays();
    return display_id >= 0 && display_id < display_cnt;
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

#endif // defined(DXMT_WSI_SDL2)