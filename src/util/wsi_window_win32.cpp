/*
 * This file is part of DXMT, Copyright (c) 2023 Feifan He
 *
 * Derived from a part of DXVK (originally under zlib License),
 * Copyright (c) 2017 Philip Rebohle
 * Copyright (c) 2019 Joshua Ashton
 *
 * See <https://github.com/doitsujin/dxvk/blob/master/LICENSE>
 */

#include "util_d3dkmt.h"
#include "wsi_window.hpp"
#include "wsi_monitor.hpp"

#include "util_string.hpp"
#include "log/log.hpp"

namespace dxmt::wsi {

static bool getMonitorDisplayMode(HMONITOR hMonitor, DWORD modeNum,
                                  DEVMODEW *pMode) {
  ::MONITORINFOEXW monInfo;
  monInfo.cbSize = sizeof(monInfo);

  if (!::GetMonitorInfoW(hMonitor, reinterpret_cast<MONITORINFO *>(&monInfo))) {
    Logger::err("Failed to query monitor info");
    return false;
  }

  return ::EnumDisplaySettingsW(monInfo.szDevice, modeNum, pMode);
}

static bool setMonitorDisplayMode(HMONITOR hMonitor, DEVMODEW *pMode) {
  ::MONITORINFOEXW monInfo;
  monInfo.cbSize = sizeof(monInfo);

  if (!::GetMonitorInfoW(hMonitor, reinterpret_cast<MONITORINFO *>(&monInfo))) {
    Logger::err("Failed to query monitor info");
    return false;
  }

  Logger::info(str::format("Setting display mode: ", pMode->dmPelsWidth, "x",
                           pMode->dmPelsHeight, "@",
                           pMode->dmDisplayFrequency));

  DEVMODEW curMode = {};
  curMode.dmSize = sizeof(curMode);

  if (getMonitorDisplayMode(hMonitor, ENUM_CURRENT_SETTINGS, &curMode)) {
    bool eq = curMode.dmPelsWidth == pMode->dmPelsWidth &&
              curMode.dmPelsHeight == pMode->dmPelsHeight &&
              curMode.dmBitsPerPel == pMode->dmBitsPerPel;

    if (pMode->dmFields & DM_DISPLAYFREQUENCY)
      eq &= curMode.dmDisplayFrequency == pMode->dmDisplayFrequency;

    if (eq)
      return true;
  }

  LONG status = ::ChangeDisplaySettingsExW(monInfo.szDevice, pMode, nullptr,
                                           CDS_FULLSCREEN, nullptr);

  if (status != DISP_CHANGE_SUCCESSFUL) {
    pMode->dmFields &= ~DM_DISPLAYFREQUENCY;

    status = ::ChangeDisplaySettingsExW(monInfo.szDevice, pMode, nullptr,
                                        CDS_FULLSCREEN, nullptr);
  }

  return status == DISP_CHANGE_SUCCESSFUL;
}

void getWindowSize(HWND hWindow, uint32_t *pWidth, uint32_t *pHeight) {
  RECT rect = {};
  ::GetClientRect(hWindow, &rect);

  if (pWidth)
    *pWidth = rect.right - rect.left;

  if (pHeight)
    *pHeight = rect.bottom - rect.top;
}

void resizeWindow(HWND hWindow, DXMTWindowState *pState, uint32_t width,
                  uint32_t height) {
  // Adjust window position and size
  RECT newRect = {0, 0, 0, 0};
  RECT oldRect = {0, 0, 0, 0};

  ::GetWindowRect(hWindow, &oldRect);
  ::SetRect(&newRect, 0, 0, width, height);
  ::AdjustWindowRectEx(&newRect, ::GetWindowLongW(hWindow, GWL_STYLE), FALSE,
                       ::GetWindowLongW(hWindow, GWL_EXSTYLE));
  ::SetRect(&newRect, 0, 0, newRect.right - newRect.left,
            newRect.bottom - newRect.top);
  ::OffsetRect(&newRect, oldRect.left, oldRect.top);
  ::MoveWindow(hWindow, newRect.left, newRect.top, newRect.right - newRect.left,
               newRect.bottom - newRect.top, TRUE);
}

bool setWindowMode(HMONITOR hMonitor, HWND hWindow, const WsiMode &mode) {
  ::MONITORINFOEXW monInfo;
  monInfo.cbSize = sizeof(monInfo);

  if (!::GetMonitorInfoW(hMonitor, reinterpret_cast<MONITORINFO *>(&monInfo))) {
    Logger::err("Win32 WSI: setWindowMode: Failed to query monitor info");
    return false;
  }

  DEVMODEW devMode = {};
  devMode.dmSize = sizeof(devMode);
  devMode.dmFields = DM_PELSWIDTH | DM_PELSHEIGHT | DM_BITSPERPEL;
  devMode.dmPelsWidth = mode.width;
  devMode.dmPelsHeight = mode.height;
  devMode.dmBitsPerPel = mode.bitsPerPixel;

  if (mode.refreshRate.numerator != 0) {
    devMode.dmFields |= DM_DISPLAYFREQUENCY;
    devMode.dmDisplayFrequency =
        mode.refreshRate.numerator / mode.refreshRate.denominator;
  }

  return setMonitorDisplayMode(hMonitor, &devMode);
}

bool enterFullscreenMode(HMONITOR hMonitor, HWND hWindow,
                         DXMTWindowState *pState,
                         [[maybe_unused]] bool modeSwitch) {
  RECT rect = {};
  getDesktopCoordinates(hMonitor, &rect);

  D3DKMT_ESCAPE escape = {};
  escape.Type = D3DKMT_ESCAPE_SET_PRESENT_RECT_WINE;
  escape.pPrivateDriverData = &rect;
  escape.PrivateDriverDataSize = sizeof(rect);
  escape.hContext = HandleToUlong(hWindow);
  D3DKMTEscape(&escape);

  // Find a display mode that matches what we need
  ::GetWindowRect(hWindow, &pState->rect);

  // Change the window flags to remove the decoration etc.
  LONG style = ::GetWindowLongW(hWindow, GWL_STYLE);
  LONG exstyle = ::GetWindowLongW(hWindow, GWL_EXSTYLE);

  pState->style = style;
  pState->exstyle = exstyle;

  style &= ~WS_OVERLAPPEDWINDOW;
  exstyle &= ~WS_EX_OVERLAPPEDWINDOW;

  ::SetWindowLongW(hWindow, GWL_STYLE, style);
  ::SetWindowLongW(hWindow, GWL_EXSTYLE, exstyle);

  ::SetWindowPos(hWindow, HWND_TOPMOST, rect.left, rect.top,
                 rect.right - rect.left, rect.bottom - rect.top,
                 SWP_FRAMECHANGED | SWP_SHOWWINDOW | SWP_NOACTIVATE);

  return true;
}

bool leaveFullscreenMode(HWND hWindow, DXMTWindowState *pState,
                         bool restoreCoordinates) {
  // Only restore the window style if the application hasn't
  // changed them. This is in line with what native DXGI does.
  LONG curStyle = ::GetWindowLongW(hWindow, GWL_STYLE) & ~WS_VISIBLE;
  LONG curExstyle = ::GetWindowLongW(hWindow, GWL_EXSTYLE) & ~WS_EX_TOPMOST;

  if (curStyle == (pState->style & ~(WS_VISIBLE | WS_OVERLAPPEDWINDOW)) &&
      curExstyle ==
          (pState->exstyle & ~(WS_EX_TOPMOST | WS_EX_OVERLAPPEDWINDOW))) {
    ::SetWindowLongW(hWindow, GWL_STYLE, pState->style);
    ::SetWindowLongW(hWindow, GWL_EXSTYLE, pState->exstyle);
  }

  RECT empty = {};
  D3DKMT_ESCAPE escape = {};
  escape.Type = D3DKMT_ESCAPE_SET_PRESENT_RECT_WINE;
  escape.pPrivateDriverData = &empty;
  escape.PrivateDriverDataSize = sizeof(empty);
  escape.hContext = HandleToUlong(hWindow);
  D3DKMTEscape(&escape);

  // Restore window position and apply the style
  UINT flags = SWP_FRAMECHANGED | SWP_NOACTIVATE;
  const RECT rect = pState->rect;

  if (!restoreCoordinates)
    flags |= SWP_NOSIZE | SWP_NOMOVE;

  ::SetWindowPos(hWindow,
                 (pState->exstyle & WS_EX_TOPMOST) ? HWND_TOPMOST
                                                   : HWND_NOTOPMOST,
                 rect.left, rect.top, rect.right - rect.left,
                 rect.bottom - rect.top, flags);

  return true;
}

bool restoreDisplayMode(HMONITOR hMonitor) {
  WCHAR device_name[32];
  DEVMODEW current_mode, registry_mode;
  getDisplayName(hMonitor, device_name);
  if (!EnumDisplaySettingsExW(device_name, ENUM_CURRENT_SETTINGS, &current_mode, 0))
    return false;
  if (!EnumDisplaySettingsExW(device_name, ENUM_REGISTRY_SETTINGS, &registry_mode, 0))
    return false;
  do {
    if (current_mode.dmPelsWidth != registry_mode.dmPelsWidth)
      break;
    if (current_mode.dmPelsHeight != registry_mode.dmPelsHeight)
      break;
    if (current_mode.dmBitsPerPel != registry_mode.dmBitsPerPel)
      break;
    if ((current_mode.dmFields & registry_mode.dmFields & DM_DISPLAYFREQUENCY) &&
        current_mode.dmDisplayFrequency != registry_mode.dmDisplayFrequency)
      break;
    return true;
  } while (0);
  LONG ret = ChangeDisplaySettingsExW(device_name, NULL, NULL, 0, NULL);
  return ret == DISP_CHANGE_SUCCESSFUL;
}

HMONITOR getWindowMonitor(HWND hWindow) {
  RECT windowRect = {0, 0, 0, 0};
  ::GetWindowRect(hWindow, &windowRect);

  HMONITOR monitor =
      ::MonitorFromPoint({(windowRect.left + windowRect.right) / 2,
                          (windowRect.top + windowRect.bottom) / 2},
                         MONITOR_DEFAULTTOPRIMARY);

  return monitor;
}

bool isWindow(HWND hWindow) { return ::IsWindow(hWindow); }

void updateFullscreenWindow(HMONITOR hMonitor, HWND hWindow,
                            bool forceTopmost) {
  RECT bounds = {};
  wsi::getDesktopCoordinates(hMonitor, &bounds);

  D3DKMT_ESCAPE escape = {};
  escape.Type = D3DKMT_ESCAPE_SET_PRESENT_RECT_WINE;
  escape.pPrivateDriverData = &bounds;
  escape.PrivateDriverDataSize = sizeof(bounds);
  escape.hContext = HandleToUlong(hWindow);
  D3DKMTEscape(&escape);

  // In D3D9, changing display modes re-forces the window
  // to become top most, whereas in DXGI, it does not.
  if (forceTopmost) {
    ::SetWindowPos(hWindow, HWND_TOPMOST, bounds.left, bounds.top,
                   bounds.right - bounds.left, bounds.bottom - bounds.top,
                   SWP_FRAMECHANGED | SWP_SHOWWINDOW | SWP_NOACTIVATE);
  } else {
    ::MoveWindow(hWindow, bounds.left, bounds.top, bounds.right - bounds.left,
                 bounds.bottom - bounds.top, TRUE);
  }
}

bool isForeground(HWND hWindow) {
  return ::GetForegroundWindow() == hWindow;
}

bool isMinimized(HWND hWindow) {
  return ::IsIconic(hWindow);
}

} // namespace dxmt::wsi
