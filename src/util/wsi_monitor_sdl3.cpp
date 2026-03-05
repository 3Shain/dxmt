#if defined(DXMT_WSI_SDL3)

#include "wsi_monitor.hpp"
#include "wsi_platform_sdl3.hpp"

#include "log/log.hpp"
#include "util_string.hpp"

#include <SDL3/SDL.h>

#include <cstring>

namespace dxmt::wsi {

HMONITOR enumMonitors(uint32_t index) {
    int count = 0;
    SDL_DisplayID *displays = SDL3Initializer::get().SDL_GetDisplays(&count);
    if (!displays || int(index) >= count) {
        Logger::err(str::format("SDL3 WSI: Failed to get display for index ", index));
        if (displays)
          SDL3Initializer::get().SDL_free(displays);
        return nullptr;
    }
    SDL_DisplayID displayId = displays[index];
    SDL3Initializer::get().SDL_free(displays);
    return toHmonitor(displayId);
}

HMONITOR getDefaultMonitor() {
    return enumMonitors(0);
}

bool getDisplayName(HMONITOR hMonitor, WCHAR (&Name)[32]) {
    SDL_DisplayID displayId = fromHmonitor(hMonitor);
    if (!isValidDisplay(displayId)) {
        Logger::err("SDL3 WSI: Invalid display ID for monitor");
        return false;
    }

    std::wstringstream display_name;
    display_name << L"\\\\.\\DISPLAY" << displayId;
    std::wstring display_name_str = display_name.str();

    std::memset(Name, 0, sizeof(WCHAR) * 32);
    if (display_name_str.length() >= 32) {
        Logger::err("SDL3 WSI: Display name exceeds maximum length of 32 characters");
        return false;
    }
    display_name_str.copy(Name, display_name_str.length(), 0);

    return true;
}

bool getDesktopCoordinates(HMONITOR hMonitor, RECT *pRect) {
    const SDL_DisplayID displayId = fromHmonitor(hMonitor);

    if (!isValidDisplay(displayId))
      return false;

    SDL_Rect rect = { };
    SDL3Initializer::get().SDL_GetDisplayBounds(displayId, &rect);

    pRect->left   = rect.x;
    pRect->top    = rect.y;
    pRect->right  = rect.x + rect.w;
    pRect->bottom = rect.y + rect.h;

    return true;
}

bool getDisplayMode(HMONITOR hMonitor, uint32_t modeNumber, WsiMode *pMode) {
    const SDL_DisplayID displayId = fromHmonitor(hMonitor);

    if (!isValidDisplay(displayId))
      return false;

    int count = 0;
    SDL_DisplayMode **modes = SDL3Initializer::get().SDL_GetFullscreenDisplayModes(displayId, &count);
    if (!modes || int(modeNumber) >= count) {
      if (modes)
        SDL3Initializer::get().SDL_free(modes);
      return false;
    }

    const SDL_DisplayMode *mode = modes[modeNumber];
    pMode->width          = uint32_t(mode->w);
    pMode->height         = uint32_t(mode->h);
    pMode->refreshRate    = WsiRational{ uint32_t(mode->refresh_rate) * 1000, 1000 };
    pMode->bitsPerPixel   = roundToNextPow2(SDL_BITSPERPIXEL(mode->format));
    pMode->interlaced     = false;

    SDL3Initializer::get().SDL_free(modes);

    return true;
}

bool getCurrentDisplayMode(HMONITOR hMonitor, WsiMode *pMode) {
    const SDL_DisplayID displayId = fromHmonitor(hMonitor);

    if (!isValidDisplay(displayId))
      return false;

    const SDL_DisplayMode *mode = SDL3Initializer::get().SDL_GetCurrentDisplayMode(displayId);
    if (!mode) {
      Logger::err(str::format("SDL_GetCurrentDisplayMode: ", SDL3Initializer::get().SDL_GetError()));
      return false;
    }

    convertMode(*mode, pMode);

    return true;
}

bool getDesktopDisplayMode(HMONITOR hMonitor, WsiMode *pMode) {
    const SDL_DisplayID displayId = fromHmonitor(hMonitor);

    if (!isValidDisplay(displayId))
      return false;

    const SDL_DisplayMode *mode = SDL3Initializer::get().SDL_GetDesktopDisplayMode(displayId);
    if (!mode) {
      Logger::err(str::format("SDL_GetDesktopDisplayMode: ", SDL3Initializer::get().SDL_GetError()));
      return false;
    }

    convertMode(*mode, pMode);

    return true;
}

} // namespace dxmt::wsi

#endif // defined(DXMT_WSI_SDL3)
