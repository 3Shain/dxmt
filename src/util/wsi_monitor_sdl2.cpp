/*
 * This file is part of DXMT, Copyright (c) 2023 Feifan He
 *
 * Derived from a part of DXVK (originally under zlib License),
 * Copyright (c) 2017 Philip Rebohle
 * Copyright (c) 2019 Joshua Ashton
 *
 * See <https://github.com/doitsujin/dxvk/blob/master/LICENSE>
 */

#if defined(DXMT_WSI_SDL2)

#include "wsi_monitor.hpp"
#include "wsi_platform_sdl2.hpp"

#include "log/log.hpp"
#include "util_string.hpp"

#include <SDL2/SDL.h>

#include <cstring>

namespace dxmt::wsi {

HMONITOR enumMonitors(uint32_t index) {
    if (!isValidDisplay(index)) {
        Logger::err(str::format("SDL2 WSI: Failed to get display for index ", index));
        return nullptr;
    }
    return toHmonitor((int32_t)index);
}

HMONITOR getDefaultMonitor() {
    return enumMonitors(0);
}

bool getDisplayName(HMONITOR hMonitor, WCHAR (&Name)[32]) {
    int32_t display_id = fromHmonitor(hMonitor);
    if (!isValidDisplay(display_id)) {
        Logger::err("SDL2 WSI: Invalid display ID for monitor");
        return false;
    }
    
    std::wstringstream display_name;
    display_name << L"\\\\.\\DISPLAY" << display_id;
    std::wstring display_name_str = display_name.str();

    std::memset(Name, 0, sizeof(WCHAR) * 32);
    if (display_name_str.length() >= 32) {
        Logger::err("SDL2 WSI: Display name exceeds maximum length of 32 characters");
        return false;
    }
    display_name_str.copy(Name, display_name_str.length(), 0);

    return true;
}

bool getDesktopCoordinates(HMONITOR hMonitor, RECT *pRect) {
    const int32_t displayId = fromHmonitor(hMonitor);

    if (!isValidDisplay(displayId))
      return false;

    SDL_Rect rect = { };
    SDL2Initializer::get().SDL_GetDisplayBounds(displayId, &rect);

    pRect->left   = rect.x;
    pRect->top    = rect.y;
    pRect->right  = rect.x + rect.w;
    pRect->bottom = rect.y + rect.h;

    return true;
}

bool getDisplayMode(HMONITOR hMonitor, uint32_t modeNumber, WsiMode *pMode) {
    const int32_t displayId = fromHmonitor(hMonitor);

    if (!isValidDisplay(displayId))
      return false;

    SDL_DisplayMode mode = { };
    if (SDL2Initializer::get().SDL_GetDisplayMode(displayId, modeNumber, &mode) != 0)
      return false;

    pMode->width          = uint32_t(mode.w);
    pMode->height         = uint32_t(mode.h);
    pMode->refreshRate    = WsiRational{ uint32_t(mode.refresh_rate) * 1000, 1000 };
    pMode->bitsPerPixel   = roundToNextPow2(SDL_BITSPERPIXEL(mode.format));
    pMode->interlaced     = false;

    return true;
}

bool getCurrentDisplayMode(HMONITOR hMonitor, WsiMode *pMode) {
    const int32_t displayId = fromHmonitor(hMonitor);

    if (!isValidDisplay(displayId))
      return false;

    SDL_DisplayMode mode = { };
    if (SDL2Initializer::get().SDL_GetCurrentDisplayMode(displayId, &mode) != 0) {
      Logger::err(str::format("SDL_GetCurrentDisplayMode: ", SDL2Initializer::get().SDL_GetError()));
      return false;
    }

    convertMode(mode, pMode);

    return true;
}

bool getDesktopDisplayMode(HMONITOR hMonitor, WsiMode *pMode) {
    const int32_t displayId = fromHmonitor(hMonitor);

    if (!isValidDisplay(displayId))
      return false;

    SDL_DisplayMode mode = { };
    if (SDL2Initializer::get().SDL_GetDesktopDisplayMode(displayId, &mode) != 0) {
      Logger::err(str::format("SDL_GetCurrentDisplayMode: ", SDL2Initializer::get().SDL_GetError()));
      return false;
    }

    convertMode(mode, pMode);

    return true;
}

} // namespace dxmt::wsi

#endif // defined(DXMT_WSI_SDL2)