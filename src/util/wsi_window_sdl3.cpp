#if defined(DXMT_WSI_SDL3)

#include "wsi_window.hpp"
#include "wsi_monitor.hpp"

#include "wsi_platform_sdl3.hpp"

#include "util_string.hpp"
#include "log/log.hpp"

#include <SDL3/SDL.h>
#include <SDL3/SDL_metal.h>

namespace dxmt::wsi {

void getWindowSize(HWND hWindow, uint32_t *pWidth, uint32_t *pHeight) {
    SDL_Window* window = fromHwnd(hWindow);

    int32_t w, h;
    SDL3Initializer::get().SDL_GetWindowSize(window, &w, &h);

    if (pWidth)
      *pWidth = uint32_t(w);

    if (pHeight)
      *pHeight = uint32_t(h);
}

void resizeWindow(HWND hWindow, DXMTWindowState *pState, uint32_t width,
                  uint32_t height) {
    SDL_Window* window = fromHwnd(hWindow);

    SDL3Initializer::get().SDL_SetWindowSize(window, int32_t(width), int32_t(height));
}

bool setWindowMode(HMONITOR hMonitor, HWND hWindow, const WsiMode &mode) {
    const SDL_DisplayID displayId = fromHmonitor(hMonitor);
    SDL_Window* window = fromHwnd(hWindow);

    if (!isValidDisplay(displayId))
      return false;

    float refreshRate = mode.refreshRate.numerator != 0
      ? float(mode.refreshRate.numerator) / float(mode.refreshRate.denominator)
      : 0.0f;
    // TODO: Implement lookup format for bitsPerPixel here.

    const SDL_DisplayMode *sdlmode = SDL3Initializer::get().SDL_GetClosestFullscreenDisplayMode(displayId, mode.width, mode.height, refreshRate, true);
    if (!sdlmode) {
      Logger::err(str::format("SDL3 WSI: setWindowMode: SDL_GetClosestFullscreenDisplayMode: ", SDL3Initializer::get().SDL_GetError()));
      return false;
    }

    if (!SDL3Initializer::get().SDL_SetWindowFullscreenMode(window, sdlmode)) {
      Logger::err(str::format("SDL3 WSI: setWindowMode: SDL_SetWindowFullscreenMode: ", SDL3Initializer::get().SDL_GetError()));
      return false;
    }

    return true;
}

bool enterFullscreenMode(HMONITOR hMonitor, HWND hWindow,
                         DXMTWindowState *pState,
                         [[maybe_unused]] bool modeSwitch) {

    const SDL_DisplayID displayId = fromHmonitor(hMonitor);
    SDL_Window* window = fromHwnd(hWindow);

    if (!isValidDisplay(displayId))
      return false;

    if (!modeSwitch) {
      // borderless fullscreen: set no fullscreen mode (NULL = borderless)
      SDL3Initializer::get().SDL_SetWindowFullscreenMode(window, nullptr);
    }

    // TODO: Set this on the correct monitor.
    // Docs aren't clear on this...
    if (!SDL3Initializer::get().SDL_SetWindowFullscreen(window, true)) {
      Logger::err(str::format("SDL3 WSI: enterFullscreenMode: SDL_SetWindowFullscreen: ", SDL3Initializer::get().SDL_GetError()));
      return false;
    }

    return true;
}

bool leaveFullscreenMode(HWND hWindow, DXMTWindowState *pState,
                         bool restoreCoordinates) {
    SDL_Window* window = fromHwnd(hWindow);

    // @todo; pState handling
    (void)pState;

    if (!SDL3Initializer::get().SDL_SetWindowFullscreen(window, false)) {
      Logger::err(str::format("SDL3 WSI: leaveFullscreenMode: SDL_SetWindowFullscreen: ", SDL3Initializer::get().SDL_GetError()));
      return false;
    }

    return true;
}

bool restoreDisplayMode() {
  return true;
}

HMONITOR getWindowMonitor(HWND hWindow) {
    SDL_Window* window = fromHwnd(hWindow);
    const SDL_DisplayID displayId = SDL3Initializer::get().SDL_GetDisplayForWindow(window);

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
    SDL_MetalView sdlView = SDL3Initializer::get().SDL_Metal_CreateView((SDL_Window*)hwnd);

    if (!sdlView) {
        Logger::err(str::format("SDL3 WSI: createMetalViewFromHWND: SDL_Metal_CreateView failed with error: ", SDL3Initializer::get().SDL_GetError()));
        return WMT::Object();
    }

    void* metalLayer = SDL3Initializer::get().SDL_Metal_GetLayer(sdlView);
    if (!metalLayer) {
        Logger::err(str::format("SDL3 WSI: createMetalViewFromHWND: SDL_Metal_GetLayer failed with error: ", SDL3Initializer::get().SDL_GetError()));
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

#endif // defined(DXMT_WSI_SDL3)
