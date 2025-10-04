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

#include "wsi_window.hpp"
#include "wsi_monitor.hpp"

#include "wsi_platform_sdl2.hpp"

#include "util_string.hpp"
#include "log/log.hpp"

#include <SDL2/SDL.h>
#include <SDL2/SDL_metal.h>

namespace dxmt::wsi {

void getWindowSize(HWND hWindow, uint32_t *pWidth, uint32_t *pHeight) {
    SDL_Window* window = fromHwnd(hWindow);

    int32_t w, h;
    SDL2Initializer::get().SDL_GetWindowSize(window, &w, &h);

    if (pWidth)
      *pWidth = uint32_t(w);

    if (pHeight)
      *pHeight = uint32_t(h);
}

void resizeWindow(HWND hWindow, DXMTWindowState *pState, uint32_t width,
                  uint32_t height) {
    SDL_Window* window = fromHwnd(hWindow);

    SDL2Initializer::get().SDL_SetWindowSize(window, int32_t(width), int32_t(height));
}

bool setWindowMode(HMONITOR hMonitor, HWND hWindow, const WsiMode &mode) {
    const int32_t displayId = fromHmonitor(hMonitor);
    SDL_Window* window = fromHwnd(hWindow);

    if (!isValidDisplay(displayId))
      return false;

    SDL_DisplayMode wantedMode = { };
    wantedMode.w            = mode.width;
    wantedMode.h            = mode.height;
    wantedMode.refresh_rate = mode.refreshRate.numerator != 0
      ? mode.refreshRate.numerator / mode.refreshRate.denominator
      : 0;
    // TODO: Implement lookup format for bitsPerPixel here.

    SDL_DisplayMode sdlmode = { };
    if (SDL2Initializer::get().SDL_GetClosestDisplayMode(displayId, &wantedMode, &sdlmode) == nullptr) {
      Logger::err(str::format("SDL2 WSI: setWindowMode: SDL_GetClosestDisplayMode: ", SDL2Initializer::get().SDL_GetError()));
      return false;
    }

    if (SDL2Initializer::get().SDL_SetWindowDisplayMode(window, &sdlmode) != 0) {
      Logger::err(str::format("SDL2 WSI: setWindowMode: SDL_SetWindowDisplayMode: ", SDL2Initializer::get().SDL_GetError()));
      return false;
    }

    return true;
}

bool enterFullscreenMode(HMONITOR hMonitor, HWND hWindow,
                         DXMTWindowState *pState,
                         [[maybe_unused]] bool modeSwitch) {
                            
    const int32_t displayId = fromHmonitor(hMonitor);
    SDL_Window* window = fromHwnd(hWindow);

    if (!isValidDisplay(displayId))
      return false;

    uint32_t flags = modeSwitch
        ? SDL_WINDOW_FULLSCREEN
        : SDL_WINDOW_FULLSCREEN_DESKTOP;
    
    // TODO: Set this on the correct monitor.
    // Docs aren't clear on this...
    if (SDL2Initializer::get().SDL_SetWindowFullscreen(window, flags) != 0) {
      Logger::err(str::format("SDL2 WSI: enterFullscreenMode: SDL_SetWindowFullscreen: ", SDL2Initializer::get().SDL_GetError()));
      return false;
    }

    return true;
}

bool leaveFullscreenMode(HWND hWindow, DXMTWindowState *pState,
                         bool restoreCoordinates) {
    SDL_Window* window = fromHwnd(hWindow);
    
    // @todo; pState handling
    (void)pState;

    if (SDL2Initializer::get().SDL_SetWindowFullscreen(window, 0) != 0) {
      Logger::err(str::format("SDL2 WSI: leaveFullscreenMode: SDL_SetWindowFullscreen: ", SDL2Initializer::get().SDL_GetError()));
      return false;
    }

    return true;
}

bool restoreDisplayMode() {
  return true;
}

HMONITOR getWindowMonitor(HWND hWindow) {
    SDL_Window* window = fromHwnd(hWindow);
    const int32_t displayId = SDL2Initializer::get().SDL_GetWindowDisplayIndex(window);

    return toHmonitor(displayId);
}

bool isWindow(HWND hWindow) { return true; }

void updateFullscreenWindow(HMONITOR hMonitor, HWND hWindow,
                            bool forceTopmost) {
    (void)hMonitor; 
    (void)forceTopmost; 
    (void)hWindow;
}

WMT::Object createMetalViewFromHWND(intptr_t hwnd, WMT::Device device, WMT::MetalLayer &layer) {
    SDL_MetalView sdlView = SDL2Initializer::get().SDL_Metal_CreateView((SDL_Window*)hwnd);

    if (!sdlView) {
        Logger::err(str::format("SDL2 WSI: createMetalViewFromHWND: SDL_Metal_CreateView failed with error: ", SDL2Initializer::get().SDL_GetError()));
        return WMT::Object();
    }

    void* metalLayer = SDL2Initializer::get().SDL_Metal_GetLayer(sdlView);
    if (!metalLayer) {
        Logger::err(str::format("SDL2 WSI: createMetalViewFromHWND: SDL_Metal_GetLayer failed with error: ", SDL2Initializer::get().SDL_GetError()));
        return WMT::Object();
    }

    layer.handle = (obj_handle_t)((void*)metalLayer);
    WMTLayerProps props = {};
    layer.getProps(props);
    props.device = device.handle;
    layer.setProps(props);
    return WMT::Object((obj_handle_t)((void*)sdlView));
}

} // namespace dxmt::wsi

#endif // defined(DXMT_WSI_SDL2)